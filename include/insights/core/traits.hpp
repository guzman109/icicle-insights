#pragma once
#include <concepts>
#include <pqxx/pqxx>
#include <string_view>

namespace insights::core {
template <typename T> struct DbTraits;

template <typename T>
concept DbEntity = requires(T t) {
  { DbTraits<T>::TableName } -> std::convertible_to<std::string_view>;
  { DbTraits<T>::Columns } -> std::convertible_to<std::string_view>;
  { DbTraits<T>::UpdateSet } -> std::convertible_to<std::string_view>;
  { DbTraits<T>::toParams(t) };
  { DbTraits<T>::fromRow(std::declval<pqxx::row>()) } -> std::same_as<T>;
};
} // namespace insights::core
