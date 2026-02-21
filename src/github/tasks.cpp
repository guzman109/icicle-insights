#include "insights/github/tasks.hpp"

#include "glaze/net/http_client.hpp"
#include "insights/core/result.hpp"
#include "insights/github/models.hpp"
#include "insights/github/responses.hpp"

#include <asio/ssl.hpp>
#include <expected>
#include <format>
#include <glaze/core/reflect.hpp>
#include <glaze/json/generic.hpp>
#include <glaze/json/lazy.hpp>
#include <glaze/json/read.hpp>
#include <glaze/net/http_router.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <system_error>

namespace insights::github::tasks {

static auto Log() { return spdlog::get("github_sync"); }

static std::expected<std::shared_ptr<glz::http_client>, core::Error>
createClient(const core::Config &Config) {
  auto Client = std::make_shared<glz::http_client>();

  auto Ok = Client->configure_system_ca_certificates();
  if (!Ok) {
    Log()->error("Error: Could not find CA certificates.");
    return std::unexpected(
        core::Error{"Error: Could not find CA certificates."}
    );
  }
  return Client;
}
auto updateRepositories(
    std::shared_ptr<glz::http_client> Client,
    db::Database &Database,
    const core::Config &Config
) -> std::expected<void, core::Error> {

  // Set Headers
  std::unordered_map<std::string, std::string> Headers = {
      {"Accept", "application/vnd.github+json"},
      {"Authorization", std::format("Bearer {}", Config.GitHubToken)},
      {"User-Agent", "Glaze"},
      {"X-GitHub-Api-Version", "2022-11-28"},
  };

  // Fetch all repositories from DB, then sync each one.
  auto Repositories = Database.getAll<github::models::Repository>();
  if (!Repositories) {
    return {};
  }

  // Validate client is initialized
  if (!Client) {
    Log()->error("HTTP Client is null");
    return std::unexpected(core::Error{"Client initialization failed"});
  }

  for (auto &Repository : *Repositories) {
    auto Account = Database.get<github::models::Account>(Repository.AccountId);
    if (!Account) {
      continue;
    }

    std::string Url = std::format(
        "https://api.github.com/repos/{}/{}", Account->Name, Repository.Name
    );

    // Log the request
    Log()->debug("Making HTTP GET request to: {}", Url);

    // Repo Stats API
    auto Response = Client->get(Url, Headers);
    if (!Response) {
      spdlog::get("github_sync")
          ->error("GET {} failed: {}", Url, Response.error().message());
      continue;
    }

    responses::GitHubRepoStatsResponse Stats{};
    auto ParseError = glz::read<glz::opts{.error_on_unknown_keys = false}>(
        Stats, Response->response_body
    );
    if (ParseError) {
      spdlog::get("github_sync")
          ->error(
              "Parse failed: {}",
              glz::format_error(ParseError, Response->response_body)
          );
      continue;
    }

    Repository.Forks += Stats.forks_count;
    Repository.Stars += Stats.stargazers_count;
    Repository.Subscribers += Stats.subscribers_count;

    // Traffic Metrics
    Response = Client->get(std::format("{}/traffic/clones", Url), Headers);
    if (!Response) {
      spdlog::get("github_sync")
          ->error("GET {} failed: {}", Url, Response.error().message());
      continue;
    }

    responses::GitHubRepoTrafficResponse TrafficStats{};
    ParseError = glz::read<glz::opts{.error_on_unknown_keys = false}>(
        TrafficStats, Response->response_body
    );
    if (ParseError) {
      spdlog::get("github_sync")
          ->error(
              "Parse failed: {}",
              glz::format_error(ParseError, Response->response_body)
          );
      continue;
    }

    Repository.Clones += TrafficStats.count;

    Response = Client->get(std::format("{}/traffic/views", Url), Headers);
    if (!Response) {
      spdlog::get("github_sync")
          ->error("GET {} failed: {}", Url, Response.error().message());
      continue;
    }

    ParseError = glz::read<glz::opts{.error_on_unknown_keys = false}>(
        TrafficStats, Response->response_body
    );
    if (ParseError) {
      spdlog::get("github_sync")
          ->error(
              "Parse failed: {}",
              glz::format_error(ParseError, Response->response_body)
          );
      continue;
    }

    Repository.Views += TrafficStats.count;

    spdlog::get("github_sync")
        ->info(
            "Repo: ID: {}, Name: {}, AccountId: {}, Clones: {}, Forks: {}, "
            "Stars: {}, Subscribers: {}, Views: {}",
            Repository.Id,
            Repository.Name,
            Repository.AccountId,
            Repository.Clones,
            Repository.Forks,
            Repository.Stars,
            Repository.Subscribers,
            Repository.Views
        );
    if (auto Result = Database.update(Repository); !Result) {
      spdlog::get("github_sync")
          ->error(
              "DB update failed for {}: {}",
              Repository.Id,
              Result.error().Message
          );
    }
  }
  return {};
}

// Account
// - /orgs/<organization>
// -- Followers
auto updateAccounts(
    std::shared_ptr<glz::http_client> Client,
    db::Database &Database,
    const core::Config &Config
) -> std::expected<void, core::Error> {
  // Set Headers
  std::unordered_map<std::string, std::string> Headers = {
      {"Accept", "application/vnd.github+json"},
      {"Authorization", std::format("Bearer {}", Config.GitHubToken)},
      {"User-Agent", "Glaze"},
      {"X-GitHub-Api-Version", "2022-11-28"},
  };

  // Fetch all repositories from DB, then sync each one.
  auto Accounts = Database.getAll<github::models::Account>();
  if (!Accounts) {
    return {};
  }

  // Validate client is initialized
  if (!Client) {
    Log()->error("HTTP Client is null");
    return std::unexpected(core::Error{"Client initialization failed"});
  }

  for (auto &Account : *Accounts) {
    std::string Url =
        std::format("https://api.github.com/orgs/{}", Account.Name);

    // Log the request
    Log()->debug("Making HTTP GET request to: {}", Url);

    // Repo Stats API
    auto Response = Client->get(Url, Headers);
    if (!Response) {
      spdlog::get("github_sync")
          ->error("GET {} failed: {}", Url, Response.error().message());
      continue;
    }

    responses::GitHubOrgStatsResponse Stats{};
    auto ParseError = glz::read<glz::opts{.error_on_unknown_keys = false}>(
        Stats, Response->response_body
    );
    if (ParseError) {
      spdlog::get("github_sync")
          ->error(
              "Parse failed: {}",
              glz::format_error(ParseError, Response->response_body)
          );
      continue;
    }

    Account.Followers += Stats.followers;

    spdlog::get("github_sync")
        ->info(
            "Account: ID: {}, Name: {}, Followers: {}",
            Account.Id,
            Account.Name,
            Account.Followers
        );
    if (auto Result = Database.update(Account); !Result) {
      spdlog::get("github_sync")
          ->error(
              "DB update failed for {}: {}", Account.Id, Result.error().Message
          );
    }
  }
  return {};
}

auto syncStats(db::Database &Database, const core::Config &Config)
    -> std::expected<void, core::Error> {
  // Create HTTP client
  auto ClientResult = createClient(Config);
  if (!ClientResult) {
    return std::unexpected(ClientResult.error());
  }
  auto Client = *ClientResult;

  // Run the pipeline in order: Repos → Accounts → Platforms
  auto RepoResult = updateRepositories(Client, Database, Config);
  if (!RepoResult) {
    return RepoResult;
  }
  return updateAccounts(Client, Database, Config);
}

} // namespace insights::github::tasks
