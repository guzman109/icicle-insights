#pragma once
#include <cstddef>
#include <cstdlib>
#include <expected>
#include <optional>
#include <string>

#include "core/result.hpp"
namespace insights::core {

struct Config {
  std::string DatabaseUrl;
  std::string GitHubToken;
  std::string Host{"127.0.0.1"};
  int Port = 3000;

  static insights::core::Result<Config> load() {
    auto *DatabaseUrlEnv = std::getenv("DATABASE_URL");
    auto *GitHubTokenEnv = std::getenv("GITHUB_TOKEN");
    auto *HostEnv = std::getenv("HOST");
    auto *PortEnv = std::getenv("PORT");

    if (DatabaseUrlEnv == nullptr) {
      return std::unexpected(Error{"DATABASE_URL is required"});
    }

    if (GitHubTokenEnv == nullptr) {
      return std::unexpected(Error{"GITHUB_TOKEN is required"});
    }

    std::string Host = "127.0.0.1";
    if (HostEnv != nullptr) {
      Host = HostEnv;
    }

    int Port = 3000;
    if (PortEnv != nullptr) {
      Port = std::stoi(PortEnv);
    }

    return Config{.DatabaseUrl = DatabaseUrlEnv, .GitHubToken = GitHubTokenEnv,
                  .Host = Host, .Port = Port};
  }
};

} // namespace insights::core
