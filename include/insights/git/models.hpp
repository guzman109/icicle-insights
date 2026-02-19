#pragma once
#include <chrono>
#include <ctime>
#include <optional>
#include <pqxx/pqxx>
#include <string>
#include <string_view>

#include "insights/core/timestamp.hpp"
#include "insights/core/traits.hpp"

namespace insights::git::models {

using Timestamp = std::chrono::system_clock::time_point;

struct Platform {
  std::string Id;
  std::string Name;
  Timestamp CreatedAt;
  Timestamp UpdatedAt;
  std::optional<Timestamp> DeletedAt;
};
struct Account {
  std::string Id;
  std::string Name;
  std::string PlatformId;
  int32_t Followers{0};
  Timestamp CreatedAt;
  Timestamp UpdatedAt;
  std::optional<Timestamp> DeletedAt;
};
struct Repository {
  std::string Id;
  std::string Name;
  std::string AccountId;
  int32_t Clones{0};
  int32_t Forks{0};
  int32_t Stars{0};
  int32_t Subscribers{0};
  int64_t Views{0};
  Timestamp CreatedAt;
  Timestamp UpdatedAt;
  std::optional<Timestamp> DeletedAt;
};
} // namespace insights::git::models

namespace insights::core {
template <> struct DbTraits<git::models::Platform> {
  static constexpr std::string_view TableName = "git_platforms";
  static constexpr std::string_view Columns = "name";

  static constexpr std::string_view UpdateSet = "name=$1";

  static auto toParams(const git::models::Platform &Platform) {
    return std::make_tuple(Platform.Name);
  }

  static git::models::Platform fromRow(const pqxx::row &Row) {
    return {
        .Id = Row["id"].as<std::string>(),
        .Name = Row["name"].as<std::string>(),
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
  static constexpr std::string_view Columns = "name, platform_id, followers";

  static constexpr std::string_view UpdateSet =
      "name=$1, platform_id=$2, followers=$3";

  static auto toParams(const git::models::Account &Account) {
    return std::make_tuple(Account.Name, Account.PlatformId, Account.Followers);
  }

  static git::models::Account fromRow(const pqxx::row &Row) {
    return {
        .Id = Row["id"].as<std::string>(),
        .Name = Row["name"].as<std::string>(),
        .PlatformId = Row["platform_id"].as<std::string>(),
        .Followers = Row["followers"].as<int32_t>(),
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
      "name, account_id, clones, forks, stars, subscribers, views";

  static constexpr std::string_view UpdateSet =
      "name=$1, account_id=$2, clones=$3, forks=$4, stars=$5, subscribers=$6, views=$7";

  static auto toParams(const git::models::Repository &Repository) {
    return std::make_tuple(Repository.Name, Repository.AccountId,
                           Repository.Clones, Repository.Forks,
                           Repository.Stars, Repository.Subscribers,
                           Repository.Views);
  }

  static git::models::Repository fromRow(const pqxx::row &Row) {
    return {
        .Id = Row["id"].as<std::string>(),
        .Name = Row["name"].as<std::string>(),
        .AccountId = Row["account_id"].as<std::string>(),
        .Clones = Row["clones"].as<int32_t>(),
        .Forks = Row["forks"].as<int32_t>(),
        .Stars = Row["stars"].as<int32_t>(),
        .Subscribers = Row["subscribers"].as<int32_t>(),
        .Views = Row["views"].as<int64_t>(),
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
