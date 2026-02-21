#pragma once
#include "glaze/net/http_router.hpp"
#include "insights/core/result.hpp"
#include "insights/db/db.hpp"

#include <expected>
#include <memory>
#include <optional>
#include <string>

namespace insights::github {

struct CreateAccountSchema {
  std::string Name;
  std::optional<int> Followers;
};

struct CreateRepositorySchema {
  std::string Name;
  std::string AccountId;
  std::optional<int> Clones;
  std::optional<int> Forks;
  std::optional<int> Stars;
  std::optional<int> Subscribers;
  std::optional<int> Views;
};

struct UpdateSchema {
  std::optional<int> Clones;
  std::optional<int> Followers;
  std::optional<int> Forks;
  std::optional<int> Stars;
  std::optional<int> Views;
  std::optional<int> Subscribers;
};

struct OutputAccountSchema {
  std::string Id;
  std::string Name;
  int Followers{0};
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

auto registerRoutes(glz::http_router &Router,
                    std::shared_ptr<db::Database> &Database)
    -> std::expected<void, core::Error>;

} // namespace insights::github
