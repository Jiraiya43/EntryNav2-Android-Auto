// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Read-only diagnostics on the car: no UUID registration, configuration writes, video surface or TCP server.
// A developer tool, not part of a release (make dev-tools).
#include <dbus/dbus.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "alps_spp.hpp"
#include "evo_log.hpp"
#include "evo_process.hpp"

namespace {
volatile sig_atomic_t stopped = 0;
void stop(int) {
    stopped = 1;
}
void expired(int) {
    const char msg[] = "evo-check: time limit reached, closing the probe.\n";
    ssize_t n = write(STDERR_FILENO, msg, sizeof(msg) - 1);
    (void)n;
    _exit(124);  // Diagnostic-only process: OS closes its D-Bus sockets; no configuration to restore.
}
bool command(const char* binary, const char* a, const char* b = nullptr, const char* c = nullptr,
             const char* d = nullptr, const char* e = nullptr) {
    if (stopped) {
        return false;
    }
    const char* values[] = {a, b, c, d, e};
    std::vector<std::string> args;
    printf("\n--- %s", binary);
    for (size_t i = 0; i < 5; ++i) {
        if (values[i]) {
            args.push_back(values[i]);
            printf(" %s", values[i]);
        }
    }
    puts(" ---");
    std::string output;
    evo::CommandReport detail;
    bool ok = evo::runCommand(binary, args, output, 2000, &detail);
    // LayerManagerControl can report a missing layer/surface yet exit with status zero.
    if (detail.text.find("LayerManagerService returned: failed") != std::string::npos) {
        ok = false;
    }
    printf("%s", detail.text.c_str());  // Preserve failed output for diagnosis, never parse it as a valid scene.
    printf("\n[%s] code=%d signal=%d spawn_error=%d timed_out=%d truncated=%d\n", ok ? "OK" : "FAILED", detail.exitCode,
           detail.signal, detail.spawnError, detail.timedOut, detail.truncated);
    return ok;
}
bool readText(const char* path, std::string& out, size_t limit = 16384) {
    out.clear();
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        return false;
    }
    char buf[1024];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        out.append(buf, static_cast<size_t>(n));
        if (out.size() > limit) {
            close(fd);
            out.clear();
            return false;
        }
    }
    close(fd);
    if (n < 0) {
        out.clear();
        return false;
    }
    return true;
}
std::string xmlField(const std::string& block, const char* tag) {
    std::string start = std::string("<") + tag + ">", end = std::string("</") + tag + ">";
    size_t a = block.find(start), b = block.find(end);
    if (a == std::string::npos || b == std::string::npos || b < a + start.size()) {
        return "";
    }
    return block.substr(a + start.size(), b - a - start.size());
}
bool hexValue(const std::string& value) {
    return value.size() >= 3 && value.size() <= 18 && value.substr(0, 2) == "0x" &&
           value.find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos;
}
void alpsConfigSummary() {
    puts("\n--- Static Alps configuration: supported services and SPP interface only ---");
    std::string text;
    if (!readText("/etc/alps/evolution/evo_config.xml", text, 262144)) {
        puts("[MISSING/UNREADABLE]");
        return;
    }
    size_t pos = 0;
    unsigned count = 0;
    while ((pos = text.find("<configuration>", pos)) != std::string::npos) {
        size_t end = text.find("</configuration>", pos);
        if (end == std::string::npos) {
            break;
        }
        std::string block = text.substr(pos, end - pos);
        pos = end + 16;
        std::string name = xmlField(block, "name");
        if (name != "\"supported_services\"" && name != "\"spp_data_interface\"") {
            continue;
        }
        std::string id = xmlField(block, "id"), instance = xmlField(block, "instance"),
                    value = xmlField(block, "value");
        if (!hexValue(id) || !hexValue(instance) || !hexValue(value)) {
            puts("[UNEXPECTED FORMAT]");
            continue;
        }
        printf("%s id=%s instance=%s value=%s\n", name.c_str(), id.c_str(), instance.c_str(), value.c_str());
        ++count;
    }
    printf("%u static entries kept. This does not prove that an instance is free at run time.\n", count);
}
void context() {
    const char* busybox = "/bin/busybox.nosuid";
    command(busybox, "uname", "-a");
    command(busybox, "ls", "-l", "/dev/dri/card0", "/dev/cmem", "/dev/rpmsg-dce");
    command(busybox, "ls", "-l", "/run/system/0/wayland-0", "/run/dbus/system_bus_socket");
    command(busybox, "ls", "-l", "/dev/btspp0", "/dev/btspp1", "/dev/input");
    const char* paths[] = {"/etc/version",
                           "/proc/uptime",
                           "/proc/modules",
                           "/sys/class/graphics/fb0/name",
                           "/sys/class/graphics/fb0/virtual_size",
                           "/sys/class/graphics/fb0/bits_per_pixel",
                           "/sys/class/graphics/fb0/stride",
                           "/sys/class/graphics/fb0/modes",
                           "/sys/bus/rpmsg/devices/rpmsg0/name",
                           "/proc/bus/input/devices",
                           "/proc/asound/cards",
                           "/proc/asound/pcm"};
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]) && !stopped; ++i) {
        printf("\n--- %s ---\n", paths[i]);
        std::string text;
        if (readText(paths[i], text)) {
            printf("%s\n", text.c_str());
        } else {
            puts("[MISSING/UNREADABLE/TOO LONG] Some entries are optional on this head unit.");
        }
    }
    if (!stopped) {
        alpsConfigSummary();
    }
    // Only documented queries. Never register a MOST block or inject a message.
    command("/usr/local/bin/vmostctl", "--list-fblock");
    command("/usr/local/bin/vmostctl", "--apps");
    command(busybox, "ifconfig", "wlan0");
    command(busybox, "ip", "route", "show");
    command(busybox, "netstat", "-tuln");
    // Avoid the HU's iptables shell wrapper (-w): invoke the binary directly without waiting on a lock.
    command("/usr/sbin/xtables-multi", "iptables", "-S");
    command(busybox, "df", "-h", "/tmp");
    puts("\nContext collected. Look for FAILED and MISSING as well; no setting changed.");
}
void usage() {
    puts(
        "evo-check --scene | --context | --bluetooth | --monitor [seconds 1..60] | --help\n"
        "evo-check --dbus-names | --dbus-introspect <destination> <path> | --dbus-watch <path> [seconds 1..3600]\n"
        "Read and watch only: no UUID, configuration, surface or TCP port.\n"
        "--scene: IVI scene and candidate layers; each command is bounded to 2 seconds.\n"
        "--context: video, MOST, audio and inputs, network, ports and firewall, read only.\n"
        "--bluetooth: the Alps API, then spp_data_interface of instances 0 and 1.\n"
        "--monitor: bounded watch (30 seconds by default), Ctrl-C to end it.\n"
        "--dbus-names: system bus names, with the pid and command line of their process.\n"
        "--dbus-introspect: XML description of one bus object.\n"
        "--dbus-watch: messages on one bus path (60 seconds by default), arguments decoded.");
}
// ---- Read-only D-Bus survey (the HU has no dbus-send/dbus-monitor). Nothing is sent to the car network:
// --dbus-names                    well-known bus names with their owner PID and command line
// --dbus-introspect <dest> <path> introspection XML of one object
// --dbus-watch <path> [seconds]   every message on that object path (monitor mode, else signal match), args decoded
union DbusBasic {
    unsigned char y;
    dbus_bool_t b;
    dbus_int16_t n;
    dbus_uint16_t q;
    dbus_int32_t i;
    dbus_uint32_t u;
    dbus_int64_t x;
    dbus_uint64_t t;
    double d;
    const char* s;
    int h;
};
void dbusDump(DBusMessageIter* it, int depth) {
    for (;;) {
        const int t = dbus_message_iter_get_arg_type(it);
        if (t == DBUS_TYPE_INVALID) {
            return;
        }
        printf("%*s", depth * 2 + 2, "");
        if (t == DBUS_TYPE_ARRAY && dbus_type_is_fixed(dbus_message_iter_get_element_type(it)) &&
            dbus_message_iter_get_element_type(it) == DBUS_TYPE_BYTE) {
            DBusMessageIter sub;
            dbus_message_iter_recurse(it, &sub);
            const unsigned char* data = nullptr;
            int n = 0;
            dbus_message_iter_get_fixed_array(&sub, &data, &n);
            printf("ay[%d]", n);
            for (int k = 0; k < n && k < 256; ++k) {
                printf(" %02x", data[k]);
            }
            printf("%s\n", n > 256 ? " ..." : "");
        } else if (dbus_type_is_basic(t)) {
            DbusBasic v;
            memset(&v, 0, sizeof(v));
            dbus_message_iter_get_basic(it, &v);
            switch (t) {
                case DBUS_TYPE_BYTE:
                    printf("y %u\n", v.y);
                    break;
                case DBUS_TYPE_BOOLEAN:
                    printf("b %u\n", static_cast<unsigned>(v.b));
                    break;
                case DBUS_TYPE_INT16:
                    printf("n %d\n", v.n);
                    break;
                case DBUS_TYPE_UINT16:
                    printf("q %u\n", v.q);
                    break;
                case DBUS_TYPE_INT32:
                    printf("i %d\n", v.i);
                    break;
                case DBUS_TYPE_UINT32:
                    printf("u %u\n", v.u);
                    break;
                case DBUS_TYPE_INT64:
                    printf("x %lld\n", static_cast<long long>(v.x));
                    break;
                case DBUS_TYPE_UINT64:
                    printf("t %llu\n", static_cast<unsigned long long>(v.t));
                    break;
                case DBUS_TYPE_DOUBLE:
                    printf("d %g\n", v.d);
                    break;
                case DBUS_TYPE_UNIX_FD:
                    printf("h %d\n", v.h);
                    break;
                default:
                    printf("%c \"%s\"\n", t, v.s ? v.s : "");
                    break;
            }
        } else {
            DBusMessageIter sub;
            dbus_message_iter_recurse(it, &sub);
            printf("%c {\n", t);
            if (depth < 12) {
                dbusDump(&sub, depth + 1);
            }
            printf("%*s}\n", depth * 2 + 2, "");
        }
        if (!dbus_message_iter_next(it)) {
            return;
        }
    }
}
DBusMessage* dbusCall(DBusConnection* c, const char* dest, const char* path, const char* iface, const char* member,
                      const char* sarg) {
    DBusMessage* m = dbus_message_new_method_call(dest, path, iface, member);
    if (!m) {
        return nullptr;
    }
    if (sarg) {
        dbus_message_append_args(m, DBUS_TYPE_STRING, &sarg, DBUS_TYPE_INVALID);
    }
    DBusError e;
    dbus_error_init(&e);
    DBusMessage* r = dbus_connection_send_with_reply_and_block(c, m, 3000, &e);
    dbus_message_unref(m);
    if (!r) {
        printf("  error %s: %s\n", member, e.message ? e.message : "?");
        dbus_error_free(&e);
    }
    return r;
}
int dbusTool(int argc, char** argv) {
    const bool listNames = argc == 2 && !strcmp(argv[1], "--dbus-names");
    const bool introspect = argc == 4 && !strcmp(argv[1], "--dbus-introspect");
    const bool watch = (argc == 3 || argc == 4) && !strcmp(argv[1], "--dbus-watch");
    long secs = 60;
    if (watch && argc == 4) {
        char* end;
        errno = 0;
        secs = strtol(argv[3], &end, 10);
        if (errno || end == argv[3] || *end || secs < 1 || secs > 3600) {
            usage();
            return 2;
        }
    }
    if (!listNames && !introspect && !watch) {
        usage();
        return 2;
    }
    DBusError e;
    dbus_error_init(&e);
    DBusConnection* c = dbus_bus_get_private(DBUS_BUS_SYSTEM, &e);
    if (!c) {
        printf("system bus unavailable: %s\n", e.message ? e.message : "?");
        return 1;
    }
    dbus_connection_set_exit_on_disconnect(c, FALSE);
    const char* bus = "org.freedesktop.DBus";
    if (listNames) {
        DBusMessage* r = dbusCall(c, bus, "/org/freedesktop/DBus", bus, "ListNames", nullptr);
        char** names = nullptr;
        int n = 0;
        if (!r || !dbus_message_get_args(r, &e, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &names, &n, DBUS_TYPE_INVALID)) {
            return 1;
        }
        for (int k = 0; k < n; ++k) {
            if (names[k][0] == ':') {
                continue;
            }
            unsigned pid = 0;
            DBusMessage* pr = dbusCall(c, bus, "/org/freedesktop/DBus", bus, "GetConnectionUnixProcessID", names[k]);
            if (pr) {
                dbus_message_get_args(pr, nullptr, DBUS_TYPE_UINT32, &pid, DBUS_TYPE_INVALID);
                dbus_message_unref(pr);
            }
            char cmd[160] = "";
            char fn[64];
            snprintf(fn, sizeof(fn), "/proc/%u/cmdline", pid);
            FILE* f = pid ? fopen(fn, "r") : nullptr;
            if (f) {
                size_t got = fread(cmd, 1, sizeof(cmd) - 1, f);
                fclose(f);
                cmd[got] = 0;
                for (size_t z = 0; z + 1 < got; ++z) {
                    if (!cmd[z]) {
                        cmd[z] = ' ';
                    }
                }
            }
            printf("%s  pid %u  %s\n", names[k], pid, cmd);
        }
        dbus_free_string_array(names);
        dbus_message_unref(r);
    } else if (introspect) {
        DBusMessage* r = dbusCall(c, argv[2], argv[3], "org.freedesktop.DBus.Introspectable", "Introspect", nullptr);
        const char* xml = nullptr;
        if (r && dbus_message_get_args(r, nullptr, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID)) {
            puts(xml);
        }
        if (r) {
            dbus_message_unref(r);
        }
    } else {
        char rule[300];
        snprintf(rule, sizeof(rule), "path='%s'", argv[2]);
        bool monitor = false;
        {  // dbus >= 1.9.10: monitor mode sees method calls and replies too
            DBusMessage* m = dbus_message_new_method_call(bus, "/org/freedesktop/DBus",
                                                          "org.freedesktop.DBus.Monitoring", "BecomeMonitor");
            const char* rules[1] = {rule};
            const char** pr = rules;
            dbus_uint32_t flags = 0;
            dbus_message_append_args(m, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &pr, 1, DBUS_TYPE_UINT32, &flags,
                                     DBUS_TYPE_INVALID);
            DBusMessage* r = dbus_connection_send_with_reply_and_block(c, m, 3000, &e);
            dbus_message_unref(m);
            if (r) {
                monitor = true;
                dbus_message_unref(r);
            } else {
                dbus_error_free(&e);
            }
        }
        if (!monitor) {
            char r2[340];
            snprintf(r2, sizeof(r2), "eavesdrop='true',%s", rule);
            dbus_bus_add_match(c, r2, &e);
            if (dbus_error_is_set(&e)) {
                dbus_error_free(&e);
                dbus_bus_add_match(c, rule, &e);
            }
            if (dbus_error_is_set(&e)) {
                printf("subscription refused: %s\n", e.message);
                return 1;
            }
        }
        printf("watching %s for %ld s (%s)\n", argv[2], secs, monitor ? "monitor mode" : "subscription");
        const time_t end = time(nullptr) + secs;
        while (time(nullptr) < end && dbus_connection_read_write(c, 200)) {
            DBusMessage* m;
            while ((m = dbus_connection_pop_message(c)) != nullptr) {
                static const char* types[] = {"?", "call", "return", "error", "signal"};
                const int ty = dbus_message_get_type(m);
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                printf("[%ld.%03ld] %s %s -> %s %s %s.%s\n", static_cast<long>(ts.tv_sec), ts.tv_nsec / 1000000,
                       types[ty >= 0 && ty <= 4 ? ty : 0],
                       dbus_message_get_sender(m) ? dbus_message_get_sender(m) : "-",
                       dbus_message_get_destination(m) ? dbus_message_get_destination(m) : "*",
                       dbus_message_get_path(m) ? dbus_message_get_path(m) : "-",
                       dbus_message_get_interface(m) ? dbus_message_get_interface(m) : "-",
                       dbus_message_get_member(m) ? dbus_message_get_member(m) : "-");
                DBusMessageIter it;
                if (dbus_message_iter_init(m, &it)) {
                    dbusDump(&it, 0);
                }
                dbus_message_unref(m);
            }
        }
    }
    dbus_connection_close(c);
    dbus_connection_unref(c);
    return 0;
}
}  // namespace
int main(int argc, char** argv) {
    if (argc >= 2 && !strncmp(argv[1], "--dbus-", 7)) {
        setvbuf(stdout, nullptr, _IOLBF, 0);
        return dbusTool(argc, argv);
    }
    if (argc == 2 && !strcmp(argv[1], "--help")) {
        usage();
        return 0;
    }
    bool scene = (argc == 2 || argc == 3) && !strcmp(argv[1], "--scene");  // optional: another CLI (host tests)
    bool surroundings = argc == 2 && !strcmp(argv[1], "--context");
    bool bluetooth = argc == 2 && !strcmp(argv[1], "--bluetooth");
    bool monitor = (argc == 2 || argc == 3) && !strcmp(argv[1], "--monitor");
    if (!scene && !surroundings && !bluetooth && !monitor) {
        usage();
        return 2;
    }
    unsigned seconds = 30;
    if (monitor && argc == 3) {
        char* end;
        errno = 0;
        unsigned long value = strtoul(argv[2], &end, 10);
        if (errno || end == argv[2] || *end || value < 1 || value > 60) {
            return 2;
        }
        seconds = static_cast<unsigned>(value);
    }
    setvbuf(stdout, nullptr, _IOLBF, 0);
    signal(SIGINT, stop);
    signal(SIGTERM, stop);
    signal(SIGHUP, stop);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGALRM, expired);
    alarm(monitor ? seconds + 10 : 30);
    if (!getenv("XDG_RUNTIME_DIR")) {
        setenv("XDG_RUNTIME_DIR", "/run/system/0", 1);
    }
    if (surroundings) {
        context();
        return stopped ? 1 : 0;
    }
    if (scene) {
        const char* ctl = argc == 3 ? argv[2] : "/usr/bin/LayerManagerControl";
        bool ok = command(ctl, "get", "scene");
        if (!stopped) {
            ok = command(ctl, "get", "screens") && ok;
        }
        if (!stopped) {
            ok = command(ctl, "get", "screen", "0") && ok;
        }
        if (!stopped) {
            ok = command(ctl, "get", "layers") && ok;
        }
        if (!stopped) {
            ok = command(ctl, "get", "surfaces") && ok;
        }
        command(ctl, "get", "keyboard", "focus");
        const char* candidates[] = {"0x100", "0x300", "0x2000", "0x3000"};
        for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]) && !stopped; ++i) {
            command(ctl, "get", "layer", candidates[i]);  // Candidate layers need not all exist.
        }
        const char* surfaces[] = {"0x101", "0x2001", "0x3001"};
        for (size_t i = 0; i < sizeof(surfaces) / sizeof(surfaces[0]) && !stopped; ++i) {
            command(ctl, "get", "surface", surfaces[i]);
        }
        puts("\nReading done. No surface or layer created or changed.");
        return ok && !stopped ? 0 : 1;
    }
    evo::AlpsSppLink bt;
    bt.setMonitorOnly(true);
    if (!bt.start(0)) {
        return 1;
    }
    if (bluetooth) {
        std::string xml = bt.introspect("/org/alps/evolution/hli");
        printf("--- API Alps ---\n%s\n", xml.c_str());
        bool ok = !xml.empty();
        for (uint8_t instance = 0; instance < 2 && !stopped; ++instance) {
            std::vector<uint8_t> cfg;
            bool read = bt.readConfiguration(instance, 0x6203, cfg, 2000);
            if (read && cfg.size() == 1) {
                printf("SPP instance=%u configuration=0x6203 value=%u\n", instance, cfg[0]);
            } else {
                printf("SPP instance=%u: configuration read not confirmed\n", instance);
                ok = false;
            }
        }
        puts("No UUID registered, no configuration changed.");
        return ok && !stopped ? 0 : 1;
    }
    printf("Watching Bluetooth for %u seconds. Nothing sent over SPP.\n", seconds);
    uint32_t begin = evo::monoMs();
    while (!stopped && bt.running() && evo::monoMs() - begin < seconds * 1000u) {
        usleep(100000);
    }
    bool alive = bt.running();
    bt.stop();
    puts("Watch over; D-Bus connection closed.");
    return alive ? 0 : 1;
}
