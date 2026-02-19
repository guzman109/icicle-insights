#pragma once
#include "glaze/net/http_client.hpp"
#include "insights/core/config.hpp"
#include "insights/core/result.hpp"
#include "insights/db/db.hpp"
#include <expected>
#include <string_view>

namespace insights::git::tasks {

static std::expected<std::shared_ptr<glz::http_client>, core::Error>
createClient(const core::Config &Config);
// Individual task functions for each entity type
std::expected<void, core::Error>
updateRepositories(std::shared_ptr<glz::http_client> Client,
                   std::string_view Token, db::Database &Database,
                   const core::Config &Config);
std::expected<void, core::Error> updateAccounts(db::Database &Database);
std::expected<void, core::Error> updatePlatforms(db::Database &Database);

// Orchestrator that runs the full pipeline: Repos → Accounts → Platforms
std::expected<void, core::Error> runAll(std::string_view Token,
                                        db::Database &Database,
                                        const core::Config &Config);
} // namespace insights::git::tasks
