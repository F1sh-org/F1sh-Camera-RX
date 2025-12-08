#include "serial_service.h"

#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <glob.h>
#include <sys/stat.h>
#endif

static gboolean serial_array_contains(GPtrArray *arr, const gchar *port)
{
    for (guint i = 0; i < arr->len; ++i) {
        SerialPortInfo *info = g_ptr_array_index(arr, i);
        if (g_strcmp0(info->port, port) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

SerialPortInfo *serial_port_info_new(const gchar *port, const gchar *description)
{
    SerialPortInfo *info = g_new0(SerialPortInfo, 1);
    info->port = g_strdup(port);
    if (description && *description) {
        info->description = g_strdup(description);
    }
    return info;
}

void serial_port_info_free(SerialPortInfo *info)
{
    if (!info) {
        return;
    }
    g_free(info->port);
    g_free(info->description);
    g_free(info);
}

#ifdef _WIN32
static gboolean windows_port_exists(const char *dos_name)
{
    char target[256];
    if (QueryDosDeviceA(dos_name, target, sizeof(target)) != 0) {
        return TRUE;
    }
    DWORD err = GetLastError();
    if (err == ERROR_INSUFFICIENT_BUFFER) {
        return TRUE;
    }

    char device_path[32];
    g_snprintf(device_path, sizeof(device_path), "\\\\.\\%s", dos_name);
    HANDLE h = CreateFileA(device_path,
                           GENERIC_READ | GENERIC_WRITE,
                           0,
                           NULL,
                           OPEN_EXISTING,
                           0,
                           NULL);
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
        return TRUE;
    }
    err = GetLastError();
    return err == ERROR_ACCESS_DENIED || err == ERROR_GEN_FAILURE || err == ERROR_SHARING_VIOLATION;
}

static void discover_windows_ports(GPtrArray *arr)
{
    char com_name[8];
    for (int com = 1; com <= 256; ++com) {
        g_snprintf(com_name, sizeof(com_name), "COM%d", com);
        if (serial_array_contains(arr, com_name)) {
            continue;
        }
        if (windows_port_exists(com_name)) {
            g_ptr_array_add(arr, serial_port_info_new(com_name, "Windows COM"));
        }
    }
}
#else
static void add_glob_matches(GPtrArray *arr, const char *pattern, const char *label)
{
    glob_t globbuf;
    memset(&globbuf, 0, sizeof(globbuf));
    if (glob(pattern, 0, NULL, &globbuf) == 0) {
        for (size_t i = 0; i < globbuf.gl_pathc; ++i) {
            const char *path = globbuf.gl_pathv[i];
            struct stat st;
            if (stat(path, &st) == 0 && S_ISCHR(st.st_mode)) {
                if (!serial_array_contains(arr, path)) {
                    g_ptr_array_add(arr, serial_port_info_new(path, label));
                }
            }
        }
    }
    globfree(&globbuf);
}

static void discover_posix_ports(GPtrArray *arr)
{
    add_glob_matches(arr, "/dev/ttyUSB*", "USB Serial");
    add_glob_matches(arr, "/dev/ttyACM*", "ACM Serial");
    add_glob_matches(arr, "/dev/ttyAMA*", "AMA Serial");
    add_glob_matches(arr, "/dev/serial/by-id/*", "Serial by-id");
    add_glob_matches(arr, "/dev/cu.usbserial*", "macOS USB Serial");
    add_glob_matches(arr, "/dev/cu.usbmodem*", "macOS USB Modem");
}
#endif

GPtrArray *serial_service_list_ports(void)
{
    GPtrArray *ports = g_ptr_array_new_with_free_func((GDestroyNotify)serial_port_info_free);
#ifdef _WIN32
    discover_windows_ports(ports);
#else
    discover_posix_ports(ports);
#endif
    return ports;
}
