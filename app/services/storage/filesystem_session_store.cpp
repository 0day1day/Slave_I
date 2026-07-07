#include "services/storage/filesystem_session_store.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <functional>
#include <sstream>

#include <nlohmann/json.hpp>

#include "core/diagnostics/log.hpp"

namespace spectra5::services {

using nlohmann::json;
using namespace spectra5::domain;

namespace {

constexpr const char* kTag = "session-store";

bool is_valid_id(const std::string& id)
{
    if (id.empty() || id.size() > 64) {
        return false;
    }
    for (char c : id) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                        c == '-' || c == '_';
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool path_exists(const std::string& path)
{
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0;
}

bool is_directory(const std::string& path)
{
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool make_dir(const std::string& path)
{
    if (::mkdir(path.c_str(), 0775) == 0) {
        return true;
    }
    return errno == EEXIST;
}

bool make_dirs(const std::string& path)
{
    std::string acc;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '/') {
            if (!acc.empty() && !make_dir(acc)) {
                return false;
            }
        }
        acc += path[i];
    }
    return acc.empty() || make_dir(acc);
}

bool read_file(const std::string& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool write_atomic(const std::string& path, const std::string& content)
{
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return false;
        }
        out << content;
        out.flush();
        if (!out) {
            return false;
        }
    }
    ::remove(path.c_str());
    return ::rename(tmp.c_str(), path.c_str()) == 0;
}

std::string safe_dump(const json& j, int indent)
{
    return j.dump(indent, ' ', false, json::error_handler_t::replace);
}

std::string to_hex(const std::vector<uint8_t>& data)
{
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(data.size() * 2);
    for (uint8_t b : data) {
        out += hex[b >> 4];
        out += hex[b & 0xF];
    }
    return out;
}

json session_to_json(const Session& s)
{
    json j;
    j["schema"]            = 1;
    j["id"]                = s.id;
    j["name"]              = s.name;
    j["description"]       = s.description;
    j["created_at"]        = s.created_at;
    j["started_at"]        = s.started_at;
    j["ended_at"]          = s.ended_at.has_value() ? json(*s.ended_at) : json(nullptr);
    j["status"]            = session_status_name(s.status);
    j["tags"]              = s.tags;
    j["observation_count"] = s.observation_count;
    return j;
}

Session session_from_json(const json& j)
{
    Session s;
    s.id                = j.value("id", std::string{});
    s.name              = j.value("name", std::string{});
    s.description       = j.value("description", std::string{});
    s.created_at        = j.value("created_at", static_cast<Timestamp>(0));
    s.started_at        = j.value("started_at", static_cast<Timestamp>(0));
    s.status            = session_status_from(j.value("status", std::string("draft")));
    s.observation_count = j.value("observation_count", 0);
    if (j.contains("ended_at") && !j.at("ended_at").is_null()) {
        s.ended_at = j.at("ended_at").get<Timestamp>();
    }
    if (j.contains("tags") && j.at("tags").is_array()) {
        for (const auto& t : j.at("tags")) {
            s.tags.push_back(t.get<std::string>());
        }
    }
    return s;
}

json observation_to_json(const Observation& o)
{
    json j;
    j["schema"]          = Observation::kSchemaVersion;
    j["id"]              = o.id;
    j["session_id"]      = o.session_id;
    j["type"]            = observation_type_name(o.type);
    j["timestamp"]       = o.timestamp;
    j["source"]          = o.source;
    j["signal_strength"] = o.signal_strength;
    j["metadata"]        = o.metadata;
    if (!o.raw_data.empty()) {
        j["raw_hex"] = to_hex(o.raw_data);
    }
    return j;
}

Observation observation_from_json(const json& j)
{
    Observation o;
    o.id              = j.value("id", std::string{});
    o.session_id      = j.value("session_id", std::string{});
    o.type            = observation_type_from(j.value("type", std::string("generic")));
    o.timestamp       = j.value("timestamp", static_cast<Timestamp>(0));
    o.source          = j.value("source", std::string{});
    o.signal_strength = j.value("signal_strength", 0);
    if (j.contains("metadata") && j.at("metadata").is_object()) {
        for (auto it = j.at("metadata").begin(); it != j.at("metadata").end(); ++it) {
            o.metadata[it.key()] = it.value().get<std::string>();
        }
    }
    return o;
}

}  // namespace

