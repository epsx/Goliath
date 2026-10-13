// controller_profiles.hpp — named, reusable snapshots of one complete
// Settings -> Input layout. Profiles are Goliath-owned and validated before
// they can be persisted or applied to the editor.
#pragma once

#include <filesystem>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace goliath {

using ControllerProfileBindings =
    std::map<std::string, std::map<std::string, std::string>>;

struct ControllerProfile {
    std::string id;
    std::string name;
    int device = 1;
    ControllerProfileBindings bindings;
};

std::string controller_profile_device_name(int device);
std::vector<std::string> controller_profile_sections(int device);
std::size_t controller_profile_binding_count(
    const ControllerProfileBindings& bindings);

// Returns a complete, canonical snapshot for the requested input layout.
// Missing definitions are represented by an empty binding. Unknown sections,
// keys, and malformed non-empty values are discarded.
ControllerProfileBindings sanitize_controller_profile_bindings(
    int device,
    const ControllerProfileBindings& bindings);

// Produces the validated non-empty bindings that can be layered as per-game
// overrides. Empty entries in a complete stored snapshot mean "inherit" in
// the exact-media editor and are therefore omitted.
ControllerProfileBindings controller_profile_assigned_bindings(
    int device,
    const ControllerProfileBindings& bindings);

class ControllerProfileStore {
public:
    bool load(const std::filesystem::path& path,
              std::string* error = nullptr);
    bool save(const std::filesystem::path& path,
              std::string* error = nullptr) const;

    const std::vector<ControllerProfile>& profiles() const noexcept;
    const ControllerProfile* find(const std::string& id) const;

    bool add(std::string name,
             int device,
             ControllerProfileBindings bindings,
             std::string* newId = nullptr,
             std::string* error = nullptr);
    bool replace(const std::string& id,
                 int device,
                 ControllerProfileBindings bindings,
                 std::string* error = nullptr);
    bool rename(const std::string& id,
                std::string name,
                std::string* error = nullptr);
    bool erase(const std::string& id);

private:
    bool nameAvailable(const std::string& name,
                       const std::string& ignoredId = {}) const;

    std::vector<ControllerProfile> m_profiles;
};

} // namespace goliath
