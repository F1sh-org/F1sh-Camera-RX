#include "f1sh_camera_rx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#if defined(__APPLE__)
#include <glob.h>
#include <sys/stat.h>
#endif
#endif

// Probe string and match substring
static const char *kProbe = "{\"status\":1}\n";
static const char *kMatch = "{\"status\":1}\n";

#if defined(_WIN32) || defined(__CYGWIN__)

static gboolean probe_windows_port(App *app, const char *name, char *out_read, size_t out_len)
{
    char path[32];
    if (strncmp(name, "COM", 3) == 0) {
        int n = atoi(name + 3);
        if (n >= 10) snprintf(path, sizeof(path), "\\\\.\\%s", name);
        else snprintf(path, sizeof(path), "%s", name);
    } else {
        snprintf(path, sizeof(path), "%s", name);
    }

    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;

    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(h, &dcb)) { CloseHandle(h); return FALSE; }
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_DISABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    if (!SetCommState(h, &dcb)) { CloseHandle(h); return FALSE; }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 400; // ms
    timeouts.WriteTotalTimeoutConstant = 200;
    SetCommTimeouts(h, &timeouts);

    DWORD written = 0;
    WriteFile(h, kProbe, (DWORD)strlen(kProbe), &written, NULL);

    DWORD read = 0;
    BOOL ok = ReadFile(h, out_read, (DWORD)(out_len - 1), &read, NULL);
    if (read > 0 && read < out_len) out_read[read] = '\0'; else out_read[0] = '\0';

    CloseHandle(h);
    if (!ok) return FALSE;
    return strstr(out_read, kMatch) != NULL;
}

#else // POSIX

static gboolean probe_posix_port(App *app, const char *path, char *out_read, size_t out_len)
{
    int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return FALSE;

    // Configure blocking mode for read/writes
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    struct termios tio;
    if (tcgetattr(fd, &tio) != 0) { close(fd); return FALSE; }

    cfmakeraw(&tio);
    cfsetspeed(&tio, B115200);
    tio.c_cflag &= ~CRTSCTS; // no flow control
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_cflag &= ~PARENB; // no parity
    tio.c_cflag &= ~CSTOPB; // 1 stop
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;

    // set read to return as soon as any data arrives or timeout
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 5; // 0.5s

    if (tcsetattr(fd, TCSANOW, &tio) != 0) { close(fd); return FALSE; }

    tcflush(fd, TCIOFLUSH);

    ssize_t w = write(fd, kProbe, strlen(kProbe));
    (void)w;

    // Wait up to 500ms for response
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 500 * 1000; // 500ms
    int sel = select(fd + 1, &rfds, NULL, NULL, &tv);
    ssize_t r = 0;
    if (sel > 0 && FD_ISSET(fd, &rfds)) {
        r = read(fd, out_read, out_len - 1);
        if (r > 0) out_read[r] = '\0'; else out_read[0] = '\0';
    } else {
        out_read[0] = '\0';
    }

    close(fd);
    return r > 0 && strstr(out_read, kMatch) != NULL;
}

#endif

char *serial_find_camera_port(App *app)
{
    if (!app) return NULL;

    char readbuf[1024];

#if defined(_WIN32) || defined(__CYGWIN__)
    // Try COM1..COM20
    for (int i = 1; i <= 20; ++i) {
        char name[16];
        snprintf(name, sizeof(name), "COM%d", i);
        if (probe_windows_port(app, name, readbuf, sizeof(readbuf))) {
            ui_log(app, "Found camera on %s (resp: %s)", name, readbuf[0] ? readbuf : "(empty)");
            return g_strdup(name);
        }
    }
#else
    // On macOS, device names are not strictly numbered; glob /dev/cu.* and /dev/tty.*
#if defined(__APPLE__)
    {
        glob_t g;
        int res = glob("/dev/cu.*", 0, NULL, &g);
        if (res == 0) {
            for (size_t i = 0; i < g.gl_pathc; ++i) {
                const char *p = g.gl_pathv[i];
                // filter likely USB serial devices
                if (strstr(p, "usb") || strstr(p, "modem") || strstr(p, "serial")) {
                    if (probe_posix_port(app, p, readbuf, sizeof(readbuf))) {
                        ui_log(app, "Found camera on %s (resp: %s)", p, readbuf[0] ? readbuf : "(empty)");
                        globfree(&g);
                        return g_strdup(p);
                    }
                }
            }
            globfree(&g);
        }

        res = glob("/dev/tty.*", 0, NULL, &g);
        if (res == 0) {
            for (size_t i = 0; i < g.gl_pathc; ++i) {
                const char *p = g.gl_pathv[i];
                if (strstr(p, "usb") || strstr(p, "modem") || strstr(p, "serial")) {
                    if (probe_posix_port(app, p, readbuf, sizeof(readbuf))) {
                        ui_log(app, "Found camera on %s (resp: %s)", p, readbuf[0] ? readbuf : "(empty)");
                        globfree(&g);
                        return g_strdup(p);
                    }
                }
            }
            globfree(&g);
        }
    }
#endif

    // Common POSIX device patterns to try (Linux and others)
    const char *patterns[] = {
        "/dev/ttyUSB%d",
        "/dev/ttyACM%d",
        "/dev/ttyS%d",
        "/dev/cu.usbmodem%d",
        "/dev/tty.usbmodem%d",
        "/dev/tty.usbserial-%d",
    };
    for (size_t p = 0; p < G_N_ELEMENTS(patterns); ++p) {
        for (int i = 0; i < 16; ++i) {
            char path[128];
            snprintf(path, sizeof(path), patterns[p], i);
            if (probe_posix_port(app, path, readbuf, sizeof(readbuf))) {
                ui_log(app, "Found camera on %s (resp: %s)", path, readbuf[0] ? readbuf : "(empty)");
                return g_strdup(path);
            }
        }
    }
#endif

    ui_log(app, "No camera COM port found");
    return NULL;
}
