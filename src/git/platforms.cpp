#include "core/http.hpp"
#include "core/result.hpp"
#include "db/db.hpp"
#include "git/models.hpp"
#include "git/router.hpp"
#include "glaze/net/http_router.hpp"
#include "server/dependencies.hpp"
#include <memory>
#include <string>
#include <vector>

namespace insights::git {

core::Result<void>
registerPlatformsRoutes(glz::http_router &Router,
                        std::shared_ptr<db::Database> &Database) {

  // Platform
  // Get All Platforms

  using enum core::HttpStatus;

  Router.get("/platforms",
             [Database](const glz::request &Request, glz::response &Response) {
               // Get all instances
               auto DbResponse = Database->getAll<git::models::Platform>();

               if (!DbResponse) {
                 Response.status(static_cast<int>(InternalServerError))
                     .json({"Error", "Error retrieving data from database."});
                 return;
               }

               // Structure data for output.
               std::vector<git::OutputPlatformSchema> Output;
               for (const auto &Platform : DbResponse.value()) {
                 Output.emplace_back(OutputPlatformSchema{
                     .Id = Platform.Id,
                     .Name = Platform.Name,
                     .Clones = Platform.Clones,
                     .Followers = Platform.Followers,
                     .Forks = Platform.Forks,
                     .Stars = Platform.Stars,
                     .Views = Platform.Views,
                     .Watchers = Platform.Watchers,
                 });
               }

               Response.status(static_cast<int>(Ok)).json(Output);
             });

  // Create Platform
  Router.post("/platforms", [Database](const glz::request &Request,
                                       glz::response &Response) {
    CreatePlatformSchema PlatformData;
    if (auto JsonError = glz::read_json(PlatformData, Request.body)) {
      Response.status(static_cast<int>(BadRequest))
          .json({{"error", "Invalid JSON"}});
      return;
    }
    std::ranges::transform(PlatformData.Name, PlatformData.Name.begin(),
                           [](unsigned char Ch) { return std::tolower(Ch); });
    git::models::Platform PlatformToCreate{.Name = PlatformData.Name,
                                           .Clones = PlatformData.Clones,
                                           .Followers = PlatformData.Followers,
                                           .Forks = PlatformData.Forks,
                                           .Stars = PlatformData.Stars,
                                           .Views = PlatformData.Views,
                                           .Watchers = PlatformData.Watchers};

    auto DbResponse = Database->create(PlatformToCreate);
    if (!DbResponse) {
      Response.status(static_cast<int>(InternalServerError))
          .json({"Error", "Error commiting data to database."});
      return;
    }
    auto Output = OutputPlatformSchema{
        .Id = DbResponse.value().Id,
        .Name = DbResponse.value().Name,
        .Clones = DbResponse.value().Clones,
        .Followers = DbResponse.value().Followers,
        .Forks = DbResponse.value().Forks,
        .Stars = DbResponse.value().Stars,
        .Views = DbResponse.value().Views,
        .Watchers = DbResponse.value().Watchers,
    };
    Response.status(static_cast<int>(Created)).json(Output);
  });

  Router.get("/platforms/:id",
             [Database](const glz::request &Request, glz::response &Response) {
               auto Id = Request.params.at("id");
               auto DbResponse = Database->get<git::models::Platform>(Id);
               if (!DbResponse) {
                 Response.status(static_cast<int>(InternalServerError))
                     .json({"Error", "Error retrieving data from database."});
                 return;
               }
               auto Output = OutputPlatformSchema{
                   .Id = DbResponse.value().Id,
                   .Name = DbResponse.value().Name,
                   .Clones = DbResponse.value().Clones,
                   .Followers = DbResponse.value().Followers,
                   .Forks = DbResponse.value().Forks,
                   .Stars = DbResponse.value().Stars,
                   .Views = DbResponse.value().Views,
                   .Watchers = DbResponse.value().Watchers,
               };

               Response.status(static_cast<int>(Ok)).json(Output);
             },
             {.constraints = {{"id", server::dependencies::uuidConstraint()}}});

  Router.patch(
      "/platforms/:id",
      [Database](const glz::request &Request, glz::response &Response) {
        UpdateSchema PlatformData;
        if (auto JsonError = glz::read_json(PlatformData, Request.body)) {
          Response.status(static_cast<int>(BadRequest))
              .json({{"error", "Invalid JSON"}});
          return;
        }

        auto Id = Request.params.at("id");
        auto DbResponse = Database->get<git::models::Platform>(Id);
        if (!DbResponse) {
          Response.status(static_cast<int>(InternalServerError))
              .json({"Error", "Error retrieving data from database."});
          return;
        }

        // Update values for platform.
        auto Platform = DbResponse.value();
        Platform.Clones = PlatformData.Clones;
        Platform.Followers = PlatformData.Followers;
        Platform.Forks = PlatformData.Forks;
        Platform.Stars = PlatformData.Stars;
        Platform.Views = PlatformData.Views;
        Platform.Watchers = PlatformData.Watchers;

        // Commit changes
        DbResponse = Database->update(Platform);
        if (!DbResponse) {
          Response.status(static_cast<int>(InternalServerError))
              .json({"Error", "Error retrieving data from database."});
          return;
        }

        auto Output = OutputPlatformSchema{
            .Id = DbResponse.value().Id,
            .Name = DbResponse.value().Name,
            .Clones = DbResponse.value().Clones,
            .Followers = DbResponse.value().Followers,
            .Forks = DbResponse.value().Forks,
            .Stars = DbResponse.value().Stars,
            .Views = DbResponse.value().Views,
            .Watchers = DbResponse.value().Watchers,
        };
        Response.status(static_cast<int>(Ok)).json(Output);
      },
      {.constraints = {{"id", server::dependencies::uuidConstraint()}}});

  Router.del("/platforms/:id",
             [Database](const glz::request &Request, glz::response &Response) {
               auto Id = Request.params.at("id");
               auto DbResponse = Database->remove<git::models::Platform>(Id);
               if (!DbResponse) {
                 Response.status(static_cast<int>(InternalServerError))
                     .json({"Error", "Error deleting data from database."});
                 return;
               }

               auto Output = OutputPlatformSchema{
                   .Id = DbResponse.value().Id,
                   .Name = DbResponse.value().Name,
                   .Clones = DbResponse.value().Clones,
                   .Followers = DbResponse.value().Followers,
                   .Forks = DbResponse.value().Forks,
                   .Stars = DbResponse.value().Stars,
                   .Views = DbResponse.value().Views,
                   .Watchers = DbResponse.value().Watchers,
               };
               Response.status(static_cast<int>(Ok)).json(Output);
             },
             {.constraints = {{"id", server::dependencies::uuidConstraint()}}});

  return {};
}
} // namespace insights::git
