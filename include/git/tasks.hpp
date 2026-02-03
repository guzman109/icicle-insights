#pragma once
#include "core/config.hpp"
#include "core/result.hpp"
#include "db/db.hpp"
#include "glaze/net/http_client.hpp"
#include <expected>

namespace insights::git::tasks {

static std::expected<std::shared_ptr<glz::http_client>, core::Error> createClient(const core::Config &Config);
// Individual task functions for each entity type
std::expected<void, core::Error> updateRepositories(std::shared_ptr<glz::http_client> Client,
                                      std::string RepoId, std::string Token,
                                      db::Database &Database,
                                      const core::Config &Config);
std::expected<void, core::Error> updateAccounts(db::Database &Database);
std::expected<void, core::Error> updatePlatforms(db::Database &Database);

// Orchestrator that runs the full pipeline: Repos → Accounts → Platforms
std::expected<void, core::Error> runAll(std::string RepoId, std::string Token,
                          db::Database &Database, const core::Config &Config);
} // namespace insights::git::tasks
