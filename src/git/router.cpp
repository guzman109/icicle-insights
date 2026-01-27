#include "git/router.hpp"

namespace insights::git {
core::Result<void> registerRoutes(glz::http_router &Router,
                                  std::shared_ptr<db::Database> &Database) {
  auto Res = registerPlatformsRoutes(Router, Database);
  if (!Res) {
    return Res;
  }

  Res = registerAccountsRoutes(Router, Database);
  if (!Res) {
    return Res;
  }

  Res = registerReposRoutes(Router, Database);
  if (!Res) {
    return Res;
  }

  return core::Result<void>{};
}
} // namespace insights::git