FilesystemSessionStore::FilesystemSessionStore(std::string base_path) : base_path_(std::move(base_path))
{
    if (!base_path_.empty() && base_path_.back() == '/') {
        base_path_.pop_back();
    }
    if (!make_dirs(base_path_)) {
        spectra5::log::tagWarn(kTag, "could not create base path: {}", base_path_);
    }
}

std::string FilesystemSessionStore::session_dir(const SessionId& id) const
{
    return base_path_ + "/" + id;
}

Status FilesystemSessionStore::write_session_file(const Session& session)
{
    const std::string dir = session_dir(session.id);
    if (!make_dirs(dir)) {
        return Status::fail(ErrorCode::IoError, "cannot create session directory");
    }
    if (!write_atomic(dir + "/session.json", safe_dump(session_to_json(session), 2))) {
        return Status::fail(ErrorCode::IoError, "cannot write session.json");
    }
    return Status::ok();
}

Status FilesystemSessionStore::create(const Session& session)
{
    if (!is_valid_id(session.id)) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid session id");
    }
    if (path_exists(session_dir(session.id))) {
        return Status::fail(ErrorCode::AlreadyExists, "session already exists");
    }
    return write_session_file(session);
}

Status FilesystemSessionStore::update(const Session& session)
{
    if (!is_valid_id(session.id)) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid session id");
    }
    if (!path_exists(session_dir(session.id))) {
        return Status::fail(ErrorCode::NotFound, "session not found");
    }
    return write_session_file(session);
}

Result<Session> FilesystemSessionStore::get(const SessionId& id)
{
    if (!is_valid_id(id)) {
        return Result<Session>::fail(ErrorCode::InvalidArgument, "invalid session id");
    }
    std::string content;
    if (!read_file(session_dir(id) + "/session.json", content)) {
        return Result<Session>::fail(ErrorCode::NotFound, "session not found");
    }
    json j = json::parse(content, nullptr, false);
    if (j.is_discarded()) {
        return Result<Session>::fail(ErrorCode::IoError, "corrupt session.json");
    }
    return Result<Session>::ok(session_from_json(j));
}

Result<std::vector<Session>> FilesystemSessionStore::list()
{
    std::vector<Session> sessions;
    DIR* dir = ::opendir(base_path_.c_str());
    if (dir == nullptr) {
        return Result<std::vector<Session>>::ok(std::move(sessions));
    }
    struct dirent* entry;
    while ((entry = ::readdir(dir)) != nullptr) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        const std::string full = base_path_ + "/" + name;
        if (!is_directory(full)) {
            continue;
        }
        std::string content;
        if (!read_file(full + "/session.json", content)) {
            continue;
        }
        json j = json::parse(content, nullptr, false);
        if (j.is_discarded()) {
            continue;  // Skip corrupt entries; do not abort the whole listing.
        }
        sessions.push_back(session_from_json(j));
    }
    ::closedir(dir);
    return Result<std::vector<Session>>::ok(std::move(sessions));
}

Status FilesystemSessionStore::remove(const SessionId& id)
{
    if (!is_valid_id(id)) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid session id");
    }
    const std::string dir = session_dir(id);
    if (!path_exists(dir)) {
        return Status::fail(ErrorCode::NotFound, "session not found");
    }

    // Recursive delete (sessions only contain files and one observations dir).
    std::function<bool(const std::string&)> rm = [&](const std::string& path) -> bool {
        DIR* d = ::opendir(path.c_str());
        if (d != nullptr) {
            struct dirent* e;
            while ((e = ::readdir(d)) != nullptr) {
                const std::string name = e->d_name;
                if (name == "." || name == "..") {
                    continue;
                }
                const std::string child = path + "/" + name;
                if (is_directory(child)) {
                    rm(child);
                } else {
                    ::remove(child.c_str());
                }
            }
            ::closedir(d);
        }
        return ::rmdir(path.c_str()) == 0;
    };

    if (!rm(dir)) {
        return Status::fail(ErrorCode::IoError, "cannot remove session directory");
    }
    return Status::ok();
}

