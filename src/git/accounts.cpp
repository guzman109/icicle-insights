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
registerAccountsRoutes(glz::http_router &Router,
                       std::shared_ptr<db::Database> &Database) {

  // Accounts
  // Get All Accounts

  using enum core::HttpStatus;

  Router.get("/accounts",
             [Database](const glz::request &Request, glz::response &Response) {
               spdlog::debug("GET /accounts - Fetching all accounts");
               auto DbResponse = Database->getAll<git::models::Account>();

               if (!DbResponse) {
                 spdlog::error("GET /accounts - Database error: {}", DbResponse.error().Message);
                 Response.status(static_cast<int>(InternalServerError))
                     .json({{"error", DbResponse.error().Message}});
                 return;
               }

               spdlog::debug("GET /accounts - Retrieved {} accounts", DbResponse.value().size());
               std::vector<git::OutputAccountSchema> Output;
               for (const auto &Account : DbResponse.value()) {
                 Output.emplace_back(OutputAccountSchema{
                     .Id = Account.Id,
                     .Name = Account.Name,
                     .PlatformId = Account.PlatformId,
                     .Clones = Account.Clones,
                     .Followers = Account.Followers,
                     .Forks = Account.Forks,
                     .Stars = Account.Stars,
                     .Views = Account.Views,
                     .Watchers = Account.Watchers,
                 });
               }

               Response.status(static_cast<int>(Ok)).json(Output);
             });

  // Create Account
  Router.post("/accounts", [Database](const glz::request &Request,
                                      glz::response &Response) {
    CreateAccountSchema AccountData;
    if (auto JsonError = glz::read_json(AccountData, Request.body)) {
      spdlog::warn("POST /accounts - Invalid JSON in request body");
      Response.status(static_cast<int>(BadRequest))
          .json({{"error", "Invalid JSON"}});
      return;
    }
    std::ranges::transform(AccountData.Name, AccountData.Name.begin(),
                           [](unsigned char Ch) { return std::tolower(Ch); });
    spdlog::debug("POST /accounts - Creating account '{}'", AccountData.Name);
    git::models::Account AccountToCreate{.Name = AccountData.Name,
                                         .PlatformId = AccountData.PlatformId,
                                         .Clones = AccountData.Clones,
                                         .Followers = AccountData.Followers,
                                         .Forks = AccountData.Forks,
                                         .Stars = AccountData.Stars,
                                         .Views = AccountData.Views,
                                         .Watchers = AccountData.Watchers};

    auto DbResponse = Database->create(AccountToCreate);
    if (!DbResponse) {
      spdlog::error("POST /accounts - Failed to create account '{}': {}",
                    AccountData.Name, DbResponse.error().Message);
      Response.status(static_cast<int>(InternalServerError))
          .json({{"error", DbResponse.error().Message}});
      return;
    }
    spdlog::info("POST /accounts - Created account '{}' with ID: {}",
                 DbResponse.value().Name, DbResponse.value().Id);
    auto Output =
        OutputAccountSchema{.Id = DbResponse.value().Id,
                            .Name = DbResponse.value().Name,
                            .PlatformId = DbResponse.value().PlatformId,
                            .Clones = DbResponse.value().Clones,
                            .Followers = DbResponse.value().Followers,
                            .Forks = DbResponse.value().Forks,
                            .Stars = DbResponse.value().Stars,
                            .Views = DbResponse.value().Views,
                            .Watchers = DbResponse.value().Watchers};
    Response.status(static_cast<int>(Created)).json(Output);
  });

  Router.get("/accounts/:id",
             [Database](const glz::request &Request, glz::response &Response) {
               auto Id = Request.params.at("id");
               spdlog::debug("GET /accounts/{} - Fetching account", Id);
               auto DbResponse = Database->get<git::models::Account>(Id);
               if (!DbResponse) {
                 spdlog::error("GET /accounts/{} - Database error: {}", Id, DbResponse.error().Message);
                 Response.status(static_cast<int>(InternalServerError))
                     .json({{"error", DbResponse.error().Message}});
                 return;
               }
               spdlog::debug("GET /accounts/{} - Found account '{}'", Id, DbResponse.value().Name);
               auto Output = OutputAccountSchema{
                   .Id = DbResponse.value().Id,
                   .Name = DbResponse.value().Name,
                   .PlatformId = DbResponse.value().PlatformId,
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
      "/accounts/:id",
      [Database](const glz::request &Request, glz::response &Response) {
        UpdateSchema AccountData;
        if (auto JsonError = glz::read_json(AccountData, Request.body)) {
          spdlog::warn("PATCH /accounts/:id - Invalid JSON in request body");
          Response.status(static_cast<int>(BadRequest))
              .json({{"error", "Invalid JSON"}});
          return;
        }

        auto Id = Request.params.at("id");
        spdlog::debug("PATCH /accounts/{} - Updating account", Id);
        auto DbResponse = Database->get<git::models::Account>(Id);
        if (!DbResponse) {
          spdlog::error("PATCH /accounts/{} - Database error: {}", Id, DbResponse.error().Message);
          Response.status(static_cast<int>(InternalServerError))
              .json({{"error", DbResponse.error().Message}});
          return;
        }

        // Update values for platform.
        auto Account = DbResponse.value();
        Account.Clones = AccountData.Clones;
        Account.Followers = AccountData.Followers;
        Account.Forks = AccountData.Forks;
        Account.Stars = AccountData.Stars;
        Account.Views = AccountData.Views;
        Account.Watchers = AccountData.Watchers;

        // Commit changes
        DbResponse = Database->update(Account);
        if (!DbResponse) {
          spdlog::error("PATCH /accounts/{} - Failed to update: {}", Id, DbResponse.error().Message);
          Response.status(static_cast<int>(InternalServerError))
              .json({{"error", DbResponse.error().Message}});
          return;
        }

        spdlog::info("PATCH /accounts/{} - Updated account '{}'", Id, DbResponse.value().Name);
        auto Output =
            OutputAccountSchema{.Id = DbResponse.value().Id,
                                .Name = DbResponse.value().Name,
                                .PlatformId = DbResponse.value().PlatformId,
                                .Clones = DbResponse.value().Clones,
                                .Followers = DbResponse.value().Followers,
                                .Forks = DbResponse.value().Forks,
                                .Stars = DbResponse.value().Stars,
                                .Views = DbResponse.value().Views,
                                .Watchers = DbResponse.value().Watchers};
        Response.status(static_cast<int>(Ok)).json(Output);
      },
      {.constraints = {{"id", server::dependencies::uuidConstraint()}}});

  Router.del("/accounts/:id",
             [Database](const glz::request &Request, glz::response &Response) {
               auto Id = Request.params.at("id");
               spdlog::debug("DELETE /accounts/{} - Soft deleting account", Id);
               auto DbResponse = Database->remove<git::models::Account>(Id);
               if (!DbResponse) {
                 spdlog::error("DELETE /accounts/{} - Database error: {}", Id, DbResponse.error().Message);
                 Response.status(static_cast<int>(InternalServerError))
                     .json({{"error", DbResponse.error().Message}});
                 return;
               }

               spdlog::info("DELETE /accounts/{} - Deleted account '{}'", Id, DbResponse.value().Name);
               auto Output = OutputAccountSchema{
                   .Id = DbResponse.value().Id,
                   .Name = DbResponse.value().Name,
                   .PlatformId = DbResponse.value().PlatformId,
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
