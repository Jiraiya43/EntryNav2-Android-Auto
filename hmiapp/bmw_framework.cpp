// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The app's own bmw::Runnable and bmw::PluginApp vtable, and small wrappers around BMW's handlers (bmw_framework.hpp).
#include "bmw_framework.hpp"

#include <dlfcn.h>
#include <string.h>

#include <new>

namespace evo {
namespace app {

namespace {

enum { kSlotOnError = 21, kSlotOnInit = 22, kSlotOnFinalize = 23, kVtblSlots = 34 };

void* g_vtbl[2 + kVtblSlots];  // the app's copy of bmw::PluginApp's primary vtable, with its offset and typeinfo

void runnableNoop(void*) {}

}  // namespace

void makeRunnable(AppRunnable& r, void (*run)(void*)) {
    void** vtbl = r.vtbl;
    void** base = reinterpret_cast<void**>(dlsym(RTLD_DEFAULT, "_ZTVN3bmw8RunnableE"));
    if (base) {
        memcpy(vtbl, base, 5 * sizeof(void*));
    } else {
        vtbl[0] = nullptr;
        vtbl[1] = nullptr;
    }
    vtbl[2] = reinterpret_cast<void*>(&runnableNoop);
    vtbl[3] = reinterpret_cast<void*>(&runnableNoop);
    vtbl[4] = reinterpret_cast<void*>(run);
    r.vptr = vtbl + 2;
}

void postRunnable(bmw::PluginApp* app, AppRunnable& r, unsigned delayMs) {
    app->post(reinterpret_cast<bmw::Runnable*>(&r), delayMs);
}

void addAction(bmw::PluginApp* app, unsigned actionId, ActionStub stub) {
    bmw::ActionDelegate d;
    d.object = app;
    d.stub = stub;
    app->addHandler(actionId, d);
}

void addStateHandler(bmw::PluginApp* app, unsigned state, unsigned kind, ActionStub stub) {
    bmw::ActionDelegate d;
    d.object = app;
    d.stub = stub;
    app->addHandler(state, kind, d);
}

void addCdsHandler(bmw::PluginApp* app, const char* property, CdsStub stub) {
    bmw::BasicString<char> name(property);
    bmw::CdsDelegate d;
    d.object = app;
    d.stub = stub;
    app->cdsAddPropertyChangedEventHandler(name, 0, d);
}

bool parcelBool(const bmw::Parcel& value) {
    bool b = false;
    value.readBool(b);
    return b;
}

void narrow(const bmw::BasicString<unsigned short>& value, char* text, size_t cap) {
    size_t n = 0;
    const unsigned short* p = value.raw();
    while (p && p[n] && n < cap - 1) {
        text[n] = (p[n] < 0x80) ? static_cast<char>(p[n]) : '?';
        ++n;
    }
    text[n] = 0;
}

bmw::PluginApp* newPluginApp(bmw::IDispatcher& dispatcher, const char* name, const bmw::Bundle& params,
                             const bmw::Bundle& config, const PluginAppHooks& hooks, void** bmwVtable) {
    void* mem = ::operator new(bmw::kPluginAppStorage);
    memset(mem, 0, bmw::kPluginAppStorage);
    bmw::PluginApp* app = new (mem) bmw::PluginApp(dispatcher, bmw::BasicString<char>(name ? name : "EntryAad"), 1, 0,
                                                   bmw::RHMI_INTERFACE_VERSION_1, params, config);
    // Own copy of bmw::PluginApp's primary vtable with onInit / onError / onFinalize replaced (no BMW headers).
    void** base = *reinterpret_cast<void***>(app);
    memcpy(g_vtbl, base - 2, sizeof(g_vtbl));
    g_vtbl[2 + kSlotOnInit] = reinterpret_cast<void*>(hooks.onInit);
    g_vtbl[2 + kSlotOnError] = reinterpret_cast<void*>(hooks.onError);
    g_vtbl[2 + kSlotOnFinalize] = reinterpret_cast<void*>(hooks.onFinalize);
    *reinterpret_cast<void***>(app) = g_vtbl + 2;
    *bmwVtable = base;
    return app;
}

void deletePluginApp(void* app) {
    void** vt = *reinterpret_cast<void***>(app);
    reinterpret_cast<void (*)(void*)>(vt[1])(app);  // deleting destructor
}

}  // namespace app
}  // namespace evo
