#pragma once
#include "insights/db/db.hpp"

#include <glaze/net/http_router.hpp>
#include <memory>
#include <optional>
#include <string>

namespace insights::core {

struct TaskStatusResponse {
  std::string TaskName;
  std::optional<std::string> LastSuccessfulRunAt;
  std::optional<std::string> LastAttemptStartedAt;
  std::optional<std::string> LastAttemptFinishedAt;
  std::optional<std::string> LastAttemptStatus;
  std::optional<std::string> LastAttemptSummary;
  std::optional<std::string> NextRunAt;
  long long SecondsUntilNextRun{0};
  int LastAttemptRepositoriesProcessed{0};
  int LastAttemptRepositoriesFailed{0};
  int LastAttemptAccountsProcessed{0};
  int LastAttemptAccountsFailed{0};
};

void registerCoreRoutes(
    glz::http_router &Router, std::shared_ptr<db::Database> Database
);

} // namespace insights::core
