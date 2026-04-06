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
static constexpr std::string_view GitHubSyncTaskName = "GitHubSync";

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

static auto syncRepository(
    std::shared_ptr<glz::http_client> Client,
    db::Database &Database,
    const core::Config &Config,
    github::models::Repository Repository
) -> std::expected<github::models::Repository, core::Error> {
  if (!Client) {
    Log()->error("HTTP Client is null");
    return std::unexpected(core::Error{"Client initialization failed"});
  }

  std::unordered_map<std::string, std::string> Headers = {
      {"Accept", "application/vnd.github+json"},
      {"Authorization", std::format("Bearer {}", Config.GitHubToken)},
      {"User-Agent", "ICICLE-AI"},
      {"X-GitHub-Api-Version", "2022-11-28"},
  };

  auto Account = Database.get<github::models::Account>(Repository.AccountId);
  if (!Account) {
    return std::unexpected(Account.error());
  }

  std::string Url = std::format(
      "https://api.github.com/repos/{}/{}", Account->Name, Repository.Name
  );

  Log()->debug("Making HTTP GET request to: {}", Url);

  auto Response = Client->get(Url, Headers);
  if (!Response) {
    return std::unexpected(core::Error{std::format(
        "GET {} failed: {}", Url, Response.error().message()
    )});
  }

  responses::GitHubRepoStatsResponse RepoStats{};
  auto ParseError = glz::read<glz::opts{.error_on_unknown_keys = false}>(
      RepoStats, Response->response_body
  );
  if (ParseError) {
    return std::unexpected(core::Error{glz::format_error(
        ParseError, Response->response_body
    )});
  }

  Repository.Forks = RepoStats.forks_count;
  Repository.Stars = RepoStats.stargazers_count;
  Repository.Subscribers = RepoStats.subscribers_count;

  Response = Client->get(std::format("{}/traffic/clones", Url), Headers);
  if (!Response) {
    return std::unexpected(core::Error{std::format(
        "GET {} failed: {}",
        std::format("{}/traffic/clones", Url),
        Response.error().message()
    )});
  }

  responses::GitHubRepoTrafficResponse TrafficStats{};
  ParseError = glz::read<glz::opts{.error_on_unknown_keys = false}>(
      TrafficStats, Response->response_body
  );
  if (ParseError) {
    return std::unexpected(core::Error{glz::format_error(
        ParseError, Response->response_body
    )});
  }

  Repository.Clones += TrafficStats.count;

  Response = Client->get(std::format("{}/traffic/views", Url), Headers);
  if (!Response) {
    return std::unexpected(core::Error{std::format(
        "GET {} failed: {}",
        std::format("{}/traffic/views", Url),
        Response.error().message()
    )});
  }

  ParseError = glz::read<glz::opts{.error_on_unknown_keys = false}>(
      TrafficStats, Response->response_body
  );
  if (ParseError) {
    return std::unexpected(core::Error{glz::format_error(
        ParseError, Response->response_body
    )});
  }

  Repository.Views += TrafficStats.count;

  if (auto Result = Database.update(Repository); !Result) {
    return std::unexpected(Result.error());
  }

  Log()->info(
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

  return Repository;
}

auto updateRepositories(
    std::shared_ptr<glz::http_client> Client,
    db::Database &Database,
    const core::Config &Config
) -> std::expected<SyncEntityStats, core::Error> {
  SyncEntityStats StepStats;

  // Fetch all repositories from DB, then sync each one.
  auto Repositories = Database.getAll<github::models::Repository>();
  if (!Repositories) {
    return std::unexpected(Repositories.error());
  }

  for (auto &Repository : *Repositories) {
    auto Result = syncRepository(Client, Database, Config, Repository);
    if (!Result) {
      ++StepStats.Failed;
      Log()->error(
          "Failed syncing repository {}: {}",
          Repository.Id,
          Result.error().Message
      );
      continue;
    }
    ++StepStats.Processed;
  }
  return StepStats;
}

auto updateAccounts(
    std::shared_ptr<glz::http_client> Client,
    db::Database &Database,
    const core::Config &Config
) -> std::expected<SyncEntityStats, core::Error> {
  SyncEntityStats StepStats;
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
    return std::unexpected(Accounts.error());
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
      ++StepStats.Failed;
      spdlog::get("github_sync")
          ->error("GET {} failed: {}", Url, Response.error().message());
      continue;
    }

    responses::GitHubOrgStatsResponse Stats{};
    auto ParseError = glz::read<glz::opts{.error_on_unknown_keys = false}>(
        Stats, Response->response_body
    );
    if (ParseError) {
      ++StepStats.Failed;
      spdlog::get("github_sync")
          ->error(
              "Parse failed: {}",
              glz::format_error(ParseError, Response->response_body)
          );
      continue;
    }

    Account.Followers = Stats.followers;

    spdlog::get("github_sync")
        ->info(
            "Account: ID: {}, Name: {}, Followers: {}",
            Account.Id,
            Account.Name,
            Account.Followers
        );
    if (auto Result = Database.update(Account); !Result) {
      ++StepStats.Failed;
      spdlog::get("github_sync")
          ->error(
              "DB update failed for {}: {}", Account.Id, Result.error().Message
          );
      continue;
    }
    ++StepStats.Processed;
  }
  return StepStats;
}

