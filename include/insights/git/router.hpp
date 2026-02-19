#pragma once
#include "insights/core/result.hpp"
#include "insights/db/db.hpp"
#include "glaze/net/http_router.hpp"
#include <expected>
#include <memory>
#include <optional>
#include <string>

namespace insights::git {
struct CreatePlatformSchema {
  std::string Name;
};
struct CreateAccountSchema {
  std::string Name;
  std::string PlatformId;
  int Followers{0};
};
struct CreateRepositorySchema {
  std::string Name;
  std::string AccountId;
  int Clones{0};
  int Forks{0};
  int Stars{0};
  int Subscribers{0};
  int Views{0};
};

struct UpdateSchema {
  int Clones{0};
  int Followers{0};
  int Forks{0};
  int Stars{0};
  int Views{0};
  int Subscribers{0};
};

struct OutputPlatformSchema {
  std::string Id;
  std::string Name;
  int Accounts;
  int Repositories;
  int Subscribers{0};
};

struct OutputAccountSchema {
  std::string Id;
  std::string Name;
  std::string PlatformId;
  int Followers{0};
  int Repositories{0};
};

struct OutputRepositorySchema {
  std::string Id;
  std::string Name;
  std::string AccountId;
  int Clones{0};
  int Forks{0};
  int Stars{0};
  int Subscribers{0};
  long long Views{0};
};
std::expected<void, core::Error> registerRoutes(glz::http_router &Router,
                                  std::shared_ptr<db::Database> &Database);

std::expected<void, core::Error>
registerPlatformsRoutes(glz::http_router &Router,
                        std::shared_ptr<db::Database> &Database);
std::expected<void, core::Error>
registerAccountsRoutes(glz::http_router &Router,
                       std::shared_ptr<db::Database> &Database);
std::expected<void, core::Error> registerReposRoutes(glz::http_router &Router,
                                       std::shared_ptr<db::Database> &Database);

} // namespace insights::git
