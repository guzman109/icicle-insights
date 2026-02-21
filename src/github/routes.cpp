#include "insights/github/routes.hpp"

#include "glaze/net/http_router.hpp"
#include "insights/core/http.hpp"
#include "insights/core/result.hpp"
#include "insights/db/db.hpp"
#include "insights/github/models.hpp"
#include "insights/server/dependencies.hpp"

#include <algorithm>
#include <cctype>
#include <glaze/core/read.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace insights::github {

auto registerRoutes(
    glz::http_router &Router, std::shared_ptr<db::Database> &Database
) -> std::expected<void, core::Error> {
  using enum core::HttpStatus;
  spdlog::debug("Registering github accounts routes");
  // Get All Accounts
  Router.get(
      "/accounts",
      [Database](const glz::request &Request, glz::response &Response) {
        spdlog::debug("GET /accounts - Fetching all accounts");
        auto Result = Database->getAll<github::models::Account>();

        if (!Result) {
          spdlog::error(
              "GET /accounts - Database error: {}", Result.error().Message
          );
          Response.status(static_cast<int>(InternalServerError))
              .json({{"error", Result.error().Message}});
          return;
        }

        spdlog::debug("GET /accounts - Retrieved {} accounts", Result->size());
        std::vector<github::OutputAccountSchema> Output;
        for (const auto &Account : *Result) {
          Output.emplace_back(
              OutputAccountSchema{
                  .Id = Account.Id,
                  .Name = Account.Name,
                  .Followers = Account.Followers,
              }
          );
        }

        Response.status(static_cast<int>(Ok)).json(Output);
      }
  );

  // Create Account
  Router.post(
      "/accounts",
      [Database](const glz::request &Request, glz::response &Response) {
        // Validate Payload
        CreateAccountSchema AccountData;
        if (auto JsonError =
                glz::read<core::JsonOpts>(AccountData, Request.body)) {
          spdlog::warn("POST /accounts - Invalid JSON in request body");
          Response.status(static_cast<int>(BadRequest))
              .json({{"error", "Invalid JSON"}});
          return;
        }

        // Make sure all accounts are lowercase.
        std::ranges::transform(
            AccountData.Name, AccountData.Name.begin(), [](unsigned char Ch) {
              return std::tolower(Ch);
            }
        );

        spdlog::debug(
            "POST /accounts - Creating account '{}'", AccountData.Name
        );
        github::models::Account AccountToCreate{
            .Name = AccountData.Name,
            .Followers = AccountData.Followers.value_or(0),
        };

        auto Result = Database->create(AccountToCreate);
        if (!Result) {
          spdlog::error(
              "POST /accounts - Failed to create account '{}': {}",
              AccountData.Name,
              Result.error().Message
          );
          Response.status(static_cast<int>(InternalServerError))
              .json({{"error", Result.error().Message}});
          return;
        }
        spdlog::info(
            "POST /accounts - Created account '{}' with ID: {}",
            Result->Name,
            Result->Id
        );
        auto Output = OutputAccountSchema{
            .Id = Result->Id,
            .Name = Result->Name,
            .Followers = Result->Followers,
        };
        Response.status(static_cast<int>(Created)).json(Output);
      }
  );

  // Get Account by ID
  Router.get(
      "/accounts/:id",
      [Database](const glz::request &Request, glz::response &Response) {
        auto Id = Request.params.at("id");
        spdlog::debug("GET /accounts/{} - Fetching account", Id);
        auto Result = Database->get<github::models::Account>(Id);
        if (!Result) {
          spdlog::error(
              "GET /accounts/{} - Database error: {}",
              Id,
              Result.error().Message
          );
          Response.status(static_cast<int>(InternalServerError))
              .json({{"error", Result.error().Message}});
          return;
        }
        spdlog::debug(
            "GET /accounts/{} - Found account '{}'", Id, Result->Name
        );
        auto Output = OutputAccountSchema{
            .Id = Result->Id,
            .Name = Result->Name,
            .Followers = Result->Followers,
        };
        Response.status(static_cast<int>(Ok)).json(Output);
      },
      {.constraints = {{"id", server::dependencies::uuidConstraint()}}}
  );

  // Soft Delete Account
  Router.del(
      "/accounts/:id",
      [Database](const glz::request &Request, glz::response &Response) {
        auto Id = Request.params.at("id");
        spdlog::debug("DELETE /accounts/{} - Soft deleting account", Id);
        auto Result = Database->remove<github::models::Account>(Id);
        if (!Result) {
          spdlog::error(
              "DELETE /accounts/{} - Database error: {}",
              Id,
              Result.error().Message
          );
          Response.status(static_cast<int>(InternalServerError))
              .json({{"error", Result.error().Message}});
          return;
        }

        spdlog::info(
            "DELETE /accounts/{} - Deleted account '{}'", Id, Result->Name
        );
        auto Output = OutputAccountSchema{
            .Id = Result->Id,
            .Name = Result->Name,
            .Followers = Result->Followers,
        };
        Response.status(static_cast<int>(Ok)).json(Output);
      },
      {.constraints = {{"id", server::dependencies::uuidConstraint()}}}
  );
  spdlog::debug("Registering github repos routes");
  // Get All Repos
  Router.get(
      "/repos",
      [Database](const glz::request &Request, glz::response &Response) {
        spdlog::debug("GET /repos - Fetching all repositories");
        auto Result = Database->getAll<github::models::Repository>();

        if (!Result) {
          spdlog::error(
              "GET /repos - Database error: {}", Result.error().Message
          );
          Response.status(static_cast<int>(InternalServerError))
              .json({{"error", Result.error().Message}});
          return;
        }

        spdlog::debug("GET /repos - Retrieved {} repositories", Result->size());
        std::vector<github::OutputRepositorySchema> Output;
        for (const auto &Repository : *Result) {
          Output.emplace_back(
              OutputRepositorySchema{
                  .Id = Repository.Id,
                  .Name = Repository.Name,
                  .AccountId = Repository.AccountId,
                  .Clones = Repository.Clones,
                  .Forks = Repository.Forks,
                  .Stars = Repository.Stars,
                  .Subscribers = Repository.Subscribers,
                  .Views = Repository.Views,
              }
          );
        }

        Response.status(static_cast<int>(Ok)).json(Output);
      }
  );

  // Create Repo
  Router.post(
      "/repos",
      [Database](const glz::request &Request, glz::response &Response) {
        CreateRepositorySchema RepositoryData;

        if (auto JsonError =
                glz::read<core::JsonOpts>(RepositoryData, Request.body)) {
          spdlog::warn("POST /repos - Invalid JSON in request body");
          Response.status(static_cast<int>(BadRequest))
              .json({{"error", "Invalid JSON"}});
          return;
        }

        std::ranges::transform(
            RepositoryData.Name,
            RepositoryData.Name.begin(),
            [](unsigned char Ch) { return std::tolower(Ch); }
        );
        spdlog::debug(
            "POST /repos - Creating repository '{}'", RepositoryData.Name
        );
        github::models::Repository RepositoryToCreate{
            .Name = RepositoryData.Name,
            .AccountId = RepositoryData.AccountId,
            .Clones = RepositoryData.Clones.value_or(0),
            .Forks = RepositoryData.Forks.value_or(0),
            .Stars = RepositoryData.Stars.value_or(0),
            .Subscribers = RepositoryData.Subscribers.value_or(0),
            .Views = RepositoryData.Views.value_or(0),
        };

        auto Result = Database->create(RepositoryToCreate);
        if (!Result) {
          spdlog::error(
              "POST /repos - Failed to create repository '{}': {}",
              RepositoryData.Name,
              Result.error().Message
          );
          Response.status(static_cast<int>(InternalServerError))
              .json({{"error", Result.error().Message}});
          return;
        }

        spdlog::info(
            "POST /repos - Created repository '{}' with ID: {}",
            Result->Name,
            Result->Id
        );
        auto Output = OutputRepositorySchema{
            .Id = Result->Id,
            .Name = Result->Name,
            .AccountId = Result->AccountId,
            .Clones = Result->Clones,
            .Forks = Result->Forks,
            .Stars = Result->Stars,
            .Subscribers = Result->Subscribers,
            .Views = Result->Views,
        };
        Response.status(static_cast<int>(Created)).json(Output);
      }
  );

  // Get Repo by ID
  Router.get(
      "/repos/:id",
      [Database](const glz::request &Request, glz::response &Response) {
        auto Id = Request.params.at("id");
        spdlog::debug("GET /repos/{} - Fetching repository", Id);
        auto Result = Database->get<github::models::Repository>(Id);
        if (!Result) {
          spdlog::error(
              "GET /repos/{} - Database error: {}", Id, Result.error().Message
          );
          Response.status(static_cast<int>(InternalServerError))
              .json({{"error", Result.error().Message}});
          return;
        }
        spdlog::debug(
            "GET /repos/{} - Found repository '{}'", Id, Result->Name
        );
        auto Output = OutputRepositorySchema{
            .Id = Result->Id,
            .Name = Result->Name,
            .AccountId = Result->AccountId,
            .Clones = Result->Clones,
            .Forks = Result->Forks,
            .Stars = Result->Stars,
            .Views = Result->Views,
        };
        Response.status(static_cast<int>(Ok)).json(Output);
      },
      {.constraints = {{"id", server::dependencies::uuidConstraint()}}}
  );

  // Update Repo
  Router.patch(
      "/repos/:id",
      [Database](const glz::request &Request, glz::response &Response) {
        UpdateSchema RepositoryData;
        if (auto JsonError =
                glz::read<core::JsonOpts>(RepositoryData, Request.body)) {
          spdlog::warn("PATCH /repos/:id - Invalid JSON in request body");
          Response.status(static_cast<int>(BadRequest))
              .json({{"error", "Invalid JSON"}});
          return;
        }

        auto Id = Request.params.at("id");
        spdlog::debug("PATCH /repos/{} - Updating repository", Id);
        auto Result = Database->get<github::models::Repository>(Id);
        if (!Result) {
          spdlog::error(
              "PATCH /repos/{} - Database error: {}", Id, Result.error().Message
          );
          Response.status(static_cast<int>(InternalServerError))
              .json({{"error", Result.error().Message}});
          return;
        }

        Result->Clones = RepositoryData.Clones.value_or(Result->Clones);
        Result->Forks = RepositoryData.Forks.value_or(Result->Forks);
        Result->Stars = RepositoryData.Stars.value_or(Result->Stars);
        Result->Subscribers =
            RepositoryData.Subscribers.value_or(Result->Subscribers);
        Result->Views = RepositoryData.Views.value_or(Result->Views);

        Result = Database->update(*Result);
        if (!Result) {
          spdlog::error(
              "PATCH /repos/{} - Failed to update: {}",
              Id,
              Result.error().Message
          );
          Response.status(static_cast<int>(InternalServerError))
              .json({{"error", Result.error().Message}});
          return;
        }

        spdlog::info(
            "PATCH /repos/{} - Updated repository '{}'", Id, Result->Name
        );
        auto Output = OutputRepositorySchema{
            .Id = Result->Id,
            .Name = Result->Name,
            .AccountId = Result->AccountId,
            .Clones = Result->Clones,
            .Forks = Result->Forks,
            .Stars = Result->Stars,
            .Views = Result->Views,
        };
        Response.status(static_cast<int>(Ok)).json(Output);
      },
      {.constraints = {{"id", server::dependencies::uuidConstraint()}}}
  );

  // Soft Delete Repo
  Router.del(
      "/repos/:id",
      [Database](const glz::request &Request, glz::response &Response) {
        auto Id = Request.params.at("id");
        spdlog::debug("DELETE /repos/{} - Soft deleting repository", Id);
        auto Result = Database->remove<github::models::Repository>(Id);

        if (!Result) {
          spdlog::error(
              "DELETE /repos/{} - Database error: {}",
              Id,
              Result.error().Message
          );
          Response.status(static_cast<int>(InternalServerError))
              .json({{"error", Result.error().Message}});
          return;
        }

        spdlog::info(
            "DELETE /repos/{} - Deleted repository '{}'", Id, Result->Name
        );
        auto Output = OutputRepositorySchema{
            .Id = Result->Id,
            .Name = Result->Name,
            .AccountId = Result->AccountId,
            .Clones = Result->Clones,
            .Forks = Result->Forks,
            .Stars = Result->Stars,
            .Views = Result->Views,
        };
        Response.status(static_cast<int>(Ok)).json(Output);
      },
      {.constraints = {{"id", server::dependencies::uuidConstraint()}}}
  );

  spdlog::info("Successfully registered all github routes");
  return {};
}

} // namespace insights::github
