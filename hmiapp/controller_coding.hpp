// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The car's key coding, read inside iDrive and written for the service (platform/controller_layout.hpp).
//
// iDrive decides which key each controller button is from coding variants (0xB80 G-style controller, and a few
// more), asked from its variant server: hmiplugin::get<IHmiVariantServer>(), then isVariantSet(id) (vtable slot 3)
// and getRole() (slot 21), as CHmiCCSController::pushbuttonToKeycode does in HmiMain3D. The app runs inside HmiMain3D,
// so it asks the same server; both functions are found with dlsym, nothing is linked. If one is missing or the server
// is not there yet, nothing is written and the service keeps its G-style table.
// Runs on the app thread (onInit).
#ifndef ENTRY_AAD_HMIAPP_CONTROLLER_CODING_HPP
#define ENTRY_AAD_HMIAPP_CONTROLLER_CODING_HPP

namespace evo {
namespace app {

// Reads the coding and writes contract::kControllerFile. False: not read (logged), the file is removed.
bool writeControllerCoding();

}  // namespace app
}  // namespace evo

#endif
