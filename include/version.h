#ifndef VERSION_H
#define VERSION_H

// Version information - automatically updated by GitHub Actions
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "1.0.9"
#endif

#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP "manual-build"
#endif

#ifndef GIT_COMMIT
#define GIT_COMMIT "unknown"
#endif

// GitHub repository information
#define GITHUB_REPO_OWNER "Cem512"
#define GITHUB_REPO_NAME "QSafe"

// OTA update configuration
#define OTA_CHECK_INTERVAL_MS (24 * 60 * 60 * 1000)  // Check every 24 hours
#define GITHUB_API_TIMEOUT_MS 10000                   // 10 second timeout for API calls

#endif
