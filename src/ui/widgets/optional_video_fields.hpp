// optional_video_fields.hpp — per-game form widgets for the shared video
// setting schema. Every row has an explicit "Inherit global" state.
#pragma once

#include <map>
#include <optional>
#include <string>

#include "game/video_settings.hpp"

class QFormLayout;
class QWidget;

namespace goliath {

class OptionalVideoFieldSet {
public:
    void addToForm(QFormLayout* form,
                   const VideoSettingSpec& spec,
                   int globalValue);

    QWidget* widget(const std::string& key) const;
    void setOverride(const std::string& key, std::optional<int> value);
    std::optional<int> overrideValue(const std::string& key) const;
    int effectiveValue(const std::string& key, int globalValue) const;
    VideoSettingValues overrides() const;
    void setOverrides(const VideoSettingValues& values);
    void setEnabled(const std::string& key, bool enabled);
    void resetToInherit();

private:
    struct Entry {
        const VideoSettingSpec* spec;
        QWidget* widget;
        int inherit_value;
    };
    std::map<std::string, Entry> m_entries;
};

} // namespace goliath
