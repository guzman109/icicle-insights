#include "core/http.hpp"
#include "core/result.hpp"
#include "db/db.hpp"
#include "git/models.hpp"
#include "git/router.hpp"
#include "glaze/net/http_router.hpp"
#include "server/dependencies.hpp"
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace insights::git {

std::expected<void, core::Error>
registerReposRoutes(glz::http_router &Router,
                    std::shared_ptr<db::Database> &Database) {

  // Repos
  // Get All Repos

  using enum core::HttpStatus;

  Router.get("/repos",
             [Database](const glz::request &Request, glz::response &Response) {
               spdlog::debug("GET /repos - Fetching all repositories");
               auto DbResponse = Database->getAll<git::models::Repository>();

               if (!DbResponse) {
                 spdlog::error("GET /repos - Database error: {}",
                               DbResponse.error().Message);
                 Response.status(static_cast<int>(InternalServerError))
                     .json({{"error", DbResponse.error().Message}});
                 return;
               }

               spdlog::debug("GET /repos - Retrieved {} repositories",
                             DbResponse.value().size());
               std::vector<git::OutputRepositorySchema> Output;
               for (const auto &Repository : DbResponse.value()) {
                 Output.emplace_back(OutputRepositorySchema{
                     .Id = Repository.Id,
                     .Name = Repository.Name,
                     .AccountId = Repository.AccountId,
                     .Clones = Repository.Clones,
                     .Forks = Repository.Forks,
                     .Stars = Repository.Stars,
                     .Views = Repository.Views,
                 });
               }

               Response.status(static_cast<int>(Ok)).json(Output);
             });

  // Create Repository
  Router.post("/repos", [Database](const glz::request &Request,
                                   glz::response &Response) {
    CreateRepositorySchema RepositoryData;
    if (auto JsonError = glz::read_json(RepositoryData, Request.body)) {
      spdlog::warn("POST /repos - Invalid JSON in request body");
      Response.status(static_cast<int>(BadRequest))
          .json({{"error", "Invalid JSON"}});
      return;
    }
    std::ranges::transform(RepositoryData.Name, RepositoryData.Name.begin(),
                           [](unsigned char Ch) { return std::tolower(Ch); });
    spdlog::debug("POST /repos - Creating repository '{}'",
                  RepositoryData.Name);
    git::models::Repository RepositoryToCreate{
        .Name = RepositoryData.Name,
        .AccountId = RepositoryData.AccountId,
        .Clones = RepositoryData.Clones,
        .Forks = RepositoryData.Forks,
        .Stars = RepositoryData.Stars,
        .Views = RepositoryData.Views,
    };

    auto DbResponse = Database->create(RepositoryToCreate);
    if (!DbResponse) {
      spdlog::error("POST /repos - Failed to create repository '{}': {}",
                    RepositoryData.Name, DbResponse.error().Message);
      Response.status(static_cast<int>(InternalServerError))
          .json({{"error", DbResponse.error().Message}});
      return;
    }
    spdlog::info("POST /repos - Created repository '{}' with ID: {}",
                 DbResponse.value().Name, DbResponse.value().Id);
    auto Output = OutputRepositorySchema{
        .Id = DbResponse.value().Id,
        .Name = DbResponse.value().Name,
        .AccountId = DbResponse.value().AccountId,
        .Clones = DbResponse.value().Clones,
        .Forks = DbResponse.value().Forks,
        .Stars = DbResponse.value().Stars,
        .Views = DbResponse.value().Views,
    };
    Response.status(static_cast<int>(Created)).json(Output);
  });

  Router.get("/repos/:id",
             [Database](const glz::request &Request, glz::response &Response) {
               auto Id = Request.params.at("id");
               spdlog::debug("GET /repos/{} - Fetching repository", Id);
               auto DbResponse = Database->get<git::models::Repository>(Id);
               if (!DbResponse) {
                 spdlog::error("GET /repos/{} - Database error: {}", Id,
                               DbResponse.error().Message);
                 Response.status(static_cast<int>(InternalServerError))
                     .json({{"error", DbResponse.error().Message}});
                 return;
               }
               spdlog::debug("GET /repos/{} - Found repository '{}'", Id,
                             DbResponse.value().Name);
               auto Output = OutputRepositorySchema{
                   .Id = DbResponse.value().Id,
                   .Name = DbResponse.value().Name,
                   .AccountId = DbResponse.value().AccountId,
                   .Clones = DbResponse.value().Clones,
                   .Forks = DbResponse.value().Forks,
                   .Stars = DbResponse.value().Stars,
                   .Views = DbResponse.value().Views,
               };

               Response.status(static_cast<int>(Ok)).json(Output);
             },
             {.constraints = {{"id", server::dependencies::uuidConstraint()}}});

  Router.patch(
      "/repos/:id",
      [Database](const glz::request &Request, glz::response &Response) {
        UpdateSchema RepositoryData;
        if (auto JsonError = glz::read_json(RepositoryData, Request.body)) {
          spdlog::warn("PATCH /repos/:id - Invalid JSON in request body");
          Response.status(static_cast<int>(BadRequest))
              .json({{"error", "Invalid JSON"}});
          return;
        }

        auto Id = Request.params.at("id");
        spdlog::debug("PATCH /repos/{} - Updating repository", Id);
        auto DbResponse = Database->get<git::models::Repository>(Id);
        if (!DbResponse) {
          spdlog::error("PATCH /repos/{} - Database error: {}", Id,
                        DbResponse.error().Message);
          Response.status(static_cast<int>(InternalServerError))
              .json({{"error", DbResponse.error().Message}});
          return;
        }

        // Update values for platform.
        auto Repository = DbResponse.value();
        Repository.Clones = RepositoryData.Clones;
        Repository.Forks = RepositoryData.Forks;
        Repository.Stars = RepositoryData.Stars;
        Repository.Views = RepositoryData.Views;

        // Commit changes
        DbResponse = Database->update(Repository);
        if (!DbResponse) {
          spdlog::error("PATCH /repos/{} - Failed to update: {}", Id,
                        DbResponse.error().Message);
          Response.status(static_cast<int>(InternalServerError))
              .json({{"error", DbResponse.error().Message}});
          return;
        }

        spdlog::info("PATCH /repos/{} - Updated repository '{}'", Id,
                     DbResponse.value().Name);
        auto Output = OutputRepositorySchema{
            .Id = DbResponse.value().Id,
            .Name = DbResponse.value().Name,
            .AccountId = DbResponse.value().AccountId,
            .Clones = DbResponse.value().Clones,
            .Forks = DbResponse.value().Forks,
            .Stars = DbResponse.value().Stars,
            .Views = DbResponse.value().Views,
        };
        Response.status(static_cast<int>(Ok)).json(Output);
      },
      {.constraints = {{"id", server::dependencies::uuidConstraint()}}});

  Router.del("/repos/:id",
             [Database](const glz::request &Request, glz::response &Response) {
               auto Id = Request.params.at("id");
               spdlog::debug("DELETE /repos/{} - Soft deleting repository", Id);
               auto DbResponse = Database->remove<git::models::Repository>(Id);
               if (!DbResponse) {
                 spdlog::error("DELETE /repos/{} - Database error: {}", Id,
                               DbResponse.error().Message);
                 Response.status(static_cast<int>(InternalServerError))
                     .json({{"error", DbResponse.error().Message}});
                 return;
               }

               spdlog::info("DELETE /repos/{} - Deleted repository '{}'", Id,
                            DbResponse.value().Name);
               auto Output = OutputRepositorySchema{
                   .Id = DbResponse.value().Id,
                   .Name = DbResponse.value().Name,
                   .AccountId = DbResponse.value().AccountId,
                   .Clones = DbResponse.value().Clones,
                   .Forks = DbResponse.value().Forks,
                   .Stars = DbResponse.value().Stars,
                   .Views = DbResponse.value().Views,
               };
               Response.status(static_cast<int>(Ok)).json(Output);
             },
             {.constraints = {{"id", server::dependencies::uuidConstraint()}}});

  return {};
}
} // namespace insights::git
