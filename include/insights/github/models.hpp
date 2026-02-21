#pragma once
#include "insights/core/timestamp.hpp"
#include "insights/core/traits.hpp"

#include <chrono>
#include <ctime>
#include <optional>
#include <pqxx/pqxx>
#include <string>
#include <string_view>
#include <tuple>

namespace insights::github::models {

using Timestamp = std::chrono::system_clock::time_point;

struct Account {
  std::string Id;
  std::string Name;
  int Followers{0};
  Timestamp CreatedAt;
  Timestamp UpdatedAt;
  std::optional<Timestamp> DeletedAt;
};
struct Repository {
  std::string Id;
  std::string Name;
  std::string AccountId;
  int Clones{0};
  int Forks{0};
  int Stars{0};
  int Subscribers{0};
  int Views{0};
  Timestamp CreatedAt;
  Timestamp UpdatedAt;
  std::optional<Timestamp> DeletedAt;
};
} // namespace insights::github::models

namespace insights::core {

template <> struct DbTraits<github::models::Account> {
  static constexpr std::string_view TableName = "github_accounts";
  static constexpr std::string_view Columns = "name, followers";

  static constexpr std::string_view UpdateSet = "name=$1, followers=$2";

  static auto toParams(const github::models::Account &Account) {
    return std::make_tuple(Account.Name, Account.Followers);
  }

  static github::models::Account fromRow(const pqxx::row &Row) {
    return {
        .Id = Row["id"].as<std::string>(),
        .Name = Row["name"].as<std::string>(),
        .Followers = Row["followers"].as<int>(),
        .CreatedAt = core::parseTimestamp(Row["created_at"].as<std::string>()),
        .UpdatedAt = core::parseTimestamp(Row["updated_at"].as<std::string>()),
        .DeletedAt = Row["deleted_at"].is_null()
                         ? std::nullopt
                         : std::optional{core::parseTimestamp(
                               Row["deleted_at"].as<std::string>()
                           )},
    };
  }
};

template <> struct DbTraits<github::models::Repository> {
  static constexpr std::string_view TableName = "github_repositories";
  static constexpr std::string_view Columns =
      "name, account_id, clones, forks, stars, subscribers, views";

  static constexpr std::string_view UpdateSet =
      "name=$1, account_id=$2, clones=$3, forks=$4, stars=$5, subscribers=$6, "
      "views=$7";

  static auto toParams(const github::models::Repository &Repository) {
    return std::make_tuple(
        Repository.Name,
        Repository.AccountId,
        Repository.Clones,
        Repository.Forks,
        Repository.Stars,
        Repository.Subscribers,
        Repository.Views
    );
  }

  static github::models::Repository fromRow(const pqxx::row &Row) {
    return {
        .Id = Row["id"].as<std::string>(),
        .Name = Row["name"].as<std::string>(),
        .AccountId = Row["account_id"].as<std::string>(),
        .Clones = Row["clones"].as<int>(),
        .Forks = Row["forks"].as<int>(),
        .Stars = Row["stars"].as<int>(),
        .Subscribers = Row["subscribers"].as<int>(),
        .Views = Row["views"].as<int>(),
        .CreatedAt = core::parseTimestamp(Row["created_at"].as<std::string>()),
        .UpdatedAt = core::parseTimestamp(Row["updated_at"].as<std::string>()),
        .DeletedAt = Row["deleted_at"].is_null()
                         ? std::nullopt
                         : std::optional{core::parseTimestamp(
                               Row["deleted_at"].as<std::string>()
                           )},
    };
  }
};

} // namespace insights::core
