#pragma once
#include "insights/db/db.hpp"

#include <glaze/net/http_router.hpp>
#include <memory>

namespace insights::core {

void registerCoreRoutes(
    glz::http_router &Router, std::shared_ptr<db::Database> Database
);

} // namespace insights::core
