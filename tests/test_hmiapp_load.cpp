// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// ARM/QEMU: load libEntryAad.so the way ManagedPluginApp::init does (dlopen + dlsym "pluginAppInfo", create/destroy
// at +0x10/+0x14) with the HU's own libAppFramework/libCoreFramework, and check the vtable slots the app patches
// against the real bmw::PluginApp functions. create() is NOT called (it needs HmiMain3D's dispatcher).
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x)                                                 \
    do {                                                         \
        if (!(x)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); \
            return 1;                                            \
        }                                                        \
    } while (0)
int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "./libEntryAad.so";
    void* fw = dlopen("libAppFramework.A.so", RTLD_NOW | RTLD_GLOBAL);  // HmiMain3D links it: global in the HMI
    if (!fw) {
        fprintf(stderr, "%s\n", dlerror());
    }
    CHECK(fw);
    void* lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        fprintf(stderr, "%s\n", dlerror());
    }
    CHECK(lib);
    const unsigned char* info = static_cast<const unsigned char*>(dlsym(lib, "pluginAppInfo"));
    CHECK(info);
    uint16_t major, minor;
    uint32_t rhmi;
    const char* name;
    void* create;
    void* destroy;
    memcpy(&major, info, 2);
    memcpy(&minor, info + 2, 2);
    memcpy(&name, info + 4, 4);
    memcpy(&rhmi, info + 0xc, 4);
    memcpy(&create, info + 0x10, 4);
    memcpy(&destroy, info + 0x14, 4);
    CHECK(sizeof(void*) == 4 && major == 1 && minor == 0 && rhmi == 1 && create && destroy &&
          !strcmp(name, "Android Auto"));
    // Slots replaced by createApp(): onError 21, onInit 22, onFinalize 23 (from the vtable address point).
    void** vt = static_cast<void**>(dlsym(fw, "_ZTVN3bmw9PluginAppE"));
    CHECK(vt);
    void* onError = dlsym(fw, "_ZN3bmw9PluginApp7onErrorEjRKNS_11BasicStringIcEE");
    void* onInit = dlsym(fw, "_ZN3bmw9PluginApp6onInitEv");
    void* onFinalize = dlsym(fw, "_ZN3bmw9PluginApp10onFinalizeEv");
    CHECK(onError && onInit && onFinalize);
    CHECK(vt[2 + 21] == onError && vt[2 + 22] == onInit && vt[2 + 23] == onFinalize);
    CHECK(dlsym(RTLD_DEFAULT, "_ZTVN3bmw8RunnableE"));  // Runnable vtable used by the app's posted tasks
    puts(
        "PASS libEntryAad.so loaded as ManagedPluginApp does (the HU's HMI libraries); pluginAppInfo valid; "
        "slots onError/onInit/onFinalize = 21/22/23 confirmed on the real bmw::PluginApp");
    return 0;
}
