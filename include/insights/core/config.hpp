#pragma once
#include "insights/core/result.hpp"
#include <cstddef>
#include <cstdlib>
#include <expected>
#include <optional>
#include <string>
namespace insights::core {

struct Config {
  int Port = 3000;
  std::string DatabaseUrl;
  std::string GitHubToken;
  std::string Host{"127.0.0.1"};

  static std::expected<Config, Error> load() {
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

    return Config{
        .Port = Port,
        .DatabaseUrl = DatabaseUrlEnv,
        .GitHubToken = GitHubTokenEnv,
        .Host = Host,
    };
  }
};

} // namespace insights::core
