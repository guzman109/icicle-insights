#pragma once
#include <glaze/net/http_router.hpp>
#include <memory>

#include "insights/db/db.hpp"

namespace insights::server {

void registerCoreRoutes(glz::http_router &Router,
                        std::shared_ptr<db::Database> Database);

} // namespace insights::server