auto syncStats(const core::Config &Config) -> std::expected<void, core::Error> {
  // Open a fresh connection for this run — closed automatically at scope exit.
  auto DatabaseResult = db::Database::connect(Config.DatabaseUrl);
  if (!DatabaseResult) {
    return std::unexpected(DatabaseResult.error());
  }
  auto &Database = **DatabaseResult;

  std::optional<long long> AttemptId;
  auto AttemptIdResult = Database.recordTaskRunAttemptStart(GitHubSyncTaskName);
  if (!AttemptIdResult) {
    Log()->warn(
        "Failed to record GitHubSync attempt start: {}",
        AttemptIdResult.error().Message
    );
  } else {
    AttemptId = *AttemptIdResult;
  }

  SyncRunStats RunStats;
  auto finishAttempt = [&](std::string_view Status,
                           std::string_view Summary) {
    if (!AttemptId) {
      return;
    }
    auto FinishResult = Database.finishTaskRunAttempt(
        *AttemptId,
        Status,
        Summary,
        RunStats.Repositories.Processed,
        RunStats.Repositories.Failed,
        RunStats.Accounts.Processed,
        RunStats.Accounts.Failed
    );
    if (!FinishResult) {
      Log()->warn(
          "Failed to finish GitHubSync attempt record: {}",
          FinishResult.error().Message
      );
    }
  };

  auto LockResult = Database.tryAcquireTaskLock(GitHubSyncTaskName);
  if (!LockResult) {
    finishAttempt("failed", LockResult.error().Message);
    return std::unexpected(LockResult.error());
  }
  if (!*LockResult) {
    std::string Summary = "Skipped: another GitHubSync run is already active.";
    Log()->warn(Summary);
    finishAttempt("skipped", Summary);
    return {};
  }

  struct TaskLockGuard {
    db::Database &Database;
    std::string_view TaskName;

    ~TaskLockGuard() {
      auto Result = Database.releaseTaskLock(TaskName);
      if (!Result) {
        spdlog::get("github_sync")
            ->error(
                "Failed to release {} advisory lock: {}",
                TaskName,
                Result.error().Message
            );
      }
    }
  } LockGuard{Database, GitHubSyncTaskName};

  // Create HTTP client
  auto ClientResult = createClient(Config);
  if (!ClientResult) {
    finishAttempt("failed", ClientResult.error().Message);
    return std::unexpected(ClientResult.error());
  }
  auto Client = *ClientResult;

  // Run the pipeline in order: Repos → Accounts
  auto RepoResult = updateRepositories(Client, Database, Config);
  if (!RepoResult) {
    finishAttempt("failed", RepoResult.error().Message);
    return std::unexpected(RepoResult.error());
  }
  RunStats.Repositories = *RepoResult;

  auto AccountResult = updateAccounts(Client, Database, Config);
  if (!AccountResult) {
    finishAttempt("failed", AccountResult.error().Message);
    return std::unexpected(AccountResult.error());
  }
  RunStats.Accounts = *AccountResult;

  // Record completion
  auto RecordResult = Database.recordTaskRun(GitHubSyncTaskName);
  if (!RecordResult) {
    finishAttempt("failed", RecordResult.error().Message);
    Log()->warn("recordTaskRun failed: {}", RecordResult.error().Message);
    return std::unexpected(RecordResult.error());
  }

  auto Summary =
      RunStats.hadFailures()
          ? std::format(
                "Sync completed with partial failures. repos ok={}, repos failed={}, "
                "accounts ok={}, accounts failed={}",
                RunStats.Repositories.Processed,
                RunStats.Repositories.Failed,
                RunStats.Accounts.Processed,
                RunStats.Accounts.Failed
            )
          : std::format(
                "Sync completed successfully. repos={}, accounts={}",
                RunStats.Repositories.Processed,
                RunStats.Accounts.Processed
            );
  finishAttempt(
      RunStats.hadFailures() ? "partial_success" : "success", Summary
  );

  return {};
}

auto syncRepositoryById(
    std::string_view RepositoryId,
    db::Database &Database,
    const core::Config &Config
) -> std::expected<github::models::Repository, core::Error> {
  auto Repository = Database.get<github::models::Repository>(RepositoryId);
  if (!Repository) {
    return std::unexpected(Repository.error());
  }

  auto ClientResult = createClient(Config);
  if (!ClientResult) {
    return std::unexpected(ClientResult.error());
  }

  return syncRepository(*ClientResult, Database, Config, *Repository);
}

} // namespace insights::github::tasks
