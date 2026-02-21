#pragma once
#include "glaze/net/http_client.hpp"
#include "insights/core/config.hpp"
#include "insights/core/result.hpp"
#include "insights/db/db.hpp"

#include <expected>
#include <string_view>

namespace insights::github::tasks {

static std::expected<std::shared_ptr<glz::http_client>, core::Error>
createClient(const core::Config &Config);
// Individual task functions for each entity type
auto updateRepositories(
    std::shared_ptr<glz::http_client> Client,
    db::Database &Database,
    const core::Config &Config
) -> std::expected<void, core::Error>;
auto updateAccounts(
    std::shared_ptr<glz::http_client> Client,
    db::Database &Database,
    const core::Config &Config
) -> std::expected<void, core::Error>;

// Orchestrator that runs the full pipeline: Repos → Accounts → Platforms
auto syncStats(db::Database &Database, const core::Config &Config)
    -> std::expected<void, core::Error>;
} // namespace insights::github::tasks
