// qt_path.hpp — lossless conversions at the Qt/std::filesystem boundary.
// Persisted path strings are UTF-8 and use generic ('/') separators.
#pragma once

#include <QString>

#include <filesystem>
#include <string>
#include <string_view>

namespace goliath {

QString path_to_qstring(const std::filesystem::path& path);
std::filesystem::path qstring_to_path(const QString& path);

std::string path_to_utf8(const std::filesystem::path& path);
std::filesystem::path path_from_utf8(std::string_view path);

} // namespace goliath
