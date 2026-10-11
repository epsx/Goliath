#include "file_transaction.hpp"

#include "common/qt_path.hpp"

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QSaveFile>

#include <cstddef>
#include <memory>
#include <set>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace goliath {

namespace {

void set_error(std::string* error, std::string message) {
    if (error) *error = std::move(message);
}

struct PendingFileSave {
    fs::path path;
    QByteArray replacement;
    QByteArray original;
    bool existed = false;
    std::unique_ptr<QSaveFile> staged;
};

bool restore_destination(const PendingFileSave& pending,
                         std::string* error) {
    if (!pending.existed) {
        std::error_code ec;
        const fs::file_status status = fs::symlink_status(pending.path, ec);
        if (ec == std::errc::no_such_file_or_directory ||
            (!ec && !fs::exists(status))) {
            return true;
        }
        if (ec) {
            set_error(error, "Could not inspect new file during rollback: " +
                                 path_to_utf8(pending.path) + ": " +
                                 ec.message());
            return false;
        }
        if (!fs::is_regular_file(status)) {
            set_error(error, "Refusing to remove a substituted file during "
                             "rollback: " + path_to_utf8(pending.path));
            return false;
        }

        const bool removed = fs::remove(pending.path, ec);
        if (ec) {
            set_error(error, "Could not remove new file during rollback: " +
                                 path_to_utf8(pending.path) + ": " +
                                 ec.message());
            return false;
        }
        if (removed) return true;

        const bool stillExists = fs::exists(pending.path, ec);
        if (ec) {
            set_error(error, "Could not verify new-file rollback: " +
                                 path_to_utf8(pending.path) + ": " +
                                 ec.message());
            return false;
        }
        return !stillExists;
    }

    QSaveFile restored(path_to_qstring(pending.path));
    restored.setDirectWriteFallback(false);
    if (!restored.open(QIODevice::WriteOnly)) {
        set_error(error, "Could not open file for rollback: " +
                             path_to_utf8(pending.path) + ": " +
                             restored.errorString().toStdString());
        return false;
    }
    if (restored.write(pending.original) != pending.original.size()) {
        set_error(error, "Could not write file rollback: " +
                             path_to_utf8(pending.path) + ": " +
                             restored.errorString().toStdString());
        restored.cancelWriting();
        return false;
    }
    if (!restored.commit()) {
        set_error(error, "Could not commit file rollback: " +
                             path_to_utf8(pending.path) + ": " +
                             restored.errorString().toStdString());
        return false;
    }
    return true;
}

} // namespace

bool save_files_transactionally(
    const std::vector<FileTransactionRequest>& requests,
    std::string* error) {
    if (error) error->clear();
    if (requests.empty()) return true;

    std::set<fs::path> destinations;
    std::vector<PendingFileSave> pending;
    pending.reserve(requests.size());

    for (const FileTransactionRequest& request : requests) {
        const fs::path path = request.path.lexically_normal();
        if (!destinations.insert(path).second) {
            set_error(error, "Duplicate file transaction destination: " +
                                 path_to_utf8(path));
            return false;
        }

        std::error_code ec;
        if (!path.parent_path().empty()) {
            fs::create_directories(path.parent_path(), ec);
            if (ec) {
                set_error(error, "Could not create transaction directory: " +
                                     path_to_utf8(path.parent_path()) + ": " +
                                     ec.message());
                return false;
            }
        }

        PendingFileSave item;
        item.path = path;
        item.replacement = QByteArray::fromStdString(request.contents);

        const fs::file_status status = fs::symlink_status(path, ec);
        if (ec == std::errc::no_such_file_or_directory) ec.clear();
        if (ec) {
            set_error(error, "Could not inspect transaction destination: " +
                                 path_to_utf8(path) + ": " + ec.message());
            return false;
        }

        item.existed = fs::exists(status);
        if (item.existed) {
            if (!fs::is_regular_file(status)) {
                set_error(error, "Transaction destination is not a direct "
                                 "regular file: " + path_to_utf8(path));
                return false;
            }

            QFile input(path_to_qstring(path));
            if (!input.open(QIODevice::ReadOnly)) {
                set_error(error, "Could not read file before transaction: " +
                                     path_to_utf8(path) + ": " +
                                     input.errorString().toStdString());
                return false;
            }
            item.original = input.readAll();
            if (input.error() != QFileDevice::NoError) {
                set_error(error, "Could not capture file before transaction: " +
                                     path_to_utf8(path) + ": " +
                                     input.errorString().toStdString());
                return false;
            }
        }

        pending.push_back(std::move(item));
    }

    for (PendingFileSave& item : pending) {
        item.staged = std::make_unique<QSaveFile>(path_to_qstring(item.path));
        item.staged->setDirectWriteFallback(false);
        if (!item.staged->open(QIODevice::WriteOnly)) {
            set_error(error, "Could not stage file for writing: " +
                                 path_to_utf8(item.path) + ": " +
                                 item.staged->errorString().toStdString());
            return false;
        }
        if (item.staged->write(item.replacement) != item.replacement.size()) {
            set_error(error, "Could not stage complete file contents: " +
                                 path_to_utf8(item.path) + ": " +
                                 item.staged->errorString().toStdString());
            item.staged->cancelWriting();
            return false;
        }
    }

    std::size_t committed = 0;
    for (; committed < pending.size(); ++committed) {
        PendingFileSave& item = pending[committed];
        if (item.staged->commit()) continue;

        std::string message = "Could not commit file transaction: " +
                              path_to_utf8(item.path) + ": " +
                              item.staged->errorString().toStdString();
        for (std::size_t rollback = committed; rollback > 0; --rollback) {
            std::string rollbackError;
            if (!restore_destination(pending[rollback - 1], &rollbackError)) {
                message += " Rollback also failed: " + rollbackError;
            }
        }
        set_error(error, std::move(message));
        return false;
    }

    return true;
}

} // namespace goliath
