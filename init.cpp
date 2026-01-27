#include <cstddef>
#include <fstream>
#include <iostream>
#include <vector>

#include "core/config.hpp"
#include "core/result.hpp"
#include "db/db.hpp"
#include "git/git.hpp"
#include "git/models.hpp"
#include "glaze/glaze.hpp"
#include "spdlog/spdlog.h"

struct Component {
    std::string Name;
};

struct Data {
    std::vector<Component> Components;
};

void init(insights::db::Database& Db);

int main() {
    spdlog::info("ICICLE Insights!");
    spdlog::info("Getting Environment Variables.");
    auto Config = insights::core::Config::load();
    if (!Config) {
        std::cerr << Config.error().Message << "\n";
        return 1;
    }

    auto Database = insights::db::Database::connect(Config->DatabaseUrl);
    if (!Database) {
        std::cerr << Database.error().Message << "\n";
        return 1;
    }

    // init(Database.value());
    auto Account = Database->get<insights::git::models::Account>("20ad9106-7db1-4367-8efe-a794d3287e58");
    auto Repository =
        Database->get<insights::git::models::Repository>("1603208a-1352-4e99-a3ff-a40bb51c48a8");
    insights::git::fetch_repository_metrics(Config->GitHubToken, *Account, *Repository);

    return 0;
}

void init(insights::db::Database& Db) {
    insights::git::models::Platform Platform{.Name = "github"};

    auto Github = Db.create(Platform);
    if (!Github) {
        std::cerr << Github.error().Message << "\n";
        return;
    }

    std::cout << "Created platform with id: " << Github->Id << "\n";

    insights::git::models::Account Account{.Name = "icicle-ai", .PlatformId = Github->Id};

    auto Icicle = Db.create(Account);
    if (!Icicle) {
        std::cerr << Icicle.error().Message << "\n";
        return;
    }

    std::ifstream File("data/component_names.json");
    std::string Content((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());

    Data ComponentData;
    auto ParseResult = glz::read_json(ComponentData, Content);
    if (!ParseResult) {
        std::cerr << "Failed to parse JSON\n";
    }

    std::println("Creating Repositories");

    for (const Component &Comp : ComponentData.Components) {
        std::println("Component: {}", Comp.Name);
        insights::git::models::Repository Repository{
            .Name = Comp.Name, .AccountId = Icicle->Id
        };
        auto _ = Db.create(Repository);
    }
}
