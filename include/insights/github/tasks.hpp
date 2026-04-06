#pragma once
#include "glaze/net/http_client.hpp"
#include "insights/core/config.hpp"
#include "insights/github/models.hpp"
#include "insights/core/result.hpp"
#include "insights/db/db.hpp"

#include <expected>
#include <string_view>

namespace insights::github::tasks {

struct SyncEntityStats {
  int Processed{0};
  int Failed{0};
};

struct SyncRunStats {
  SyncEntityStats Repositories;
  SyncEntityStats Accounts;

  bool hadFailures() const {
    return Repositories.Failed > 0 || Accounts.Failed > 0;
  }
};

static std::expected<std::shared_ptr<glz::http_client>, core::Error>
createClient(const core::Config &Config);
// Individual task functions for each entity type
auto updateRepositories(
    std::shared_ptr<glz::http_client> Client,
    db::Database &Database,
    const core::Config &Config
) -> std::expected<SyncEntityStats, core::Error>;
auto updateAccounts(
    std::shared_ptr<glz::http_client> Client,
    db::Database &Database,
    const core::Config &Config
) -> std::expected<SyncEntityStats, core::Error>;

auto syncRepositoryById(
    std::string_view RepositoryId,
    db::Database &Database,
    const core::Config &Config
) -> std::expected<github::models::Repository, core::Error>;

// Orchestrator that runs the full pipeline: Repos → Accounts
auto syncStats(const core::Config &Config) -> std::expected<void, core::Error>;
} // namespace insights::github::tasks
