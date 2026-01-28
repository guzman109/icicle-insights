#pragma once
#include "core/result.hpp"
#include "db/db.hpp"

namespace insights::git::tasks {
  // Individual task functions for each entity type
  core::Result<void> updateRepositories(db::Database &Database);
  core::Result<void> updateAccounts(db::Database &Database);
  core::Result<void> updatePlatforms(db::Database &Database);

  // Orchestrator that runs the full pipeline: Repos → Accounts → Platforms
  core::Result<void> runAll(db::Database &Database);
}
