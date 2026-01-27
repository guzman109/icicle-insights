#pragma once
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <string>

#include "core/result.hpp"
namespace insights::core {

struct Config {
  std::string DatabaseUrl;
  std::string GitHubToken;

  static insights::core::Result<Config> load() {
    auto *DatabaseUrl = std::getenv("DATABASE_URL");
    auto *GitHubToken = std::getenv("GITHUB_TOKEN");

    if (DatabaseUrl == nullptr) {
      return std::unexpected(Error{"DATABASE_URL is required"});
    }

    if (GitHubToken == nullptr) {
      return std::unexpected(Error{"GITHUB_TOKEN is required"});
    }

    return Config{.DatabaseUrl = DatabaseUrl, .GitHubToken = GitHubToken};
  }
};

} // namespace insights::core
