#pragma once
#include <chrono>
#include <ctime>
#include <optional>
#include <pqxx/pqxx>
#include <string>
#include <string_view>

#include "core/timestamp.hpp"
#include "core/traits.hpp"

namespace insights::git::models {

using Timestamp = std::chrono::system_clock::time_point;

struct Platform {
  std::string Id;
  std::string Name;
  int32_t Clones{0};
  int32_t Followers{0};
  int32_t Forks{0};
  int32_t Stars{0};
  int64_t Views{0};
  int32_t Watchers{0};
  Timestamp CreatedAt;
  Timestamp UpdatedAt;
  std::optional<Timestamp> DeletedAt;
};
struct Account {
  std::string Id;
  std::string Name;
  std::string PlatformId;
  int32_t Clones{0};
  int32_t Followers{0};
  int32_t Forks{0};
  int32_t Stars{0};
  int64_t Views{0};
  int32_t Watchers{0};
  Timestamp CreatedAt;
  Timestamp UpdatedAt;
  std::optional<Timestamp> DeletedAt;
};
struct Repository {
  std::string Id;
  std::string Name;
  std::string AccountId;
  int32_t Clones{0};
  int32_t Followers{0};
  int32_t Forks{0};
  int32_t Stars{0};
  int64_t Views{0};
  int32_t Watchers{0};
  Timestamp CreatedAt;
  Timestamp UpdatedAt;
  std::optional<Timestamp> DeletedAt;
};
} // namespace insights::git::models

namespace insights::core {
template <> struct DbTraits<git::models::Platform> {
  static constexpr std::string_view TableName = "git_platforms";
  static constexpr std::string_view Columns =
      "name, clones, followers, forks, stars, views, watchers";

  static constexpr std::string_view UpdateSet =
      "name=$1, clones=$2, followers=$3, forks=$4, stars=$5, views=$6, "
      "watchers=$7";

  static auto toParams(const git::models::Platform &Platform) {
    return std::make_tuple(Platform.Name, Platform.Clones, Platform.Followers,
                           Platform.Forks, Platform.Stars, Platform.Views,
                           Platform.Watchers);
  }

  static git::models::Platform fromRow(const pqxx::row &Row) {
    return {
        .Id = Row["id"].as<std::string>(),
        .Name = Row["name"].as<std::string>(),
        .Clones = Row["clones"].as<int32_t>(),
        .Followers = Row["followers"].as<int32_t>(),
        .Forks = Row["forks"].as<int32_t>(),
        .Stars = Row["stars"].as<int32_t>(),
        .Views = Row["views"].as<int64_t>(),
        .Watchers = Row["watchers"].as<int32_t>(),
        .CreatedAt = core::parseTimestamp(Row["created_at"].as<std::string>()),
        .UpdatedAt = core::parseTimestamp(Row["updated_at"].as<std::string>()),
        .DeletedAt = Row["deleted_at"].is_null()
                         ? std::nullopt
                         : std::optional{core::parseTimestamp(
                               Row["deleted_at"].as<std::string>())},
    };
  }
};

template <> struct DbTraits<git::models::Account> {
  static constexpr std::string_view TableName = "git_accounts";
  static constexpr std::string_view Columns =
      "name, platform_id, clones, followers, forks, stars, views, watchers";

  static constexpr std::string_view UpdateSet =
      "name=$1, platform_id=$2, clones=$3, followers=$4, forks=$5, stars=$6, "
      "views=$7, "
      "watchers=$8";

  static auto toParams(const git::models::Account &Account) {
    return std::make_tuple(Account.Name, Account.PlatformId, Account.Clones,
                           Account.Followers, Account.Forks, Account.Stars,
                           Account.Views, Account.Watchers);
  }

  static git::models::Account fromRow(const pqxx::row &Row) {
    return {
        .Id = Row["id"].as<std::string>(),
        .Name = Row["name"].as<std::string>(),
        .PlatformId = Row["platform_id"].as<std::string>(),
        .Clones = Row["clones"].as<int32_t>(),
        .Followers = Row["followers"].as<int32_t>(),
        .Forks = Row["forks"].as<int32_t>(),
        .Stars = Row["stars"].as<int32_t>(),
        .Views = Row["views"].as<int64_t>(),
        .Watchers = Row["watchers"].as<int32_t>(),
        .CreatedAt = core::parseTimestamp(Row["created_at"].as<std::string>()),
        .UpdatedAt = core::parseTimestamp(Row["updated_at"].as<std::string>()),
        .DeletedAt = Row["deleted_at"].is_null()
                         ? std::nullopt
                         : std::optional{core::parseTimestamp(
                               Row["deleted_at"].as<std::string>())},
    };
  }
};

template <> struct DbTraits<git::models::Repository> {
  static constexpr std::string_view TableName = "git_repositories";
  static constexpr std::string_view Columns =
      "name, account_id, clones, followers, forks, stars, views, watchers";

  static constexpr std::string_view UpdateSet =
      "name=$1, account_id=$2, clones=$3, followers=$4, forks=$5, stars=$6, "
      "views=$7, "
      "watchers=$8";

  static auto toParams(const git::models::Repository &Repository) {
    return std::make_tuple(Repository.Name, Repository.AccountId,
                           Repository.Clones, Repository.Followers,
                           Repository.Forks, Repository.Stars, Repository.Views,
                           Repository.Watchers);
  }

  static git::models::Repository fromRow(const pqxx::row &Row) {
    return {
        .Id = Row["id"].as<std::string>(),
        .Name = Row["name"].as<std::string>(),
        .AccountId = Row["account_id"].as<std::string>(),
        .Clones = Row["clones"].as<int32_t>(),
        .Followers = Row["followers"].as<int32_t>(),
        .Forks = Row["forks"].as<int32_t>(),
        .Stars = Row["stars"].as<int32_t>(),
        .Views = Row["views"].as<int64_t>(),
        .Watchers = Row["watchers"].as<int32_t>(),
        .CreatedAt = core::parseTimestamp(Row["created_at"].as<std::string>()),
        .UpdatedAt = core::parseTimestamp(Row["updated_at"].as<std::string>()),
        .DeletedAt = Row["deleted_at"].is_null()
                         ? std::nullopt
                         : std::optional{core::parseTimestamp(
                               Row["deleted_at"].as<std::string>())},
    };
  }
};

} // namespace insights::core
