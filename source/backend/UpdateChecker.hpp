#pragma once
#include <string>

// Manages the background update checking thread for the engine.
// Exposes simple query functions so TitleState doesn't need to touch HTTP/SOC services.
class UpdateChecker {
public:
    // Starts the background update check thread (only if enabled in settings and not already checking).
    static void startCheck();

    // Returns true if the background thread has finished.
    static bool isFinished();

    // Returns true if the background thread is currently active.
    static bool isChecking();

    // Returns the version string found online, or empty if check failed / not finished.
    static std::string getOnlineVersion();

    // Returns the hardcoded version of the current build.
    static std::string getCurrentVersion();

    // Returns -1 if v1 < v2 (outdated), 0 if v1 == v2, 1 if v1 > v2.
    static int compareVersions(const std::string& v1, const std::string& v2);
};
