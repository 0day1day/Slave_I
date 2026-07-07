#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/result.hpp"

namespace spectra5::services {

struct FileNode {
    std::string name;
    bool is_dir       = false;
    std::uint64_t size = 0;
};

// Read-only filesystem browser rooted at a base path. Used by the Files module
// to navigate the microSD (Tab5, root "/sd") or the local data dir (desktop).
// Relative paths are sanitised to stay within the root (no ".." escape).
class FilesystemBrowser {
public:
    explicit FilesystemBrowser(std::string root);

    const std::string& root() const { return root_; }

    // Lists a directory relative to the root. Entries are sorted: directories
    // first, then files, each alphabetically.
    Result<std::vector<FileNode>> list(const std::string& relative_path = "");

    // Reads up to max_bytes of a text file relative to the root.
    Result<std::string> read_text(const std::string& relative_path,
                                  std::size_t max_bytes = 64 * 1024);

    // Mutating operations (all paths relative to the root, ".." rejected).
    bool make_dir(const std::string& relative_path);
    bool make_file(const std::string& relative_path);        // create empty file
    bool write_text(const std::string& relative_path, const std::string& content);
    bool remove_path(const std::string& relative_path);      // file or dir (recursive)
    bool copy_path(const std::string& src_rel, const std::string& dst_rel);   // file/dir (recursive)
    bool move_path(const std::string& src_rel, const std::string& dst_rel);   // rename/move

private:
    std::string resolve(const std::string& relative_path) const;

    std::string root_;
};

// Injectable singleton (non-owning; platform keeps the instance alive).
FilesystemBrowser* storage_browser();
void inject_storage_browser(FilesystemBrowser* browser);
bool has_storage_browser();

}  // namespace spectra5::services
