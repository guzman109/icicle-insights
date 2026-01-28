#include "server/routes.hpp"
#include <pqxx/pqxx>
#include <spdlog/spdlog.h>

namespace insights::server {

void registerCoreRoutes(glz::http_router &Router,
                        std::shared_ptr<db::Database> Database) {
  // Healthcheck endpoint
  Router.get("/health", [Database](const glz::request &, glz::response &Response) {
    spdlog::debug("GET /health - Running healthcheck");

    // Test database connectivity
    try {
      pqxx::work Tx(Database->Cx);
      Tx.exec("SELECT 1");

      spdlog::debug("GET /health - Database connection healthy");
      Response.status(200).json({
        {"status", "healthy"},
        {"database", "connected"}
      });
    } catch (const std::exception &Err) {
      spdlog::error("GET /health - Database connection failed: {}", Err.what());
      Response.status(503).json({
        {"status", "unhealthy"},
        {"database", "disconnected"},
        {"error", Err.what()}
      });
    }
  });

  // Routes documentation endpoint
  Router.get("/routes", [](const glz::request &, glz::response &Response) {
    spdlog::debug("GET /routes - Listing all endpoints");
    Response.status(200).json({
      {"service", "ICICLE Insights API"},
      {"version", "1.0.0"},
      {"endpoints", {
        {
          {"path", "/health"},
          {"method", "GET"},
          {"description", "Health check endpoint - verifies database connectivity"}
        },
        {
          {"path", "/routes"},
          {"method", "GET"},
          {"description", "Lists all available API endpoints"}
        },
        {
          {"path", "/api/git/platforms"},
          {"method", "GET"},
          {"description", "Get all git platforms"}
        },
        {
          {"path", "/api/git/platforms"},
          {"method", "POST"},
          {"description", "Create a new git platform"}
        },
        {
          {"path", "/api/git/platforms/:id"},
          {"method", "GET"},
          {"description", "Get a specific git platform by ID"}
        },
        {
          {"path", "/api/git/platforms/:id"},
          {"method", "PATCH"},
          {"description", "Update a git platform by ID"}
        },
        {
          {"path", "/api/git/platforms/:id"},
          {"method", "DELETE"},
          {"description", "Soft delete a git platform by ID"}
        },
        {
          {"path", "/api/git/accounts"},
          {"method", "GET"},
          {"description", "Get all git accounts"}
        },
        {
          {"path", "/api/git/accounts"},
          {"method", "POST"},
          {"description", "Create a new git account"}
        },
        {
          {"path", "/api/git/accounts/:id"},
          {"method", "GET"},
          {"description", "Get a specific git account by ID"}
        },
        {
          {"path", "/api/git/accounts/:id"},
          {"method", "PATCH"},
          {"description", "Update a git account by ID"}
        },
        {
          {"path", "/api/git/accounts/:id"},
          {"method", "DELETE"},
          {"description", "Soft delete a git account by ID"}
        },
        {
          {"path", "/api/git/repos"},
          {"method", "GET"},
          {"description", "Get all git repositories"}
        },
        {
          {"path", "/api/git/repos"},
          {"method", "POST"},
          {"description", "Create a new git repository"}
        },
        {
          {"path", "/api/git/repos/:id"},
          {"method", "GET"},
          {"description", "Get a specific git repository by ID"}
        },
        {
          {"path", "/api/git/repos/:id"},
          {"method", "PATCH"},
          {"description", "Update a git repository by ID"}
        },
        {
          {"path", "/api/git/repos/:id"},
          {"method", "DELETE"},
          {"description", "Soft delete a git repository by ID"}
        }
      }}
    });
  });
}

} // namespace insights::server
