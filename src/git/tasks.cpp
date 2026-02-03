#include "git/tasks.hpp"
#include "core/result.hpp"
#include "git/models.hpp"
#include "glaze/net/http_client.hpp"
#include <asio/ssl.hpp>
#include <expected>
#include <format>
#include <spdlog/spdlog.h>
#include <system_error>

namespace insights::git::tasks {
static std::expected<std::shared_ptr<glz::http_client>, core::Error> createClient(const core::Config &Config) {
  auto Client = std::make_shared<glz::http_client>();

  // Configure SSL to use system certificates
  Client->configure_ssl_context([&Config](auto &Ctx) {
    std::error_code Ec;

    // Try SSL_CERT_FILE from config if set
    if (Config.SslCertFile) {
      Ctx.load_verify_file(*Config.SslCertFile, Ec);
      if (Ec) {
        spdlog::warn(
            "Failed to load CA certificates from SSL_CERT_FILE ({}): {}",
            *Config.SslCertFile, Ec.message());
      }
    }

    // Fall back to default paths if not set or failed
    if (!Config.SslCertFile || Ec) {
      Ctx.set_default_verify_paths(Ec);
      if (Ec) {
        spdlog::warn("Failed to set default verify paths: {}", Ec.message());
        // Continue anyway - SSL verification will use system defaults
      }
    }

    Ctx.set_verify_mode(asio::ssl::verify_peer);
  });
  return Client;
}

std::expected<void, core::Error> updateRepositories(std::shared_ptr<glz::http_client> Client, std::string RepoId, std::string Token,
                                      db::Database &Database,
                                      const core::Config &Config) {
  // TODO: Fetch repository metrics from Git APIs
  // Will have to fetch from 
  // Repository
  // - /repos/<owner>/<repo>
  // -- Forks
  // -- Stars,
  // -- Subscribers
  // - /repos/<owner>/<repo>/traffic/clones
  // -- Clones
  // - /repos/<owner>/<repo>/traffic/views
  // -- Views
  // Account
  // - /orgs/<organization>
  // -- Followers

  std::unordered_map<std::string, std::string> Headers = {
      {"Accept", "application/vnd.github+json"},
      {"Authorization", std::format("Bearer {}", Token)},
      {"User-Agent", "Glaze"},
      {"X-GitHub-Api-Version", "2022-11-28"},
  };

  auto DbGetRepoRes = Database.get<git::models::Repository>(RepoId);
  if (!DbGetRepoRes) {
    return {};
  }

  auto Repository = DbGetRepoRes.value();

  auto DbGetAccountRes =
      Database.get<git::models::Account>(Repository.AccountId);
  if (!DbGetAccountRes) {
    return {};
  }

  auto Account = DbGetAccountRes.value();

  std::string Url = std::format("https://api.github.com/repos/{}/{}",
                                Account.Name, Repository.Name);

  // Validate client is initialized
  if (!Client) {
    spdlog::error("HTTP Client is null");
    return std::unexpected(core::Error{"Client initialization failed"});
  }

  // Log the request
  spdlog::debug("Making HTTP GET request to: {}", Url);

  auto Response = Client->get(Url, Headers);
  if (!Response) {
    std::string ErrorMsg = std::format("HTTP request failed: {}", Response.error().message());
    spdlog::error(ErrorMsg);
    return std::unexpected(core::Error{ErrorMsg});
  }

  spdlog::info("{}", Response->response_body);
  // TODO: Update repository records in database
  return {};
}

std::expected<void, core::Error> updateAccounts(db::Database &Database) {
  // TODO: Aggregate repository data to account level
  // TODO: Update account records in database
  return {};
}

std::expected<void, core::Error> updatePlatforms(db::Database &Database) {
  // TODO: Aggregate account/repository data to platform level
  // TODO: Update platform records in database
  return {};
}

std::expected<void, core::Error> runAll(std::string RepoId, std::string Token,
                          db::Database &Database, const core::Config &Config) {
  // Create HTTP client
  auto ClientResult = createClient(Config);
  if (!ClientResult) {
    return std::unexpected<core::Error>(ClientResult.error());
  }
  auto Client = ClientResult.value();

  // Run the pipeline in order: Repos → Accounts → Platforms
  auto RepoResult = updateRepositories(Client, RepoId, Token, Database, Config);
  if (!RepoResult) {
    return RepoResult;
  }

  auto AccountResult = updateAccounts(Database);
  if (!AccountResult) {
    return AccountResult;
  }

  return updatePlatforms(Database);
}

} // namespace insights::git::tasks
