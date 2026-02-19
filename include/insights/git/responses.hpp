#pragma once
#include <string>

namespace insights::git::tasks::responses {
struct GitHubRepoStatsResponse {
  int stargazers_count;
  int forks_count;
  int subscribers_count;
};

struct GitHubRepoTrafficResponse {
  int count;
};
struct GitHubOrgStatsResponse {
  int followers;
};
struct GitHubRepoRefferersResponse {
  std::string referrer;
  int count;
  int unique;
};

} // namespace insights::git::tasks::responses
