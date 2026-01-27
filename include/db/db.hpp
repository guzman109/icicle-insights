#pragma once
#include <format>
#include <memory>
#include <pqxx/pqxx>
#include <pqxx/zview>
#include <string>
#include <exception>
#include <expected>
#include <utility>

#include "core/result.hpp"
#include "core/traits.hpp"

namespace insights::db {
struct Database {
  pqxx::connection Cx;

  static core::Result<std::shared_ptr<Database>>
  connect(const std::string &ConnString) {
    try {
      return std::shared_ptr<Database>(
          new Database{pqxx::connection(ConnString.c_str())});
    } catch (const std::exception &Err) {
      return std::unexpected(core::Error{Err.what()});
    }
  }

  template <core::DbEntity T> core::Result<T> create(const T &Entity) {
    try {
      pqxx::work Tx(Cx);
      auto Params = core::DbTraits<T>::toParams(Entity);

      auto PlaceHolders = []<std::size_t... Is>(std::index_sequence<Is...>) {
        return (std::string{} + ... +
                (Is == 0 ? "$1" : ", $" + std::to_string(Is + 1)));
      }(std::make_index_sequence<std::tuple_size_v<decltype(Params)>>{});

      auto Query = std::format("INSERT INTO {} ({}) VALUES ({}) RETURNING *",
                               core::DbTraits<T>::TableName,
                               core::DbTraits<T>::Columns, PlaceHolders);

      pqxx::result Res;
      std::apply(
          [&](auto &&...Args) {
            Res = Tx.exec(pqxx::zview{Query}, pqxx::params{Args...});
          },
          Params);

      Tx.commit();

      return core::DbTraits<T>::fromRow(Res[0]);
    } catch (const std::exception &Err) {
      return std::unexpected(core::Error{Err.what()});
    }
  }

  template <core::DbEntity T> core::Result<T> get(const std::string &Id) {
    try {
      pqxx::work Tx(Cx);

      auto Query = std::format("SELECT * FROM {} WHERE id = $1",
                               core::DbTraits<T>::TableName);

      auto Result = Tx.exec(pqxx::zview{Query}, pqxx::params{Id});

      if (Result.empty()) {
        return std::unexpected(core::Error{"Not found"});
      }

      return core::DbTraits<T>::fromRow(Result[0]);
    } catch (const std::exception &Err) {
      return std::unexpected(core::Error{Err.what()});
    }
  }

  template <core::DbEntity T> core::Result<T> remove(const std::string &Id) {
    try {
      pqxx::work Tx(Cx);

      auto Query = std::format(
          "UPDATE {} SET deleted_at = NOW() WHERE id = $1 RETURNING *",
          core::DbTraits<T>::TableName);

      auto Res = Tx.exec(pqxx::zview(Query), pqxx::params{Id});

      Tx.commit();
      return core::DbTraits<T>::fromRow(Res[0]);
    } catch (const std::exception &Err) {
      return std::unexpected(core::Error{Err.what()});
    }
  }

  template <core::DbEntity T> core::Result<T> update(const T &Entity) {
    try {
      pqxx::work Tx(Cx);
      auto Params = core::DbTraits<T>::toParams(Entity);

      auto Query = std::format(
          "UPDATE {} SET {}, updated_at = NOW() WHERE id = ${} RETURNING *",
          core::DbTraits<T>::TableName, core::DbTraits<T>::UpdateSet,
          std::tuple_size_v<decltype(Params)> + 1);

      pqxx::result Res;
      std::apply(
          [&](auto &&...Args) {
            Res = Tx.exec(pqxx::zview{Query}, pqxx::params{Args..., Entity.Id});
          },
          Params);

      Tx.commit();
      return core::DbTraits<T>::fromRow(Res[0]);
    } catch (const std::exception &Err) {
      return std::unexpected(core::Error{Err.what()});
    }
  }

  template <core::DbEntity T> core::Result<std::vector<T>> getAll() {
    try {
      pqxx::work Tx(Cx);
      auto Query =
          std::format("SELECT * FROM {}", core::DbTraits<T>::TableName);

      auto Res = Tx.exec(pqxx::zview(Query));

      if (Res.empty()) {
        return std::unexpected(core::Error{"Not found"});
      }

      std::vector<T> Results;
      for (const auto &Row : Res) {
        Results.push_back(core::DbTraits<T>::fromRow(Row));
      }

      return Results;

    } catch (const std::exception &Err) {
      return std::unexpected(core::Error{Err.what()});
    }
  }
};
} // namespace insights::db
