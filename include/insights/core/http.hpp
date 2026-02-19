#pragma once

#include "glaze/core/opts.hpp"
#include <cstdint>

namespace insights::core {

inline constexpr auto JsonOpts = glz::opts{.error_on_missing_keys = true};


enum class HttpStatus : uint16_t {
  // 2xx Success
  Ok = 200,
  Created = 201,
  Accepted = 202,
  NoContent = 204,

  // 4xx Client Errors
  BadRequest = 400,
  Unauthorized = 401,
  Forbidden = 403,
  NotFound = 404,
  Conflict = 409,
  UnprocessableEntity = 422,

  // 5xx Server Errors
  InternalServerError = 500,
  NotImplemented = 501,
  BadGateway = 502,
  ServiceUnavailable = 503,
};

} // namespace insights::core
