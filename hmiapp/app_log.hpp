// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The app's log, /var/local/data/AA/app.log: written in the maintenance profile only (hmiapp/log_profile.hpp).
#ifndef ENTRY_AAD_HMIAPP_APP_LOG_HPP
#define ENTRY_AAD_HMIAPP_APP_LOG_HPP

// The build's name in the logs. No build date: unchanged code gives the same file, so an update can
// tell that the iDrive needs no restart.
#define ENTRY_AA_APP_VERSION "entryaad-app-10"

namespace evo {
namespace app {

// One line: date, monotonic seconds, tag [EntryAadApp]. Bounded: past 512 KiB the file becomes app.old.log.
// Any thread.
void alog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

}  // namespace app
}  // namespace evo

#endif
