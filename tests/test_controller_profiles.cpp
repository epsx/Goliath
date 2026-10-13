#include "input/controller_profiles.hpp"

#include <catch2/catch.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace fs = std::filesystem;

using goliath::ControllerProfileBindings;
using goliath::ControllerProfileStore;
using goliath::controller_profile_assigned_bindings;
using goliath::controller_profile_binding_count;
using goliath::controller_profile_device_name;
using goliath::controller_profile_sections;
using goliath::sanitize_controller_profile_bindings;

namespace {

fs::path make_controller_profile_root(const std::string& name) {
    const auto stamp = std::chrono::steady_clock::now()
                           .time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() /
        ("goliath_controller_profiles_" + name + "_" +
         std::to_string(stamp));
    fs::remove_all(root);
    fs::create_directories(root);
    return root;
}

void write_text(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.good());
    output << text;
    REQUIRE(output.good());
}

std::string read_text(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

ControllerProfileBindings joystick_bindings() {
    return {
        {"neogeojs1", {{"Up", "26"}, {"A", "j0b1"}, {"B", "j0b99"}}},
        {"neogeojs2", {{"Up", "82"}, {"A", "j1b0"}}},
        {"neogeosystem", {{"Coin1", "34"}}},
        {"unknown", {{"Anything", "1"}}},
    };
}

} // namespace

TEST_CASE("controller profile layouts have stable labels and sections",
          "[controller_profiles][input]") {
    CHECK(controller_profile_device_name(1) == "Neo Geo Joysticks");
    CHECK(controller_profile_device_name(5) == "Irritating Maze");
    CHECK(controller_profile_device_name(99) == "Unknown input layout");

    const std::vector<std::string> joystickSections = {
        "neogeosystem", "neogeojs1", "neogeojs2"};
    CHECK(controller_profile_sections(1) == joystickSections);
    CHECK(controller_profile_sections(3).size() == 5);
    CHECK(controller_profile_sections(99).empty());
}

TEST_CASE("controller profile snapshots are complete and validated",
          "[controller_profiles][input][validation]") {
    const ControllerProfileBindings sanitized =
        sanitize_controller_profile_bindings(1, joystick_bindings());

    REQUIRE(sanitized.size() == 3);
    CHECK(sanitized.find("unknown") == sanitized.end());
    REQUIRE(sanitized.at("neogeojs1").size() == 10);
    REQUIRE(sanitized.at("neogeojs2").size() == 10);
    REQUIRE(sanitized.at("neogeosystem").size() == 4);
    CHECK(sanitized.at("neogeojs1").at("Up") == "26");
    CHECK(sanitized.at("neogeojs1").at("A") == "j0b1");
    CHECK(sanitized.at("neogeojs1").at("B").empty());
    CHECK(sanitized.at("neogeojs1").at("Down").empty());
    CHECK(controller_profile_binding_count(sanitized) == 5);
}

TEST_CASE("controller profile names are unique and operations are scoped",
          "[controller_profiles][input][store]") {
    ControllerProfileStore store;
    std::string id;
    std::string error;

    REQUIRE(store.add("  Arcade Stick  ", 1, joystick_bindings(), &id, &error));
    CHECK(error.empty());
    REQUIRE(store.profiles().size() == 1);
    CHECK(store.profiles().front().name == "Arcade Stick");
    CHECK_FALSE(id.empty());

    CHECK_FALSE(store.add("arcade stick", 1, {}, nullptr, &error));
    CHECK(error.find("already exists") != std::string::npos);
    CHECK_FALSE(store.replace(id, 2, {}, &error));
    CHECK(error.find("does not match") != std::string::npos);

    REQUIRE(store.rename(id, "Player One", &error));
    REQUIRE(store.find(id) != nullptr);
    CHECK(store.find(id)->name == "Player One");
    CHECK(store.erase(id));
    CHECK_FALSE(store.erase(id));
    CHECK(store.profiles().empty());
}

TEST_CASE("controller profiles save and reload atomically",
          "[controller_profiles][input][persistence]") {
    const fs::path root = make_controller_profile_root("roundtrip");
    const fs::path path = root / "config/controller_profiles.json";
    ControllerProfileStore saved;
    std::string firstId;
    std::string secondId;
    std::string error;

    REQUIRE(saved.add(
        "Arcade Stick", 1, joystick_bindings(), &firstId, &error));
    REQUIRE(saved.add(
        "Trackball", 5,
        {{"neogeoirrmaze", {{"XAxis", "j0a0"}, {"YAxis", "j0a1"}}}},
        &secondId, &error));
    REQUIRE(saved.save(path, &error));
    CHECK(error.empty());

    const std::string document = read_text(path);
    CHECK(document.find("\"version\": 1") != std::string::npos);
    CHECK(document.find("Arcade Stick") != std::string::npos);
    CHECK(document.find("j0b99") == std::string::npos);
    CHECK(document.find("unknown") == std::string::npos);

    ControllerProfileStore loaded;
    REQUIRE(loaded.load(path, &error));
    CHECK(error.empty());
    REQUIRE(loaded.profiles().size() == 2);
    REQUIRE(loaded.find(firstId) != nullptr);
    REQUIRE(loaded.find(secondId) != nullptr);
    CHECK(loaded.find(firstId)->bindings.at("neogeojs1").at("B").empty());
    CHECK(loaded.find(secondId)->bindings.at("neogeoirrmaze").at("XAxis") ==
          "j0a0");
}

TEST_CASE("missing controller profile storage is a valid empty store",
          "[controller_profiles][input][persistence]") {
    const fs::path root = make_controller_profile_root("missing");
    ControllerProfileStore store;
    std::string error;

    CHECK(store.load(root / "controller_profiles.json", &error));
    CHECK(error.empty());
    CHECK(store.profiles().empty());
}

TEST_CASE("controller profile assigned bindings are safe per-game overrides",
          "[controller_profiles][input][per_game]") {
    const ControllerProfileBindings assigned =
        controller_profile_assigned_bindings(1, joystick_bindings());

    REQUIRE(assigned.contains("neogeojs1"));
    CHECK(assigned.at("neogeojs1").at("A") == "j0b1");
    CHECK_FALSE(assigned.at("neogeojs1").contains("B"));
    CHECK_FALSE(assigned.contains("unknown"));
}

TEST_CASE("malformed controller profile storage fails safely",
          "[controller_profiles][input][persistence]") {
    const fs::path root = make_controller_profile_root("malformed");
    const fs::path path = root / "controller_profiles.json";
    write_text(path, "{\"version\":99,\"profiles\":[]}");

    ControllerProfileStore store;
    std::string error;
    CHECK_FALSE(store.load(path, &error));
    CHECK_FALSE(error.empty());
    CHECK(store.profiles().empty());
    CHECK(read_text(path) == "{\"version\":99,\"profiles\":[]}");
}
