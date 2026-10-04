#include "hash_cache.hpp"

#include "json.hpp"

#include <QByteArray>
#include <QIODevice>
#include <QSaveFile>
#include <QString>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <utility>

namespace fs = std::filesystem;
using json = nlohmann::ordered_json;

namespace goliath {
namespace {

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

QString path_to_qstring(const fs::path& path) {
#if defined(_WIN32)
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromStdString(path.string());
#endif
}

fs::path logical_path(const fs::path& path) {
#ifdef _WIN32
    const std::wstring native = path.wstring();
    static const std::wstring unc_prefix = L"\\\\?\\UNC\\";
    static const std::wstring local_prefix = L"\\\\?\\";

    if (native.rfind(unc_prefix, 0) == 0)
        return fs::path(std::wstring(L"\\\\") + native.substr(unc_prefix.size()));
    if (native.rfind(local_prefix, 0) == 0)
        return fs::path(native.substr(local_prefix.size()));
#endif
    return path;
}

std::string cache_key(const fs::path& path) {
    const fs::path logical = logical_path(path);

    std::error_code ec;
    fs::path absolute = logical;
    if (!absolute.is_absolute()) {
        absolute = fs::absolute(logical, ec);
        if (ec)
            absolute = logical;
    }

    std::string key = absolute.lexically_normal().generic_string();
#ifdef _WIN32
    key = to_lower(key);
#endif
    return key;
}

std::optional<std::int64_t> file_mtime_ticks(const fs::path& path) {
    std::error_code ec;
    const fs::file_time_type mtime = fs::last_write_time(path, ec);
    if (ec)
        return std::nullopt;

    using namespace std::chrono;
    return duration_cast<nanoseconds>(mtime.time_since_epoch()).count();
}

} // namespace

HashCache::HashCache(fs::path path,
                     std::string algorithm,
                     std::string digest_field,
                     std::size_t digest_hex_length)
    : m_path(std::move(path)),
      m_algorithm(std::move(algorithm)),
      m_digest_field(std::move(digest_field)),
      m_digest_hex_length(digest_hex_length) {}

bool HashCache::valid_digest(const std::string& value) const {
    return value.size() == m_digest_hex_length &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) {
               return std::isxdigit(c) != 0;
           });
}

HashCache HashCache::load(const fs::path& path,
                          std::string algorithm,
                          std::string digest_field,
                          std::size_t digest_hex_length) {
    HashCache cache(path, std::move(algorithm), std::move(digest_field),
                    digest_hex_length);

    std::ifstream in(path, std::ios::binary);
    if (!in)
        return cache;

    try {
        json root;
        in >> root;
        if (!root.is_object() || !root.contains("version") ||
            !root["version"].is_number_integer() ||
            root["version"].get<int>() != 1 ||
            !root.contains("files") || !root["files"].is_object()) {
            return cache;
        }
        if (root.contains("algorithm") &&
            (!root["algorithm"].is_string() ||
             to_lower(root["algorithm"].get<std::string>()) !=
                 to_lower(cache.m_algorithm))) {
            return cache;
        }

        for (auto it = root["files"].begin(); it != root["files"].end(); ++it) {
            if (!it.value().is_object())
                continue;
            const json& row = it.value();
            if (!row.contains("size") || !row["size"].is_number_unsigned() ||
                !row.contains("mtime_ns") || !row["mtime_ns"].is_number_integer() ||
                !row.contains(cache.m_digest_field) ||
                !row[cache.m_digest_field].is_string()) {
                continue;
            }

            Entry entry;
            entry.size = row["size"].get<std::uintmax_t>();
            entry.mtime_ticks = row["mtime_ns"].get<std::int64_t>();
            entry.digest =
                to_lower(row[cache.m_digest_field].get<std::string>());
            if (!cache.valid_digest(entry.digest))
                continue;
            cache.m_entries[it.key()] = std::move(entry);
        }
    } catch (...) {
        cache.m_entries.clear();
    }

    return cache;
}

std::optional<std::string> HashCache::file_hash(
    const fs::path& file,
    std::uintmax_t size,
    const Calculator& calculator,
    const std::atomic<bool>* cancel) {
    const auto mtime = file_mtime_ticks(file);
    if (!mtime.has_value())
        return std::nullopt;

    const std::string key = cache_key(file);
    m_touched_keys.insert(key);

    const auto it = m_entries.find(key);
    if (it != m_entries.end() && it->second.size == size &&
        it->second.mtime_ticks == *mtime && valid_digest(it->second.digest)) {
        ++m_hits;
        return it->second.digest;
    }

    const auto calculated = calculator(file, cancel);
    if (!calculated.has_value())
        return std::nullopt;
    const std::string digest = to_lower(*calculated);
    if (!valid_digest(digest))
        return std::nullopt;

    ++m_calculated;
    m_entries[key] = Entry{size, *mtime, digest};
    m_dirty = true;
    return m_entries[key].digest;
}

void HashCache::prune_untouched() {
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (m_touched_keys.find(it->first) != m_touched_keys.end()) {
            ++it;
            continue;
        }
        it = m_entries.erase(it);
        ++m_pruned;
        m_dirty = true;
    }
}

bool HashCache::save() {
    if (!m_dirty)
        return true;

    std::error_code ec;
    fs::create_directories(m_path.parent_path(), ec);
    if (ec)
        return false;

    json root;
    root["version"] = 1;
    root["algorithm"] = m_algorithm;
    root["files"] = json::object();
    for (const auto& [key, entry] : m_entries) {
        json row;
        row["size"] = entry.size;
        row["mtime_ns"] = entry.mtime_ticks;
        row[m_digest_field] = entry.digest;
        root["files"][key] = std::move(row);
    }

    const QByteArray serialized = QByteArray::fromStdString(root.dump(2) + '\n');
    QSaveFile output(path_to_qstring(m_path));
    output.setDirectWriteFallback(false);
    if (!output.open(QIODevice::WriteOnly))
        return false;
    if (output.write(serialized) != serialized.size()) {
        output.cancelWriting();
        return false;
    }
    if (!output.commit())
        return false;

    m_dirty = false;
    return true;
}

} // namespace goliath
