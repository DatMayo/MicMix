#pragma once

// Looks up the latest published release of the MicMix repository on GitHub.
// Only the public "latest release" endpoint is requested; no user data is sent
// (User-Agent: "MicMix/<version>").

#include <string>

struct UpdateInfo {
    bool checked = false;   // a response was received and understood
    bool available = false; // the release is newer than the running version
    std::string version;    // "1.5.0"
    std::string url;        // https://github.com/<repo>/releases/tag/<tag>
};

// Blocking (a few seconds at most). Call from a worker thread.
UpdateInfo CheckForUpdate(const std::string& currentVersion, const std::string& repo);

// Aborts a running CheckForUpdate() from another thread (e.g. on shutdown).
void CancelUpdateCheck();

// Helpers exposed for tests.
bool ParseReleaseTag(const std::string& json, std::string& tagOut);
bool IsNewerVersion(const std::string& candidate, const std::string& current);
