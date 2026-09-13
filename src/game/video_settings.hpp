// video_settings.hpp — authoritative JGRF and Geolith video setting schema.
// Both the global Settings tab and exact-media profiles consume these
// definitions so keys, ranges, defaults and option labels cannot drift.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace goliath {

enum class VideoSettingKind { Choice, Boolean, Integer };

struct VideoSettingOption {
    int value;
    const char* label;
};

struct VideoSettingSpec {
    const char* key;
    const char* label;
    int default_value;
    VideoSettingKind kind;
    std::vector<VideoSettingOption> options; // Choice only
    int minimum = 0;                         // Boolean/Integer
    int maximum = 0;                         // Boolean/Integer
};

using VideoSettingValues = std::map<std::string, int>;

const std::vector<VideoSettingSpec>& jgrf_video_setting_specs();
const std::vector<VideoSettingSpec>& geolith_video_setting_specs();

const VideoSettingSpec* find_video_setting(
    const std::vector<VideoSettingSpec>& specs,
    std::string_view key);

bool is_valid_video_setting_value(const VideoSettingSpec& spec, int value);
int normalize_video_setting_value(const VideoSettingSpec& spec, int value);

VideoSettingValues default_video_setting_values(
    const std::vector<VideoSettingSpec>& specs);
VideoSettingValues sanitize_video_setting_values(
    const VideoSettingValues& values,
    const std::vector<VideoSettingSpec>& specs);

int video_setting_value(const VideoSettingValues& values,
                        const VideoSettingSpec& spec);
std::optional<int> video_setting_override(const VideoSettingValues& values,
                                          const VideoSettingSpec& spec);

} // namespace goliath
