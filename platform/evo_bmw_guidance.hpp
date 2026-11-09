// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Is the BMW navigation guiding? Read-only: one attribute getter on the HU session bus, nothing else.

// Lets the Android Auto arrows give way to BMW guidance (platform/kombi_nav.cpp, Arrows::start).
#ifndef ENTRY_AAD_PLATFORM_EVO_BMW_GUIDANCE_HPP
#define ENTRY_AAD_PLATFORM_EVO_BMW_GUIDANCE_HPP

namespace evo {
namespace kombi {

// 1 = BMW guidance active, 0 = not active, -1 = unknown (bus or navigation not available).
// May block up to 1.5 s. Call from one thread only; reconnects by itself.
int probeBmwGuidance();
// Host tests: another D-Bus address than the car's session bus.
void setGuidanceBusForTest(const char* address);

}  // namespace kombi
}  // namespace evo

#endif
