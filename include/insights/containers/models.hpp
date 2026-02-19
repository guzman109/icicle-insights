// #pragma once
// #include <chrono>
// #include <optional>
// #include <string>
//
// namespace insights::containers::models {
//
// using Timestamp = std::chrono::system_clock::time_point;
//
// struct Platform {
//   std::string id;
//   std::string name;
//   int64_t pulls{0};
//   int32_t stars{0};
//   Timestamp created_at;
//   Timestamp updated_at;
//   std::optional<Timestamp> deleted_at;
// };
//
// struct Account {
//   std::string id;
//   std::string name;
//   std::string platform_id;
//   int64_t pulls{0};
//   int32_t stars{0};
//   Timestamp created_at;
//   Timestamp updated_at;
//   std::optional<Timestamp> deleted_at;
// };
//
// struct Repository {
//   std::string id;
//   std::string name;
//   std::string account_id;
//   int64_t pulls{0};
//   int32_t stars{0};
//   Timestamp created_at;
//   Timestamp updated_at;
//   std::optional<Timestamp> deleted_at;
// };
// } // namespace insights::containers::models
