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

core::Result<void>
registerReposRoutes(glz::http_router &Router,
                    std::shared_ptr<db::Database> &Database) {

  // Repos
  // Get All Repos

  using enum core::HttpStatus;

  Router.get("/repos",
             [Database](const glz::request &Request, glz::response &Response) {
               // Get all instances
               auto DbResponse = Database->getAll<git::models::Repository>();

               if (!DbResponse) {
                 Response.status(static_cast<int>(InternalServerError))
                     .json({"Error", "Error retrieving data from database."});
                 return;
               }

               // Structure data for output.
               std::vector<git::OutputRepositorySchema> Output;
               for (const auto &Repository : DbResponse.value()) {
                 Output.emplace_back(OutputRepositorySchema{
                     .Id = Repository.Id,
                     .Name = Repository.Name,
                     .AccountId = Repository.AccountId,
                     .Clones = Repository.Clones,
                     .Followers = Repository.Followers,
                     .Forks = Repository.Forks,
                     .Stars = Repository.Stars,
                     .Views = Repository.Views,
                     .Watchers = Repository.Watchers,
                 });
               }

               Response.status(static_cast<int>(Ok)).json(Output);
             });

  // Create Repository
  Router.post("/repos", [Database](const glz::request &Request,
                                   glz::response &Response) {
    CreateRepositorySchema RepositoryData;
    if (auto JsonError = glz::read_json(RepositoryData, Request.body)) {
      Response.status(static_cast<int>(BadRequest))
          .json({{"error", "Invalid JSON"}});
      return;
    }
    std::ranges::transform(RepositoryData.Name, RepositoryData.Name.begin(),
                           [](unsigned char Ch) { return std::tolower(Ch); });
    git::models::Repository RepositoryToCreate{
        .Name = RepositoryData.Name,
        .AccountId = RepositoryData.AccountId,
        .Clones = RepositoryData.Clones,
        .Followers = RepositoryData.Followers,
        .Forks = RepositoryData.Forks,
        .Stars = RepositoryData.Stars,
        .Views = RepositoryData.Views,
        .Watchers = RepositoryData.Watchers};

    auto DbResponse = Database->create(RepositoryToCreate);
    if (!DbResponse) {
      spdlog::error("Failed to create repository '{}': {}", RepositoryData.Name,
                    DbResponse.error().Message);
      Response.status(static_cast<int>(InternalServerError))
          .json({"Error", "Error commiting data to database."});
      return;
    }
    auto Output =
        OutputRepositorySchema{.Id = DbResponse.value().Id,
                               .Name = DbResponse.value().Name,
                               .AccountId = DbResponse.value().AccountId,
                               .Clones = DbResponse.value().Clones,
                               .Followers = DbResponse.value().Followers,
                               .Forks = DbResponse.value().Forks,
                               .Stars = DbResponse.value().Stars,
                               .Views = DbResponse.value().Views,
                               .Watchers = DbResponse.value().Watchers};
    Response.status(static_cast<int>(Created)).json(Output);
  });

  Router.get("/repos/:id",
             [Database](const glz::request &Request, glz::response &Response) {
               auto Id = Request.params.at("id");
               auto DbResponse = Database->get<git::models::Repository>(Id);
               if (!DbResponse) {
                 Response.status(static_cast<int>(InternalServerError))
                     .json({"Error", "Error retrieving data from database."});
                 return;
               }
               auto Output = OutputRepositorySchema{
                   .Id = DbResponse.value().Id,
                   .Name = DbResponse.value().Name,
                   .AccountId = DbResponse.value().AccountId,
                   .Clones = DbResponse.value().Clones,
                   .Followers = DbResponse.value().Followers,
                   .Forks = DbResponse.value().Forks,
                   .Stars = DbResponse.value().Stars,
                   .Views = DbResponse.value().Views,
                   .Watchers = DbResponse.value().Watchers};

               Response.status(static_cast<int>(Ok)).json(Output);
             },
             {.constraints = {{"id", server::dependencies::uuidConstraint()}}});

  Router.patch(
      "/repos/:id",
      [Database](const glz::request &Request, glz::response &Response) {
        UpdateSchema RepositoryData;
        if (auto JsonError = glz::read_json(RepositoryData, Request.body)) {
          Response.status(static_cast<int>(BadRequest))
              .json({{"error", "Invalid JSON"}});
          return;
        }

        auto Id = Request.params.at("id");
        auto DbResponse = Database->get<git::models::Repository>(Id);
        if (!DbResponse) {
          Response.status(static_cast<int>(InternalServerError))
              .json({"Error", "Error retrieving data from database."});
          return;
        }

        // Update values for platform.
        auto Repository = DbResponse.value();
        Repository.Clones = RepositoryData.Clones;
        Repository.Followers = RepositoryData.Followers;
        Repository.Forks = RepositoryData.Forks;
        Repository.Stars = RepositoryData.Stars;
        Repository.Views = RepositoryData.Views;
        Repository.Watchers = RepositoryData.Watchers;

        // Commit changes
        DbResponse = Database->update(Repository);
        if (!DbResponse) {
          Response.status(static_cast<int>(InternalServerError))
              .json({"Error", "Error retrieving data from database."});
          return;
        }

        auto Output =
            OutputRepositorySchema{.Id = DbResponse.value().Id,
                                   .Name = DbResponse.value().Name,
                                   .AccountId = DbResponse.value().AccountId,
                                   .Clones = DbResponse.value().Clones,
                                   .Followers = DbResponse.value().Followers,
                                   .Forks = DbResponse.value().Forks,
                                   .Stars = DbResponse.value().Stars,
                                   .Views = DbResponse.value().Views,
                                   .Watchers = DbResponse.value().Watchers};
        Response.status(static_cast<int>(Ok)).json(Output);
      },
      {.constraints = {{"id", server::dependencies::uuidConstraint()}}});

  Router.del("/repos/:id",
             [Database](const glz::request &Request, glz::response &Response) {
               auto Id = Request.params.at("id");
               auto DbResponse = Database->remove<git::models::Repository>(Id);
               if (!DbResponse) {
                 Response.status(static_cast<int>(InternalServerError))
                     .json({"Error", "Error deleting data from database."});
                 return;
               }

               auto Output = OutputRepositorySchema{
                   .Id = DbResponse.value().Id,
                   .Name = DbResponse.value().Name,
                   .AccountId = DbResponse.value().AccountId,
                   .Clones = DbResponse.value().Clones,
                   .Followers = DbResponse.value().Followers,
                   .Forks = DbResponse.value().Forks,
                   .Stars = DbResponse.value().Stars,
                   .Views = DbResponse.value().Views,
                   .Watchers = DbResponse.value().Watchers};
               Response.status(static_cast<int>(Ok)).json(Output);
             },
             {.constraints = {{"id", server::dependencies::uuidConstraint()}}});

  return {};
}
} // namespace insights::git
