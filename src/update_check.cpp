#include "update_check.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winhttp.h>

#include <array>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <mutex>
#include <string>

#pragma comment(lib, "Winhttp.lib")

namespace {

std::mutex g_sessionMutex;
HINTERNET g_session = nullptr;
std::atomic<bool> g_cancelled{ false };

constexpr DWORD kTimeoutMs = 4000;
constexpr size_t kMaxResponseBytes = 512 * 1024;

std::wstring Widen(const std::string& s) {
    return std::wstring(s.begin(), s.end()); // ASCII only (host, path, version)
}

// "v1.12.3" / "1.12.3" -> {1, 12, 3}
bool ParseSemVer(const std::string& text, std::array<int, 3>& out) {
    size_t i = (!text.empty() && (text[0] == 'v' || text[0] == 'V')) ? 1 : 0;
    for (int part = 0; part < 3; ++part) {
        if (i >= text.size() || !std::isdigit(static_cast<unsigned char>(text[i]))) {
            return false;
        }
        int value = 0;
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) {
            value = (value * 10) + (text[i] - '0');
            if (value > 100000) return false;
            ++i;
        }
        out[static_cast<size_t>(part)] = value;
        if (part < 2) {
            if (i >= text.size() || text[i] != '.') return false;
            ++i;
        }
    }
    return i == text.size();
}

bool ReadResponse(HINTERNET request, std::string& body) {
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) return false;
        if (available == 0) return true;
        if (body.size() + available > kMaxResponseBytes) return false;
        const size_t offset = body.size();
        body.resize(offset + available);
        DWORD read = 0;
        if (!WinHttpReadData(request, body.data() + offset, available, &read)) return false;
        body.resize(offset + read);
        if (read == 0) return true;
    }
}

// GET https://api.github.com/<path>; returns the body of a 200 response.
bool HttpsGet(const std::wstring& host, const std::wstring& path, const std::wstring& userAgent, std::string& body) {
    HINTERNET session = WinHttpOpen(userAgent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        // Windows < 8.1 does not know WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY.
        session = WinHttpOpen(userAgent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (!session) return false;
    {
        std::lock_guard<std::mutex> lock(g_sessionMutex);
        if (g_cancelled.load()) {
            WinHttpCloseHandle(session);
            return false;
        }
        g_session = session;
    }
    WinHttpSetTimeouts(session, static_cast<int>(kTimeoutMs), static_cast<int>(kTimeoutMs), static_cast<int>(kTimeoutMs), static_cast<int>(kTimeoutMs));

    bool ok = false;
    HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (request) {
        const wchar_t* headers = L"Accept: application/vnd.github+json\r\n";
        if (WinHttpSendRequest(request, headers, static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(request, nullptr)) {
            DWORD status = 0;
            DWORD size = sizeof(status);
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                                    &status, &size, WINHTTP_NO_HEADER_INDEX) &&
                status == 200) {
                ok = ReadResponse(request, body);
            }
        }
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    {
        std::lock_guard<std::mutex> lock(g_sessionMutex);
        if (g_session == session) {
            g_session = nullptr;
            WinHttpCloseHandle(session);
        }
    }
    return ok && !g_cancelled.load();
}

} // namespace

bool ParseReleaseTag(const std::string& json, std::string& tagOut) {
    // Top-level "tag_name": "<value>" of the GitHub release object.
    const std::string key = "\"tag_name\"";
    size_t pos = json.find(key);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos + key.size());
    if (pos == std::string::npos) return false;
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return false;
    const size_t end = json.find('"', pos + 1);
    if (end == std::string::npos || end - pos - 1 > 32) return false;
    std::string tag = json.substr(pos + 1, end - pos - 1);
    std::array<int, 3> parsed{};
    if (!ParseSemVer(tag, parsed)) return false;
    tagOut = tag;
    return true;
}

bool IsNewerVersion(const std::string& candidate, const std::string& current) {
    std::array<int, 3> a{};
    std::array<int, 3> b{};
    if (!ParseSemVer(candidate, a) || !ParseSemVer(current, b)) return false;
    return a > b;
}

UpdateInfo CheckForUpdate(const std::string& currentVersion, const std::string& repo) {
    UpdateInfo info;
    // Only "owner/name" made of safe characters is accepted.
    for (char c : repo) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '/' || c == '-' || c == '_' || c == '.')) {
            return info;
        }
    }
    if (repo.empty() || repo.find('/') == std::string::npos) {
        return info;
    }
    std::string body;
    const std::wstring userAgent = L"MicMix/" + Widen(currentVersion);
    if (!HttpsGet(L"api.github.com", L"/repos/" + Widen(repo) + L"/releases/latest", userAgent, body)) {
        return info;
    }
    std::string tag;
    if (!ParseReleaseTag(body, tag)) {
        return info;
    }
    info.checked = true;
    info.version = (tag[0] == 'v' || tag[0] == 'V') ? tag.substr(1) : tag;
    info.url = "https://github.com/" + repo + "/releases/tag/" + tag;
    info.available = IsNewerVersion(tag, currentVersion);
    return info;
}

void CancelUpdateCheck() {
    g_cancelled.store(true);
    std::lock_guard<std::mutex> lock(g_sessionMutex);
    if (g_session) {
        // Closing the session makes blocking WinHTTP calls on the worker return.
        WinHttpCloseHandle(g_session);
        g_session = nullptr;
    }
}
