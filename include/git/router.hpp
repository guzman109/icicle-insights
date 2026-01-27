#pragma once
#include "core/result.hpp"
#include "db/db.hpp"
#include "glaze/net/http_router.hpp"
#include <memory>
#include <optional>
#include <string>

namespace insights::git {
struct CreatePlatformSchema {
  std::string Name;
  int Clones{0};
  int Followers{0};
  int Forks{0};
  int Stars{0};
  int Views{0};
  int Watchers{0};
};
struct CreateAccountSchema {
  std::string Name;
  std::string PlatformId;
  int Clones{0};
  int Followers{0};
  int Forks{0};
  int Stars{0};
  int Views{0};
  int Watchers{0};
};
struct CreateRepositorySchema {
  std::string Name;
  std::string AccountId;
  int Clones{0};
  int Followers{0};
  int Forks{0};
  int Stars{0};
  int Views{0};
  int Watchers{0};
};

struct UpdateSchema {
  int Clones{0};
  int Followers{0};
  int Forks{0};
  int Stars{0};
  int Views{0};
  int Watchers{0};
};

struct OutputPlatformSchema {
  std::string Id;
  std::string Name;
  int Clones{0};
  int Followers{0};
  int Forks{0};
  int Stars{0};
  long long Views{0};
  int Watchers{0};
};

struct OutputAccountSchema {
  std::string Id;
  std::string Name;
  std::string PlatformId;
  int Clones{0};
  int Followers{0};
  int Forks{0};
  int Stars{0};
  long long Views{0};
  int Watchers{0};
};

struct OutputRepositorySchema {
  std::string Id;
  std::string Name;
  std::string AccountId;
  int Clones{0};
  int Followers{0};
  int Forks{0};
  int Stars{0};
  long long Views{0};
  int Watchers{0};
};
core::Result<void> registerRoutes(glz::http_router &Router,
                                  std::shared_ptr<db::Database> &Database);

core::Result<void>
registerPlatformsRoutes(glz::http_router &Router,
                        std::shared_ptr<db::Database> &Database);
core::Result<void>
registerAccountsRoutes(glz::http_router &Router,
                       std::shared_ptr<db::Database> &Database);
core::Result<void> registerReposRoutes(glz::http_router &Router,
                                       std::shared_ptr<db::Database> &Database);

} // namespace insights::git
