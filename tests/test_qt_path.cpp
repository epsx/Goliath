#include "catch2/catch.hpp"

#include "common/goliath_common.hpp"
#include "common/qt_path.hpp"

#include <QDir>

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

using namespace goliath;

TEST_CASE("Qt and filesystem paths preserve Unicode", "[qt-path][unicode]") {
    const QString directory = QStringLiteral(u"Știință 日本");
    const QString filename = QStringLiteral(u"fișier テスト.zip");
    const fs::path path = qstring_to_path(directory) / qstring_to_path(filename);
    const QString expected = directory + QChar('/') + filename;

    CHECK(QDir::fromNativeSeparators(path_to_qstring(path)) == expected);
    CHECK(QDir::fromNativeSeparators(
              path_to_qstring(path_from_utf8(path_to_utf8(path)))) == expected);
}

TEST_CASE("UTF-8 config paths resolve without locale conversion",
          "[qt-path][unicode][config]") {
    Config config;
    const fs::path base = qstring_to_path(QStringLiteral(u"portable Știință"));
    const fs::path relative =
        qstring_to_path(QStringLiteral(u"ROM-uri/日本"));
    config.set("Paths", "roms", path_to_utf8(relative));

    CHECK(resolve_path(config, "roms", base) == base / relative);
}
