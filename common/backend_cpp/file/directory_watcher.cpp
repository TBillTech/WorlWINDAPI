#include "directory_watcher.h"
#include "inotify_directory_watcher.h"

std::unique_ptr<DirectoryWatcher> makeDefaultDirectoryWatcher()
{
#ifndef _WIN32
    auto watcher = std::make_unique<InotifyDirectoryWatcher>();
    if (watcher->isValid()) {
        return watcher;
    }
#endif
    return std::make_unique<NullDirectoryWatcher>();
}
