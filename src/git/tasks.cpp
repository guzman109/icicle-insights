#include "insights/git/tasks.hpp"
#include "glaze/glaze.hpp"
#include "glaze/net/http_client.hpp"
#include "insights/core/result.hpp"
#include "insights/git/models.hpp"
#include "insights/git/responses.hpp"
#include <asio/ssl.hpp>
#include <expected>
#include <format>
#include <glaze/json/generic.hpp>
#include <glaze/json/lazy.hpp>
#include <glaze/json/read.hpp>
#include <glaze/net/http_router.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <string_view>
#include <system_error>

namespace insights::git::tasks {
static std::expected<std::shared_ptr<glz::http_client>, core::Error>
createClient(const core::Config &Config) {
  auto Client = std::make_shared<glz::http_client>();

  auto Configuration = Client->configure_system_ca_certificates();
  if (!Configuration) {
    spdlog::error("Error: Could not find CA certificates.");
    return std::unexpected<core::Error>(
        core::Error{.Message = "Error: Could not find CA certificates."});
  }
  return Client;
}

std::expected<void, core::Error>
updateRepositories(std::shared_ptr<glz::http_client> Client,
                   std::string_view RepoId, std::string_view Token,
                   db::Database &Database, const core::Config &Config) {
  // TODO: Fetch repository metrics from Git APIs
  // Will have to fetch from
  // Repository
  // - /repos/<owner>/<repo>
  // -- Forks
  // -- Stars,
  // -- Subscribers
  // - /repos/<owner>/<repo>/traffic/clones
  // -- Clones
  // - /repos/<owner>/<repo>/traffic/views
  // -- Views
  // Account
  // - /orgs/<organization>
  // -- Followers

  // Set Headers
  std::unordered_map<std::string, std::string> Headers = {
      {"Accept", "application/vnd.github+json"},
      {"Authorization", std::format("Bearer {}", Token)},
      {"User-Agent", "Glaze"},
      {"X-GitHub-Api-Version", "2022-11-28"},
  };

  // Get instances from database.
  auto DbGetRepoRes = Database.get<git::models::Repository>(RepoId);
  if (!DbGetRepoRes) {
    return {};
  }

  auto Repository = DbGetRepoRes.value();

  auto DbGetAccountRes =
      Database.get<git::models::Account>(Repository.AccountId);
  if (!DbGetAccountRes) {
    return {};
  }

  auto Account = DbGetAccountRes.value();

  std::string Url = std::format("https://api.github.com/repos/{}/{}",
                                Account.Name, Repository.Name);

  // Validate client is initialized
  if (!Client) {
    spdlog::error("HTTP Client is null");
    return std::unexpected(core::Error{"Client initialization failed"});
  }

  // Log the request
  spdlog::debug("Making HTTP GET request to: {}", Url);

  // Repo Stats API
  auto Response = Client->get(Url, Headers);
  if (!Response) {
    std::string ErrorMsg =
        std::format("HTTP request failed: {}", Response.error().message());
    spdlog::error(ErrorMsg);
    return std::unexpected(core::Error{ErrorMsg});
  }

  responses::GitHubRepoStatsResponse RepoStats{};
  auto Error = glz::read<glz::opts{.error_on_unknown_keys = false}>(
      RepoStats, Response->response_body);
  if (Error) {
    std::string ErrorMsg =
        std::format("Failed to parse response: {}",
                    glz::format_error(Error, Response->response_body));
    spdlog::error(ErrorMsg);
    return std::unexpected(core::Error{ErrorMsg});
  }

  Repository.Forks += RepoStats.forks_count;
  Repository.Stars += RepoStats.stargazers_count;
  Repository.Subscribers += RepoStats.subscribers_count;

  // Traffic Metrics
  Response = Client->get(std::format("{}/traffic/clones", Url), Headers);
  if (!Response) {
    std::string ErrorMsg =
        std::format("HTTP request failed: {}", Response.error().message());
    spdlog::error(ErrorMsg);
    return std::unexpected(core::Error{ErrorMsg});
  }

  responses::GitHubRepoTrafficResponse TrafficStats{};
  Error = glz::read<glz::opts{.error_on_unknown_keys = false}>(
      TrafficStats, Response->response_body);
  if (Error) {
    std::string ErrorMsg =
        std::format("Failed to parse response: {}",
                    glz::format_error(Error, Response->response_body));
    spdlog::error(ErrorMsg);
    return std::unexpected(core::Error{ErrorMsg});
  }

  Repository.Clones += TrafficStats.count;

  Response = Client->get(std::format("{}/traffic/views", Url), Headers);
  if (!Response) {
    std::string ErrorMsg =
        std::format("HTTP request failed: {}", Response.error().message());
    spdlog::error(ErrorMsg);
    return std::unexpected(core::Error{ErrorMsg});
  }

  Error = glz::read<glz::opts{.error_on_unknown_keys = false}>(
      TrafficStats, Response->response_body);
  if (Error) {
    std::string ErrorMsg =
        std::format("Failed to parse response: {}",
                    glz::format_error(Error, Response->response_body));
    spdlog::error(ErrorMsg);
    return std::unexpected(core::Error{ErrorMsg});
  }

  Repository.Views += TrafficStats.count;

  // TODO: Update repository records in database
  spdlog::info("Repo: ID: {}, Name: {}, AccountId: {}, Clones: {}, Forks: {}, "
               "Stars: {}, Subscribers: {}, Views: {}",
               Repository.Id, Repository.Name, Repository.AccountId,
               Repository.Clones, Repository.Forks, Repository.Stars,
               Repository.Subscribers, Repository.Views);
  auto DbResponse = Database.update(Repository);
  if (!DbResponse) {
    spdlog::error("Task: Failed to update: {}", RepoId,
                  DbResponse.error().Message);
    return {};
  }
  return {};
}

std::expected<void, core::Error> updateAccounts(db::Database &Database) {
  // TODO: Aggregate repository data to account level
  // TODO: Update account records in database
  return {};
}

std::expected<void, core::Error> updatePlatforms(db::Database &Database) {
  // TODO: Aggregate account/repository data to platform level
  // TODO: Update platform records in database
  return {};
}

std::expected<void, core::Error> runAll(std::string_view Token,
                                        db::Database &Database,
                                        const core::Config &Config) {
  // Create HTTP client
  auto ClientResult = createClient(Config);
  if (!ClientResult) {
    return std::unexpected<core::Error>(ClientResult.error());
  }
  auto Client = ClientResult.value();

  // Run the pipeline in order: Repos → Accounts → Platforms
  auto RepoResult = updateRepositories(
      Client, "2859e33c-8bbb-4852-a50e-cf652a8e8fe6", Token, Database, Config);
  if (!RepoResult) {
    return RepoResult;
  }

  auto AccountResult = updateAccounts(Database);
  if (!AccountResult) {
    return AccountResult;
  }

  return updatePlatforms(Database);
}

} // namespace insights::git::tasks
