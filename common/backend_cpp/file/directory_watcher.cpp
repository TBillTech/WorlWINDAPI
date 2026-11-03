#include "directory_watcher.h"
#include "inotify_directory_watcher.h"
#include "windows_directory_watcher.h"

std::unique_ptr<DirectoryWatcher> makeDefaultDirectoryWatcher()
{
#ifdef _WIN32
    return std::make_unique<WindowsDirectoryWatcher>();
#else
    auto watcher = std::make_unique<InotifyDirectoryWatcher>();
    if (watcher->isValid()) {
        return watcher;
    }
#endif
    return std::make_unique<NullDirectoryWatcher>();
}
