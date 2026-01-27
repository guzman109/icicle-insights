#include "core/http.hpp"
#include "core/result.hpp"
#include "db/db.hpp"
#include "git/models.hpp"
#include "git/router.hpp"
#include "glaze/net/http_router.hpp"
#include "server/dependencies.hpp"
#include <memory>
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
               // Get all instances
               auto DbResponse = Database->getAll<git::models::Account>();

               if (!DbResponse) {
                 Response.status(static_cast<int>(InternalServerError))
                     .json({"Error", "Error retrieving data from database."});
                 return;
               }

               // Structure data for output.
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
      Response.status(static_cast<int>(BadRequest))
          .json({{"error", "Invalid JSON"}});
      return;
    }
    std::ranges::transform(AccountData.Name, AccountData.Name.begin(),
                           [](unsigned char Ch) { return std::tolower(Ch); });
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
      Response.status(static_cast<int>(InternalServerError))
          .json({"Error", "Error commiting data to database."});
      return;
    }
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
               auto DbResponse = Database->get<git::models::Account>(Id);
               if (!DbResponse) {
                 Response.status(static_cast<int>(InternalServerError))
                     .json({"Error", "Error retrieving data from database."});
                 return;
               }
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
          Response.status(static_cast<int>(BadRequest))
              .json({{"error", "Invalid JSON"}});
          return;
        }

        auto Id = Request.params.at("id");
        auto DbResponse = Database->get<git::models::Account>(Id);
        if (!DbResponse) {
          Response.status(static_cast<int>(InternalServerError))
              .json({"Error", "Error retrieving data from database."});
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
          Response.status(static_cast<int>(InternalServerError))
              .json({"Error", "Error retrieving data from database."});
          return;
        }

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
               auto DbResponse = Database->remove<git::models::Account>(Id);
               if (!DbResponse) {
                 Response.status(static_cast<int>(InternalServerError))
                     .json({"Error", "Error deleting data from database."});
                 return;
               }

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
