#ifndef SERIAL_SERVICE_H
#define SERIAL_SERVICE_H

#include <glib.h>

G_BEGIN_DECLS

/*
 * SerialService enumerates serial interfaces on the system.
 */
typedef struct SerialPortInfo {
    gchar *port;         /* Platform specific device path (e.g. COM3, /dev/ttyACM0) */
    gchar *description;  /* Optional short description, may be NULL */
} SerialPortInfo;

SerialPortInfo *serial_port_info_new(const gchar *port, const gchar *description);
void serial_port_info_free(SerialPortInfo *info);

/* Returns a GPtrArray of SerialPortInfo*. Caller owns the array and its contents. */
GPtrArray *serial_service_list_ports(void);

G_END_DECLS

#endif /* SERIAL_SERVICE_H */
