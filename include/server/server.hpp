#pragma once
#include "core/result.hpp"
#include "glaze/net/http_server.hpp"
#include <memory>
#include <optional>

namespace insights::server {

core::Result<std::unique_ptr<glz::http_server<false>>>
initServer(std::string Address, int Port);
core::Result<void> startServer(glz::http_server<false> &Server,
                               std::optional<int> Workers = std::nullopt);
// core::Result<void> registerRouter(const GlazeServer &Server,
//                                   const GlazeRouter &Router,
//                                   std::string Route);
} // namespace insights::server
