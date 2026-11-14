// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#include "controller_coding.hpp"

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "../platform/controller_layout.hpp"
#include "../platform/service_contract.hpp"
#include "app_log.hpp"

namespace evo {
namespace app {

namespace {

// HmiMain3D's exported template instances (nm -D: weak, defined). The usage object is one pointer-sized value,
// returned in r0 (read in pushbuttonToKeycode: the result of get() is stored, then its address is passed to get()).
const char kGetUsage[] = "_ZN9hmiplugin3getI17IHmiVariantServerEENS_25CPluginInstanceCacheUsageIT_EEv";
const char kUsageGet[] = "_ZNK9hmiplugin25CPluginInstanceCacheUsageI17IHmiVariantServerE3getEv";
const unsigned kIsVariantSetSlot = 3;  // CHmiVariantServer vtable (libhmicoding.so): isVariantSet(unsigned)
const unsigned kGetRoleSlot = 21;      // getRole()

typedef uintptr_t (*GetUsageFn)();
typedef void* (*UsageGetFn)(const uintptr_t* usage);
typedef bool (*IsVariantSetFn)(void* server, unsigned id);
typedef int (*GetRoleFn)(void* server);

}  // namespace

bool writeControllerCoding() {
    GetUsageFn getUsage = reinterpret_cast<GetUsageFn>(dlsym(RTLD_DEFAULT, kGetUsage));
    UsageGetFn usageGet = reinterpret_cast<UsageGetFn>(dlsym(RTLD_DEFAULT, kUsageGet));
    void* server = nullptr;
    if (getUsage && usageGet) {
        const uintptr_t usage = getUsage();
        server = usageGet(&usage);
    }
    void** vtable = server ? *reinterpret_cast<void***>(server) : nullptr;
    if (!vtable || !vtable[kIsVariantSetSlot] || !vtable[kGetRoleSlot]) {
        alog("controller coding: iDrive's variant server not found (%s %s): the service keeps G-style keys",
             getUsage ? "get" : "no get", usageGet ? "usage" : "no usage");
        unlink(contract::kControllerFile);
        return false;
    }
    IsVariantSetFn isSet = reinterpret_cast<IsVariantSetFn>(vtable[kIsVariantSetSlot]);
    GetRoleFn role = reinterpret_cast<GetRoleFn>(vtable[kGetRoleSlot]);
    aa::ControllerCoding coding = aa::defaultControllerCoding();
    coding.known = true;
    coding.gStyle = isSet(server, aa::kVariantGStyle);
    coding.v5141 = isSet(server, aa::kVariant5141);
    coding.v4e59 = isSet(server, aa::kVariant4E59);
    coding.v4f1e = isSet(server, aa::kVariant4F1E);
    coding.v5486 = isSet(server, aa::kVariant5486);
    coding.role = role(server);
    char line[96];
    aa::formatControllerCoding(coding, line, sizeof(line));
    const char tmp[] = "/tmp/entry-aad-controller.tmp";
    FILE* f = fopen(tmp, "w");
    bool written = f && fprintf(f, "%s\n", line) > 0;
    if (f && fclose(f) != 0) {
        written = false;
    }
    if (!written || rename(tmp, contract::kControllerFile) != 0) {
        alog("controller coding: %s, not written", line);
        unlink(tmp);
        return false;
    }
    alog("controller coding: %s (%s controller)", line, coding.gStyle ? "G-style" : "F-style");
    return true;
}

}  // namespace app
}  // namespace evo
