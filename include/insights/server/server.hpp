#pragma once
#include "glaze/net/http_server.hpp"
#include "insights/core/result.hpp"

#include <expected>
#include <memory>
#include <optional>

namespace insights::server {

std::expected<std::unique_ptr<glz::http_server<false>>, core::Error>
initServer(std::string Address, int Port);
std::expected<void, core::Error> startServer(
    glz::http_server<false> &Server,
    std::string Address,
    int Port,
    std::optional<int> Workers = std::nullopt
);
// core::Result<void> registerRouter(const GlazeServer &Server,
//                                   const GlazeRouter &Router,
//                                   std::string Route);
} // namespace insights::server
