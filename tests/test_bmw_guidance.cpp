// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Tests platform/evo_bmw_guidance.cpp against a fake BMW navigation on a private session bus (dbus-run-session).

// Checks the exact destination, path, interface and member observed on the car, the boolean reply, and that every
// other outcome is "unknown" and never "active".
#include <assert.h>
#include <dbus/dbus.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "evo_bmw_guidance.hpp"

namespace {
volatile int g_mode = 0;  // 0 false, 1 true, 2 wrong reply type, 3 error
volatile int g_calls = 0;
volatile bool g_quit = false;
volatile bool g_ready = false;

void* fakeNavigation(void*) {
    DBusError e;
    dbus_error_init(&e);
    DBusConnection* c = dbus_bus_get_private(DBUS_BUS_SESSION, &e);
    assert(c);
    dbus_connection_set_exit_on_disconnect(c, FALSE);
    assert(dbus_bus_request_name(c, "com.bmw.navigation.guiding.Guiding.MainInstance", 0, &e) ==
           DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER);
    g_ready = true;
    while (!g_quit && dbus_connection_read_write(c, 50)) {
        DBusMessage* m;
        while ((m = dbus_connection_pop_message(c)) != 0) {
            if (dbus_message_get_type(m) == DBUS_MESSAGE_TYPE_METHOD_CALL) {
                assert(!strcmp(dbus_message_get_path(m), "/com/bmw/navigation/guiding/Guiding/MainInstance"));
                assert(!strcmp(dbus_message_get_interface(m), "com.bmw.navigation.guiding.Guiding"));
                assert(!strcmp(dbus_message_get_member(m), "getGuidanceActiveAttribute"));
                ++g_calls;
                DBusMessage* r;
                if (g_mode == 3) {
                    r = dbus_message_new_error(m, "org.freedesktop.DBus.Error.Failed", "test");
                } else {
                    r = dbus_message_new_method_return(m);
                    if (g_mode == 2) {
                        dbus_int32_t i = 1;
                        dbus_message_append_args(r, DBUS_TYPE_INT32, &i, DBUS_TYPE_INVALID);
                    } else {
                        dbus_bool_t v = g_mode == 1;
                        dbus_message_append_args(r, DBUS_TYPE_BOOLEAN, &v, DBUS_TYPE_INVALID);
                    }
                }
                dbus_connection_send(c, r, 0);
                dbus_connection_flush(c);
                dbus_message_unref(r);
            }
            dbus_message_unref(m);
        }
    }
    dbus_connection_close(c);
    dbus_connection_unref(c);
    return 0;
}
}  // namespace

int main() {
    const char* bus = getenv("DBUS_SESSION_BUS_ADDRESS");
    assert(bus && *bus);
    dbus_threads_init_default();

    evo::kombi::setGuidanceBusForTest("unix:path=/nonexistent/session_bus");
    assert(evo::kombi::probeBmwGuidance() == -1);  // no bus: unknown

    evo::kombi::setGuidanceBusForTest(bus);
    assert(evo::kombi::probeBmwGuidance() == -1);  // no navigation on the bus: unknown

    pthread_t t;
    assert(pthread_create(&t, 0, fakeNavigation, 0) == 0);
    while (!g_ready) {
        usleep(10000);
    }

    g_mode = 0;
    assert(evo::kombi::probeBmwGuidance() == 0);
    g_mode = 1;
    assert(evo::kombi::probeBmwGuidance() == 1);
    g_mode = 2;
    assert(evo::kombi::probeBmwGuidance() == -1);  // unexpected reply: unknown, never active
    g_mode = 3;
    assert(evo::kombi::probeBmwGuidance() == -1);
    g_mode = 0;
    assert(evo::kombi::probeBmwGuidance() == 0);  // same connection keeps working
    assert(g_calls == 5);

    g_quit = true;
    pthread_join(t, 0);
    puts("PASS BMW guidance probe (exact names, boolean reply, unknown on anything else)");
    return 0;
}
