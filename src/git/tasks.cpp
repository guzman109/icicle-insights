#include "git/tasks.hpp"

namespace insights::git::tasks {

core::Result<void> updateRepositories(db::Database &Database) {
  // TODO: Fetch repository metrics from Git APIs
  // TODO: Update repository records in database
  return {};
}

core::Result<void> updateAccounts(db::Database &Database) {
  // TODO: Aggregate repository data to account level
  // TODO: Update account records in database
  return {};
}

core::Result<void> updatePlatforms(db::Database &Database) {
  // TODO: Aggregate account/repository data to platform level
  // TODO: Update platform records in database
  return {};
}

core::Result<void> runAll(db::Database &Database) {
  // Run the pipeline in order: Repos → Accounts → Platforms
  auto RepoResult = updateRepositories(Database);
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
