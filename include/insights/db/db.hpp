#pragma once
#include "insights/core/result.hpp"
#include "insights/core/traits.hpp"

#include <chrono>
#include <exception>
#include <expected>
#include <format>
#include <memory>
#include <pqxx/pqxx>
#include <pqxx/zview>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace insights::db {
struct Database {
  pqxx::connection Cx;

  explicit Database(const std::string &ConnString)
      : Cx(ConnString), ConnString(ConnString) {}

  static std::expected<std::shared_ptr<Database>, core::Error>
  connect(const std::string &ConnString) {
    try {
      spdlog::debug("Database::connect - Establishing connection");
      auto Db = std::make_shared<Database>(ConnString);
      spdlog::info("Database::connect - Successfully connected to database");
      return Db;
    } catch (const std::exception &Err) {
      spdlog::error("Database::connect - Connection failed: {}", Err.what());
      return std::unexpected(core::Error{Err.what()});
    }
  }

  bool reconnect() {
    spdlog::warn("Database::reconnect - Attempting to reconnect");
    try {
      Cx = pqxx::connection(ConnString);
      spdlog::info("Database::reconnect - Reconnected successfully");
      return true;
    } catch (const std::exception &Err) {
      spdlog::error("Database::reconnect - Failed: {}", Err.what());
      return false;
    }
  }

private:
  std::string ConnString;

  static constexpr int MaxRetries = 3;
  static constexpr std::chrono::seconds BaseDelay{1};

  static std::chrono::seconds backoffDelay(int Attempt) {
    // Exponential backoff: BaseDelay * 2^Attempt, max 30s
    auto Delay = BaseDelay.count() * (1LL << Attempt);
    return std::chrono::seconds(std::min(Delay, 30LL));
  }

  template <typename F>
  auto withRetry(const char *OpName, F &&Op)
      -> std::expected<std::invoke_result_t<F>, core::Error> {
    for (int Attempt = 0; Attempt <= MaxRetries; ++Attempt) {
      try {
        if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
          std::forward<F>(Op)();
          return {};
        } else {
          return std::forward<F>(Op)();
        }
      } catch (const pqxx::broken_connection &Err) {
        if (Attempt == MaxRetries) {
          spdlog::error("{} - Connection lost, no retries left: {}", OpName, Err.what());
          return std::unexpected(core::Error{Err.what()});
        }
        auto Delay = backoffDelay(Attempt);
        spdlog::warn(
            "{} - Connection lost (attempt {}/{}), retrying in {}s: {}",
            OpName, Attempt + 1, MaxRetries, Delay.count(), Err.what()
        );
        std::this_thread::sleep_for(Delay);
        reconnect();
      } catch (const std::exception &Err) {
        std::string_view Msg = Err.what();
        bool IsConnectionError = 
            Msg.find("connection") != std::string_view::npos ||
            Msg.find("Connection") != std::string_view::npos ||
            Msg.find("lost") != std::string_view::npos ||
            Msg.find("Lost") != std::string_view::npos ||
            Msg.find("broken") != std::string_view::npos;

        if (IsConnectionError) {
          if (Attempt == MaxRetries) {
            spdlog::error("{} - Connection lost, no retries left: {}", OpName, Err.what());
            return std::unexpected(core::Error{Err.what()});
          }
          auto Delay = backoffDelay(Attempt);
          spdlog::warn(
              "{} - Connection lost (attempt {}/{}), retrying in {}s: {}",
              OpName, Attempt + 1, MaxRetries, Delay.count(), Err.what()
          );
          std::this_thread::sleep_for(Delay);
          reconnect();
          continue;
        }

        // Permanent error (SQL error, constraint violation, etc.) — don't retry
        spdlog::error("{} - Failed: {}", OpName, Err.what());
        return std::unexpected(core::Error{Err.what()});
      }
    }
    return std::unexpected(core::Error{"Exhausted retries"});
  }

