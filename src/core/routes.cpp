#include "insights/core/routes.hpp"

#include <pqxx/pqxx>
#include <spdlog/spdlog.h>

namespace insights::core {

void registerCoreRoutes(
    glz::http_router &Router, std::shared_ptr<db::Database> Database
) {
  // Healthcheck endpoint
  Router.get(
      "/health", [Database](const glz::request &, glz::response &Response) {
        spdlog::debug("GET /health - Running healthcheck");

        // Test database connectivity
        try {
          pqxx::work Tx(Database->Cx);
          Tx.exec("SELECT 1");

          spdlog::debug("GET /health - Database connection healthy");
          Response.status(200).json(
              {{"status", "healthy"}, {"database", "connected"}}
          );
        } catch (const std::exception &Err) {
          spdlog::error(
              "GET /health - Database connection failed: {}", Err.what()
          );
          Response.status(503).json(
              {{"status", "unhealthy"},
               {"database", "disconnected"},
               {"error", Err.what()}}
          );
        }
      }
  );

  // Routes documentation endpoint
  Router.get("/routes", [](const glz::request &, glz::response &Response) {
    spdlog::debug("GET /routes - Listing all endpoints");
    Response.status(200).json(
        {{"service", "ICICLE Insights API"},
         {"version", "1.0.0"},
         {"endpoints",
          {{{"path", "/health"},
            {"method", "GET"},
            {"description",
             "Health check endpoint - verifies database connectivity"}},
           {{"path", "/routes"},
            {"method", "GET"},
            {"description", "Lists all available API endpoints"}},
           {{"path", "/api/github/accounts"},
            {"method", "GET"},
            {"description", "Get all github accounts"}},
           {{"path", "/api/github/accounts"},
            {"method", "POST"},
            {"description", "Create a new github account"}},
           {{"path", "/api/github/accounts/:id"},
            {"method", "GET"},
            {"description", "Get a specific github account by ID"}},
           {{"path", "/api/github/accounts/:id"},
            {"method", "DELETE"},
            {"description", "Soft delete a github account by ID"}},
           {{"path", "/api/github/repos"},
            {"method", "GET"},
            {"description", "Get all github repositories"}},
           {{"path", "/api/github/repos"},
            {"method", "POST"},
            {"description", "Create a new github repository"}},
           {{"path", "/api/github/repos/:id"},
            {"method", "GET"},
            {"description", "Get a specific github repository by ID"}},
           {{"path", "/api/github/repos/:id"},
            {"method", "PATCH"},
            {"description", "Update a github repository by ID"}},
           {{"path", "/api/github/repos/:id"},
            {"method", "DELETE"},
            {"description", "Soft delete a github repository by ID"}}}}}
    );
  });
}

} // namespace insights::core
