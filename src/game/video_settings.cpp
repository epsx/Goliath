#include "game/video_settings.hpp"

namespace goliath {

const std::vector<VideoSettingSpec>& jgrf_video_setting_specs() {
    static const std::vector<VideoSettingSpec> specs = {
        {"api", "Video API", 0, VideoSettingKind::Choice,
         {{0, "OpenGL Core"},
          {1, "OpenGL ES"},
          {2, "OpenGL Compatibility"},
          {3, "Vulkan (Experimental)"}}},
        {"fullscreen", "Fullscreen", 0, VideoSettingKind::Boolean, {}, 0, 1},
        {"scale", "Scale (Initial)", 3, VideoSettingKind::Integer, {}, 1, 8},
        {"shader", "Shader", 2, VideoSettingKind::Choice,
         {{0, "None / Nearest Neighbour"},
          {1, "Linear"},
          {2, "Sharp Bilinear"},
          {3, "Anti-Aliased Nearest Neighbour"},
          {4, "CRT-Yee64"},
          {5, "CRTea"},
          {6, "LCD"}}},
        {"crtea_mode", "CRTea Mode", 2, VideoSettingKind::Choice,
         {{0, "Scanlines"},
          {1, "Aperture Grille Lite"},
          {2, "Aperture Grille"},
          {3, "Shadow Mask"},
          {4, "Custom"}}},
        {"crtea_masktype", "CRTea Mask Type", 2, VideoSettingKind::Choice,
         {{0, "No Mask"},
          {1, "Aperture Grille Lite"},
          {2, "Aperture Grille"},
          {3, "Shadow Mask"}}},
        {"crtea_maskstr", "CRTea Mask Strength", 5,
         VideoSettingKind::Integer, {}, 0, 10},
        {"crtea_scanstr", "CRTea Scanline Strength", 6,
         VideoSettingKind::Integer, {}, 0, 10},
        {"crtea_sharpness", "CRTea Sharpness", 7,
         VideoSettingKind::Integer, {}, 0, 10},
        {"crtea_curve", "CRTea Curve", 2,
         VideoSettingKind::Integer, {}, 0, 10},
        {"crtea_corner", "CRTea Corner", 3,
         VideoSettingKind::Integer, {}, 0, 10},
        {"crtea_tcurve", "CRTea Trinitron Curve", 10,
         VideoSettingKind::Integer, {}, 0, 10},
    };
    return specs;
}

const std::vector<VideoSettingSpec>& geolith_video_setting_specs() {
    static const std::vector<VideoSettingOption> overscan = {
        {0, "0"}, {1, "4"}, {2, "8"}, {3, "12"}, {4, "16"}};
    static const std::vector<VideoSettingSpec> specs = {
        {"aspect", "Aspect Ratio", 0, VideoSettingKind::Choice,
         {{0, "1:1 PAR"}, {1, "45:44 PAR"}, {2, "4:3 DAR"}}},
        {"palette", "Palette", 0, VideoSettingKind::Choice,
         {{0, "Resistor Network"}, {1, "Raw"}}},
        {"overscan_t", "Mask Overscan (Top)", 2,
         VideoSettingKind::Choice, overscan},
        {"overscan_b", "Mask Overscan (Bottom)", 2,
         VideoSettingKind::Choice, overscan},
        {"overscan_l", "Mask Overscan (Left)", 2,
         VideoSettingKind::Choice, overscan},
        {"overscan_r", "Mask Overscan (Right)", 2,
         VideoSettingKind::Choice, overscan},
    };
    return specs;
}

const VideoSettingSpec* find_video_setting(
        const std::vector<VideoSettingSpec>& specs,
        std::string_view key) {
    for (const VideoSettingSpec& spec : specs) {
        if (key == spec.key) return &spec;
    }
    return nullptr;
}

bool is_valid_video_setting_value(const VideoSettingSpec& spec, int value) {
    if (spec.kind == VideoSettingKind::Choice) {
        for (const VideoSettingOption& option : spec.options) {
            if (option.value == value) return true;
        }
        return false;
    }
    return value >= spec.minimum && value <= spec.maximum;
}

int normalize_video_setting_value(const VideoSettingSpec& spec, int value) {
    return is_valid_video_setting_value(spec, value)
               ? value
               : spec.default_value;
}

VideoSettingValues default_video_setting_values(
        const std::vector<VideoSettingSpec>& specs) {
    VideoSettingValues result;
    for (const VideoSettingSpec& spec : specs)
        result.emplace(spec.key, spec.default_value);
    return result;
}

VideoSettingValues sanitize_video_setting_values(
        const VideoSettingValues& values,
        const std::vector<VideoSettingSpec>& specs) {
    VideoSettingValues result;
    for (const auto& [key, value] : values) {
        const VideoSettingSpec* spec = find_video_setting(specs, key);
        if (spec && is_valid_video_setting_value(*spec, value))
            result.emplace(key, value);
    }
    return result;
}

int video_setting_value(const VideoSettingValues& values,
                        const VideoSettingSpec& spec) {
    const auto it = values.find(spec.key);
    if (it == values.end()) return spec.default_value;
    return normalize_video_setting_value(spec, it->second);
}

std::optional<int> video_setting_override(const VideoSettingValues& values,
                                          const VideoSettingSpec& spec) {
    const auto it = values.find(spec.key);
    if (it == values.end() ||
        !is_valid_video_setting_value(spec, it->second)) {
        return std::nullopt;
    }
    return it->second;
}

} // namespace goliath
