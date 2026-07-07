#pragma once

#include <string>
#include <vector>

#include "domain/sessions/session_repository.hpp"

namespace spectra5::services {

// Filesystem-backed session repository (PRD §22 storage layout). Uses POSIX
// directory/file APIs plus append-only JSONL so it works unchanged on the
// desktop host filesystem and on the Tab5 microSD (FATFS via VFS).
//
// Layout:
//   <base>/<session-id>/session.json
//   <base>/<session-id>/observations/<type>.jsonl   (append-only)
//   <base>/<session-id>/exports/export-<ts>.jsonl
class FilesystemSessionStore final : public domain::ISessionRepository {
public:
    explicit FilesystemSessionStore(std::string base_path);

    Status create(const domain::Session& session) override;
    Status update(const domain::Session& session) override;
    Result<domain::Session> get(const domain::SessionId& id) override;
    Result<std::vector<domain::Session>> list() override;
    Status remove(const domain::SessionId& id) override;

    Status append_observation(const domain::Observation& observation) override;
    Result<std::vector<domain::Observation>> observations(const domain::SessionId& id) override;

    Result<std::string> export_jsonl(const domain::SessionId& id) override;

    const std::string& base_path() const { return base_path_; }

private:
    std::string session_dir(const domain::SessionId& id) const;
    Status write_session_file(const domain::Session& session);

    std::string base_path_;
};

}  // namespace spectra5::services
