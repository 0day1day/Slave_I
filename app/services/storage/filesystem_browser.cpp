#include "services/storage/filesystem_browser.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <sstream>

namespace spectra5::services {

namespace {

// Strips leading slashes and rejects any path containing a ".." component so
// listings cannot escape the configured root.
bool is_safe_relative(const std::string& path)
{
    std::string token;
    for (std::size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            if (token == "..") {
                return false;
            }
            token.clear();
        } else {
            token += path[i];
        }
    }
    return true;
}

}  // namespace

FilesystemBrowser::FilesystemBrowser(std::string root) : root_(std::move(root))
{
    if (!root_.empty() && root_.back() == '/') {
        root_.pop_back();
    }
}

std::string FilesystemBrowser::resolve(const std::string& relative_path) const
{
    std::string rel = relative_path;
    while (!rel.empty() && rel.front() == '/') {
        rel.erase(rel.begin());
    }
    if (rel.empty()) {
        return root_;
    }
    return root_ + "/" + rel;
}

Result<std::vector<FileNode>> FilesystemBrowser::list(const std::string& relative_path)
{
    if (!is_safe_relative(relative_path)) {
        return Result<std::vector<FileNode>>::fail(ErrorCode::InvalidArgument, "invalid path");
    }

    const std::string dir_path = resolve(relative_path);
    std::vector<FileNode> nodes;

    DIR* dir = ::opendir(dir_path.c_str());
    if (dir == nullptr) {
        return Result<std::vector<FileNode>>::fail(ErrorCode::NotFound, "cannot open directory");
    }

    struct dirent* entry;
    while ((entry = ::readdir(dir)) != nullptr) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        FileNode node;
        node.name = name;

        struct stat st {};
        if (::stat((dir_path + "/" + name).c_str(), &st) == 0) {
            node.is_dir = S_ISDIR(st.st_mode);
            node.size   = static_cast<std::uint64_t>(st.st_size);
        }
        nodes.push_back(std::move(node));
    }
    ::closedir(dir);

    std::sort(nodes.begin(), nodes.end(), [](const FileNode& a, const FileNode& b) {
        if (a.is_dir != b.is_dir) {
            return a.is_dir > b.is_dir;
        }
        return a.name < b.name;
    });

    return Result<std::vector<FileNode>>::ok(std::move(nodes));
}

Result<std::string> FilesystemBrowser::read_text(const std::string& relative_path,
                                                 std::size_t max_bytes)
{
    if (!is_safe_relative(relative_path)) {
        return Result<std::string>::fail(ErrorCode::InvalidArgument, "invalid path");
    }
    std::ifstream in(resolve(relative_path), std::ios::binary);
    if (!in) {
        return Result<std::string>::fail(ErrorCode::NotFound, "cannot open file");
    }
    std::string content(max_bytes, '\0');
    in.read(content.data(), static_cast<std::streamsize>(max_bytes));
    content.resize(static_cast<std::size_t>(in.gcount()));
    return Result<std::string>::ok(std::move(content));
}

bool FilesystemBrowser::make_dir(const std::string& rel)
{
    if (!is_safe_relative(rel) || rel.empty()) {
        return false;
    }
    return ::mkdir(resolve(rel).c_str(), 0777) == 0;
}

bool FilesystemBrowser::make_file(const std::string& rel)
{
    return write_text(rel, "");
}

bool FilesystemBrowser::write_text(const std::string& rel, const std::string& content)
{
    if (!is_safe_relative(rel) || rel.empty()) {
        return false;
    }
    std::ofstream out(resolve(rel), std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(out);
}

bool FilesystemBrowser::remove_path(const std::string& rel)
{
    if (!is_safe_relative(rel) || rel.empty()) {
        return false;
    }
    const std::string abs = resolve(rel);
    struct stat st {};
    if (::stat(abs.c_str(), &st) != 0) {
        return false;
    }
    if (S_ISDIR(st.st_mode)) {
        if (DIR* d = ::opendir(abs.c_str())) {
            struct dirent* e;
            while ((e = ::readdir(d)) != nullptr) {
                const std::string n = e->d_name;
                if (n == "." || n == "..") {
                    continue;
                }
                remove_path(rel + "/" + n);
            }
            ::closedir(d);
        }
        return ::rmdir(abs.c_str()) == 0;
    }
    return ::remove(abs.c_str()) == 0;
}

bool FilesystemBrowser::copy_path(const std::string& src_rel, const std::string& dst_rel)
{
    if (!is_safe_relative(src_rel) || !is_safe_relative(dst_rel) || src_rel.empty() ||
        dst_rel.empty()) {
        return false;
    }
    const std::string src = resolve(src_rel);
    const std::string dst = resolve(dst_rel);
    struct stat st {};
    if (::stat(src.c_str(), &st) != 0) {
        return false;
    }
    if (S_ISDIR(st.st_mode)) {
        if (::mkdir(dst.c_str(), 0777) != 0 && errno != EEXIST) {
            return false;
        }
        bool ok = true;
        if (DIR* d = ::opendir(src.c_str())) {
            struct dirent* e;
            while ((e = ::readdir(d)) != nullptr) {
                const std::string n = e->d_name;
                if (n == "." || n == "..") {
                    continue;
                }
                ok = copy_path(src_rel + "/" + n, dst_rel + "/" + n) && ok;
            }
            ::closedir(d);
        }
        return ok;
    }
    std::ifstream in(src, std::ios::binary);
    std::ofstream out(dst, std::ios::binary | std::ios::trunc);
    if (!in || !out) {
        return false;
    }
    out << in.rdbuf();
    return static_cast<bool>(out);
}

bool FilesystemBrowser::move_path(const std::string& src_rel, const std::string& dst_rel)
{
    if (!is_safe_relative(src_rel) || !is_safe_relative(dst_rel) || src_rel.empty() ||
        dst_rel.empty()) {
        return false;
    }
    const std::string src = resolve(src_rel);
    const std::string dst = resolve(dst_rel);
    if (::rename(src.c_str(), dst.c_str()) == 0) {
        return true;
    }
    // Cross-device or FAT quirk: fall back to copy + delete.
    if (copy_path(src_rel, dst_rel)) {
        return remove_path(src_rel);
    }
    return false;
}

namespace {
FilesystemBrowser* g_browser = nullptr;
}

FilesystemBrowser* storage_browser()
{
    return g_browser;
}

void inject_storage_browser(FilesystemBrowser* browser)
{
    g_browser = browser;
}

bool has_storage_browser()
{
    return g_browser != nullptr;
}

}  // namespace spectra5::services
