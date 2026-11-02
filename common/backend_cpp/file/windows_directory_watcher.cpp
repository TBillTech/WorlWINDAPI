#include "windows_directory_watcher.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <iostream>

namespace {

constexpr DWORD kBufferBytes = 64 * 1024;  // Bounded; overflow is detected as a zero-byte completion.
constexpr DWORD kNotifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE;

std::wstring utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string wideToUtf8(const wchar_t* w, size_t len)
{
    if (len == 0) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(len), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(len), s.data(), n, nullptr, nullptr);
    return s;
}

}  // namespace

struct WindowsDirectoryWatcher::Watch {
    HANDLE dir = INVALID_HANDLE_VALUE;
    HANDLE event = nullptr;
    OVERLAPPED overlapped{};
    bool pending = false;       // A ReadDirectoryChangesW request is outstanding.
    bool invalidated = false;   // Directory deleted or read failed; no further events.
    uint32_t mask = 0;
    std::string filter_name;    // Non-empty when a file (not a directory) is being watched.
    alignas(DWORD) char buffer[kBufferBytes];

    ~Watch() { shutdown(); }

    bool arm()
    {
        ResetEvent(event);
        overlapped = OVERLAPPED{};
        overlapped.hEvent = event;
        DWORD ignored = 0;
        if (!ReadDirectoryChangesW(dir, buffer, kBufferBytes, FALSE, kNotifyFilter, &ignored, &overlapped, nullptr)) {
            pending = false;
            return false;
        }
        pending = true;
        return true;
    }

    // Cancels pending I/O and waits for the cancellation to complete before anything is released.
    void shutdown()
    {
        if (pending) {
            CancelIoEx(dir, &overlapped);
            DWORD transferred = 0;
            GetOverlappedResult(dir, &overlapped, &transferred, TRUE);
            pending = false;
        }
        if (dir != INVALID_HANDLE_VALUE) {
            CloseHandle(dir);
            dir = INVALID_HANDLE_VALUE;
        }
        if (event) {
            CloseHandle(event);
            event = nullptr;
        }
    }
};

WindowsDirectoryWatcher::WindowsDirectoryWatcher() = default;

WindowsDirectoryWatcher::~WindowsDirectoryWatcher()
{
    watches_.clear();  // Each Watch cancels and waits for its pending I/O before releasing handles and buffers.
}

WatchHandle WindowsDirectoryWatcher::add(const std::string& path, uint32_t event_mask)
{
    std::wstring wpath = utf8ToWide(path);
    if (wpath.empty()) {
        return kInvalidWatchHandle;
    }
    DWORD attrs = GetFileAttributesW(wpath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return kInvalidWatchHandle;
    }
    std::string filter_name;
    std::wstring dir_path = wpath;
    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        size_t slash = wpath.find_last_of(L"/\\");
        if (slash == std::wstring::npos) {
            dir_path = L".";
            filter_name = wideToUtf8(wpath.data(), wpath.size());
        } else {
            dir_path = wpath.substr(0, slash + 1);
            filter_name = wideToUtf8(wpath.data() + slash + 1, wpath.size() - slash - 1);
        }
    }
    auto watch = std::make_unique<Watch>();
    watch->mask = event_mask;
    watch->filter_name = filter_name;
    watch->dir = CreateFileW(dir_path.c_str(), FILE_LIST_DIRECTORY,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                             FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
    if (watch->dir == INVALID_HANDLE_VALUE) {
        return kInvalidWatchHandle;
    }
    watch->event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!watch->event || !watch->arm()) {
        return kInvalidWatchHandle;  // ~Watch releases whatever was opened.
    }
    WatchHandle handle = next_handle_++;
    watches_[handle] = std::move(watch);
    return handle;
}

bool WindowsDirectoryWatcher::remove(WatchHandle handle)
{
    auto it = watches_.find(handle);
    if (it == watches_.end()) {
        return false;
    }
    watches_.erase(it);  // Cancels and waits before release.
    return true;
}

std::vector<WatchEvent> WindowsDirectoryWatcher::poll()
{
    std::vector<WatchEvent> events;
    for (auto& entry : watches_) {
        WatchHandle handle = entry.first;
        Watch& watch = *entry.second;
        if (!watch.pending) {
            continue;
        }
        DWORD bytes = 0;
        if (!GetOverlappedResult(watch.dir, &watch.overlapped, &bytes, FALSE)) {
            DWORD err = GetLastError();
            if (err == ERROR_IO_INCOMPLETE) {
                continue;
            }
            // The directory was deleted (or otherwise became unreadable): report it and invalidate.
            watch.pending = false;
            watch.invalidated = true;
            if (watch.mask & kWatchDelete) {
                events.push_back({handle, WatchEventKind::remove, ""});
            }
            continue;
        }
        // Copy the completed buffer so the read can be re-armed before processing (no gap for lost events).
        std::vector<char> completed;
        if (bytes > 0) {
            completed.assign(watch.buffer, watch.buffer + bytes);
        }
        if (!watch.arm()) {
            watch.invalidated = true;
            if (watch.mask & kWatchDelete) {
                events.push_back({handle, WatchEventKind::remove, ""});
            }
        }
        if (bytes == 0) {
            std::cerr << "[WindowsDirectoryWatcher] change buffer overflow; events were missed" << std::endl;
            if (!watch.filter_name.empty() && (watch.mask & kWatchModify)) {
                events.push_back({handle, WatchEventKind::modify, ""});
            }
            continue;
        }

        bool last_was_modify = false;
        auto emit = [&](WatchEventKind kind, const std::string& name) {
            uint32_t bit = kind == WatchEventKind::modify ? kWatchModify
                         : kind == WatchEventKind::create ? kWatchCreate : kWatchDelete;
            if (!(watch.mask & bit)) return;
            if (!watch.filter_name.empty() && name != watch.filter_name) return;
            if (kind == WatchEventKind::modify) {
                if (last_was_modify) return;  // Coalesce consecutive modifies; the final one is kept.
                last_was_modify = true;
                events.push_back({handle, kind, ""});
            } else {
                last_was_modify = false;
                events.push_back({handle, kind, name});
            }
        };
        size_t offset = 0;
        while (offset + sizeof(FILE_NOTIFY_INFORMATION) <= completed.size()) {
            auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(completed.data() + offset);
            std::string name = wideToUtf8(info->FileName, info->FileNameLength / sizeof(wchar_t));
            switch (info->Action) {
                case FILE_ACTION_ADDED:
                case FILE_ACTION_RENAMED_NEW_NAME:
                    emit(WatchEventKind::create, name);
                    break;
                case FILE_ACTION_REMOVED:
                case FILE_ACTION_RENAMED_OLD_NAME:
                    emit(WatchEventKind::remove, name);
                    break;
                case FILE_ACTION_MODIFIED:
                    emit(WatchEventKind::modify, name);
                    break;
                default:
                    break;
            }
            if (info->NextEntryOffset == 0) break;
            offset += info->NextEntryOffset;
        }
    }
    return events;
}
#endif