Status FilesystemSessionStore::append_observation(const Observation& observation)
{
    if (!is_valid_id(observation.session_id)) {
        return Status::fail(ErrorCode::InvalidArgument, "invalid session id");
    }
    const std::string dir = session_dir(observation.session_id);
    if (!path_exists(dir)) {
        return Status::fail(ErrorCode::NotFound, "session not found");
    }
    const std::string obs_dir = dir + "/observations";
    if (!make_dirs(obs_dir)) {
        return Status::fail(ErrorCode::IoError, "cannot create observations directory");
    }

    const std::string file = obs_dir + "/" + observation_type_name(observation.type) + ".jsonl";
    {
        std::ofstream out(file, std::ios::binary | std::ios::app);
        if (!out) {
            return Status::fail(ErrorCode::IoError, "cannot append observation");
        }
        out << safe_dump(observation_to_json(observation), -1) << "\n";
        out.flush();
        if (!out) {
            return Status::fail(ErrorCode::IoError, "observation write failed");
        }
    }

    // Keep the running observation count in session.json up to date.
    auto session = get(observation.session_id);
    if (session) {
        session.value().observation_count += 1;
        write_session_file(session.value());
    }
    return Status::ok();
}

Result<std::vector<Observation>> FilesystemSessionStore::observations(const SessionId& id)
{
    std::vector<Observation> result;
    const std::string obs_dir = session_dir(id) + "/observations";
    DIR* dir = ::opendir(obs_dir.c_str());
    if (dir == nullptr) {
        return Result<std::vector<Observation>>::ok(std::move(result));
    }
    struct dirent* entry;
    while ((entry = ::readdir(dir)) != nullptr) {
        const std::string name = entry->d_name;
        if (name.size() < 6 || name.substr(name.size() - 6) != ".jsonl") {
            continue;
        }
        std::ifstream in(obs_dir + "/" + name, std::ios::binary);
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) {
                continue;
            }
            json j = json::parse(line, nullptr, false);
            if (j.is_discarded()) {
                continue;  // Skip malformed lines.
            }
            result.push_back(observation_from_json(j));
        }
    }
    ::closedir(dir);
    return Result<std::vector<Observation>>::ok(std::move(result));
}

Result<std::string> FilesystemSessionStore::export_jsonl(const SessionId& id)
{
    auto session = get(id);
    if (!session) {
        return Result<std::string>::fail(session.error().code, session.error().message);
    }

    const std::string export_dir = session_dir(id) + "/exports";
    if (!make_dirs(export_dir)) {
        return Result<std::string>::fail(ErrorCode::IoError, "cannot create exports directory");
    }

    const std::string path = export_dir + "/export-" + std::to_string(session.value().created_at) +
                             "-" + std::to_string(session.value().observation_count) + ".jsonl";

    std::ostringstream body;
    {
        json header;
        header["record"]  = "session";
        header["session"] = session_to_json(session.value());
        body << safe_dump(header, -1) << "\n";
    }

    auto obs = observations(id);
    if (obs) {
        for (const auto& o : obs.value()) {
            json line;
            line["record"]      = "observation";
            line["observation"] = observation_to_json(o);
            body << safe_dump(line, -1) << "\n";
        }
    }

    if (!write_atomic(path, body.str())) {
        return Result<std::string>::fail(ErrorCode::IoError, "cannot write export file");
    }
    return Result<std::string>::ok(path);
}

}  // namespace spectra5::services
