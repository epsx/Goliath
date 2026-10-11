#include "qt_path.hpp"

#include <QByteArray>

namespace fs = std::filesystem;

namespace goliath {

std::string path_to_utf8(const fs::path& path) {
    const std::u8string encoded = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()),
                       encoded.size());
}

fs::path path_from_utf8(std::string_view path) {
    std::u8string encoded;
    encoded.reserve(path.size());
    for (const char rawByte : path) {
        const auto byte = static_cast<unsigned char>(rawByte);
        encoded.push_back(static_cast<char8_t>(byte));
    }
    return fs::path(encoded);
}

QString path_to_qstring(const fs::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.native());
#else
    const std::string encoded = path_to_utf8(path);
    return QString::fromUtf8(encoded.data(),
                             static_cast<qsizetype>(encoded.size()));
#endif
}

fs::path qstring_to_path(const QString& path) {
#if defined(_WIN32)
    return fs::path(path.toStdWString());
#else
    const QByteArray encoded = path.toUtf8();
    return path_from_utf8(std::string_view(encoded.constData(),
                                           static_cast<std::size_t>(encoded.size())));
#endif
}

} // namespace goliath