public:

  std::expected<void, core::Error> recordTaskRun(std::string_view TaskName) {
    return withRetry("Database::recordTaskRun", [this, TaskName]() -> void {
      pqxx::work Tx(Cx);
      static constexpr std::string_view Query =
          "INSERT INTO task_runs (task_name, last_run_at) VALUES ($1, NOW()) "
          "ON CONFLICT (task_name) "
          "DO UPDATE SET last_run_at = EXCLUDED.last_run_at";
      Tx.exec(pqxx::zview{Query}, pqxx::params{TaskName});
      Tx.commit();
    });
  }

  std::expected<std::optional<long long>, core::Error>
  querySecondsUntilNextRun(std::string_view TaskName) {
    return withRetry("Database::querySecondsUntilNextRun", [this, TaskName]() -> std::optional<long long> {
      pqxx::work Tx(Cx);
      static constexpr std::string_view Query =
          "SELECT EXTRACT(EPOCH FROM (next_run_at - NOW()))::bigint "
          "FROM task_runs WHERE task_name = $1";
      auto Res = Tx.exec(pqxx::zview{Query}, pqxx::params{TaskName});
      if (Res.empty()) return std::nullopt;
      return Res[0][0].as<long long>();
    });
  }

  template <core::DbEntity T>
  std::expected<T, core::Error> create(const T &Entity) {
    return withRetry("Database::create", [this, &Entity]() -> T {
      spdlog::trace(
          "Database::create<{}> - Starting transaction",
          core::DbTraits<T>::TableName
      );
      pqxx::work Tx(Cx);
      auto Params = core::DbTraits<T>::toParams(Entity);

      auto PlaceHolders = []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (
            std::string{} + ... +
            (Is == 0 ? "$1" : ", $" + std::to_string(Is + 1))
        );
      }(std::make_index_sequence<std::tuple_size_v<decltype(Params)>>{});

      auto Query = std::format(
          "INSERT INTO {} ({}) VALUES ({}) RETURNING *",
          core::DbTraits<T>::TableName,
          core::DbTraits<T>::Columns,
          PlaceHolders
      );

      spdlog::trace(
          "Database::create<{}> - Query: {}",
          core::DbTraits<T>::TableName,
          Query
      );

      pqxx::result Res;
      std::apply(
          [&](auto &&...Args) {
            Res = Tx.exec(pqxx::zview{Query}, pqxx::params{Args...});
          },
          Params
      );

      Tx.commit();
      spdlog::trace(
          "Database::create<{}> - Successfully created entity",
          core::DbTraits<T>::TableName
      );

      return core::DbTraits<T>::fromRow(Res[0]);
    });
  }

  template <core::DbEntity T>
  std::expected<T, core::Error> get(std::string_view Id) {
    return withRetry("Database::get", [this, Id]() -> T {
      spdlog::trace(
          "Database::get<{}> - Fetching entity with ID: {}",
          core::DbTraits<T>::TableName,
          Id
      );
      pqxx::work Tx(Cx);

      auto Query = std::format(
          "SELECT * FROM {} WHERE id = $1", core::DbTraits<T>::TableName
      );

      auto Result = Tx.exec(pqxx::zview{Query}, pqxx::params{Id});

      if (Result.empty()) {
        spdlog::debug(
            "Database::get<{}> - Entity not found: {}",
            core::DbTraits<T>::TableName,
            Id
        );
        throw std::runtime_error("Not found");
      }

      spdlog::trace(
          "Database::get<{}> - Successfully retrieved entity",
          core::DbTraits<T>::TableName
      );
      return core::DbTraits<T>::fromRow(Result[0]);
    });
  }

  template <core::DbEntity T>
  std::expected<T, core::Error> remove(std::string_view Id) {
    return withRetry("Database::remove", [this, Id]() -> T {
      spdlog::trace(
          "Database::remove<{}> - Soft deleting entity with ID: {}",
          core::DbTraits<T>::TableName,
          Id
      );
      pqxx::work Tx(Cx);

      auto Query = std::format(
          "UPDATE {} SET deleted_at = NOW() WHERE id = $1 RETURNING *",
          core::DbTraits<T>::TableName
      );

      auto Res = Tx.exec(pqxx::zview(Query), pqxx::params{Id});

      Tx.commit();
      spdlog::trace(
          "Database::remove<{}> - Successfully soft deleted entity",
          core::DbTraits<T>::TableName
      );
      return core::DbTraits<T>::fromRow(Res[0]);
    });
  }

  template <core::DbEntity T>
  std::expected<T, core::Error> update(const T &Entity) {
    return withRetry("Database::update", [this, &Entity]() -> T {
      spdlog::trace(
          "Database::update<{}> - Updating entity with ID: {}",
          core::DbTraits<T>::TableName,
          Entity.Id
      );
      pqxx::work Tx(Cx);
      auto Params = core::DbTraits<T>::toParams(Entity);

      auto Query = std::format(
          "UPDATE {} SET {}, updated_at = NOW() WHERE id = ${} RETURNING *",
          core::DbTraits<T>::TableName,
          core::DbTraits<T>::UpdateSet,
          std::tuple_size_v<decltype(Params)> + 1
      );

      pqxx::result Res;
      std::apply(
          [&](auto &&...Args) {
            Res = Tx.exec(pqxx::zview{Query}, pqxx::params{Args..., Entity.Id});
          },
          Params
      );

      Tx.commit();
      spdlog::trace(
          "Database::update<{}> - Successfully updated entity",
          core::DbTraits<T>::TableName
      );
      return core::DbTraits<T>::fromRow(Res[0]);
    });
  }

  template <core::DbEntity T>
  std::expected<std::vector<T>, core::Error> getAll() {
    return withRetry("Database::getAll", [this]() -> std::vector<T> {
      spdlog::trace(
          "Database::getAll<{}> - Fetching all entities",
          core::DbTraits<T>::TableName
      );
      pqxx::work Tx(Cx);
      auto Query =
          std::format("SELECT * FROM {}", core::DbTraits<T>::TableName);

      auto Res = Tx.exec(pqxx::zview(Query));

      std::vector<T> Results;
      for (const auto &Row : Res) {
        Results.push_back(core::DbTraits<T>::fromRow(Row));
      }

      spdlog::trace(
          "Database::getAll<{}> - Successfully retrieved {} entities",
          core::DbTraits<T>::TableName,
          Results.size()
      );
      return Results;
    });
  }
};
} // namespace insights::db
