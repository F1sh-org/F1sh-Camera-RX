#include "f1sh_camera_rx.h"
#include <stdarg.h>
#include <glib.h>

typedef struct {
    App *app;
    char *msg;
} LogIdleData;

// Buffer for logs generated before the UI is available
static GQueue *log_buffer = NULL;
static GMutex log_buffer_mutex = G_STATIC_MUTEX_INIT;
// Maximum number of buffered log messages to retain
#define MAX_BUFFERED_LOGS 1024

static gboolean append_log_idle(gpointer data)
{
    LogIdleData *d = (LogIdleData*)data;
    if (!d) return FALSE;

    if (d->app && d->app->log_view && GTK_IS_TEXT_VIEW(d->app->log_view)) {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(d->app->log_view));
        if (buffer) {
            GtkTextIter end;
            gtk_text_buffer_get_end_iter(buffer, &end);
            gtk_text_buffer_insert(buffer, &end, d->msg, -1);
            GtkTextMark *mark = gtk_text_buffer_create_mark(buffer, NULL, &end, FALSE);
            gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(d->app->log_view), mark, 0.0, TRUE, 0.0, 1.0);
            gtk_text_buffer_delete_mark(buffer, mark);
        }
    }
    g_free(d->msg);
    g_free(d);
    return FALSE;
}

static char *wrap_text(const char *s, int width)
{
    if (!s) return NULL;
    int len = (int)strlen(s);
    if (len <= width) return g_strdup(s);
    GString *out = g_string_new(NULL);
    int count = 0;
    int last_space = -1;
    for (int i = 0; s[i]; ++i) {
        char c = s[i];
        g_string_append_c(out, c);
        if (c == '\n') {
            count = 0;
            last_space = -1;
            continue;
        }
        if (c == ' ' || c == '\t') {
            last_space = out->len - 1;
        }
        count++;
        if (count >= width) {
            if (last_space != -1) {
                out->str[last_space] = '\n';
                count = out->len - last_space - 1;
                last_space = -1;
            } else {
                g_string_append_c(out, '\n');
                count = 0;
                last_space = -1;
            }
        }
    }
    return g_string_free(out, FALSE);
}

void ui_log(App *app, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    char *msg = g_strdup_vprintf(format, ap);
    va_end(ap);

    if (!msg) return;

    char *wrapped = wrap_text(msg, 120);
    g_free(msg);
    msg = wrapped ? wrapped : g_strdup("");
    int l = (int)strlen(msg);
    if (l == 0 || msg[l - 1] != '\n') {
        char *tmp = g_malloc(l + 2);
        memcpy(tmp, msg, l);
        tmp[l] = '\n';
        tmp[l + 1] = '\0';
        g_free(msg);
        msg = tmp;
    }

    // Append only to UI log view when available. Buffer otherwise.
    if (app && app->log_view) {
        LogIdleData *d = g_malloc(sizeof(LogIdleData));
        d->app = app;
        d->msg = msg;
        g_idle_add(append_log_idle, d);
    } else {
        // Buffer the message until UI is available, so startup messages aren't lost
        g_mutex_lock(&log_buffer_mutex);
        if (!log_buffer) log_buffer = g_queue_new();
        g_queue_push_tail(log_buffer, msg);
        while (g_queue_get_length(log_buffer) > MAX_BUFFERED_LOGS) {
            char *old = g_queue_pop_head(log_buffer);
            g_free(old);
        }
        g_mutex_unlock(&log_buffer_mutex);
    }
}

// Flush any buffered logs into the UI; intended to be called after UI created
void ui_log_flush_buffer(App *app)
{
    if (!app || !app->log_view) return;
    g_mutex_lock(&log_buffer_mutex);
    if (!log_buffer) {
        g_mutex_unlock(&log_buffer_mutex);
        return;
    }
    while (g_queue_get_length(log_buffer) > 0) {
        char *msg = g_queue_pop_head(log_buffer);
        LogIdleData *d = g_malloc(sizeof(LogIdleData));
        d->app = app;
        d->msg = msg;
        g_idle_add(append_log_idle, d);
    }
    g_queue_free(log_buffer);
    log_buffer = NULL;
    g_mutex_unlock(&log_buffer_mutex);
}

void ui_log_discard_buffer(void)
{
    g_mutex_lock(&log_buffer_mutex);
    if (!log_buffer) {
        g_mutex_unlock(&log_buffer_mutex);
        return;
    }
    while (g_queue_get_length(log_buffer) > 0) {
        char *msg = g_queue_pop_head(log_buffer);
        g_free(msg);
    }
    g_queue_free(log_buffer);
    log_buffer = NULL;
    g_mutex_unlock(&log_buffer_mutex);
}

static gboolean log_view_button_event(GtkWidget *widget __attribute__((unused)), GdkEvent *event __attribute__((unused)), gpointer user_data __attribute__((unused)))
{
    return TRUE; 
}

static gboolean log_view_enter_notify(GtkWidget *widget, GdkEvent *event __attribute__((unused)), gpointer user_data __attribute__((unused)))
{
    if (!widget) return FALSE;
    GdkWindow *win = gtk_widget_get_window(widget);
    if (!win) return FALSE;
    GdkDisplay *display = gdk_window_get_display(win);
    if (!display) return FALSE;
    GdkCursor *cursor = gdk_cursor_new_for_display(display, GDK_LEFT_PTR);
    gdk_window_set_cursor(win, cursor);
    g_object_unref(cursor);
    return FALSE;
}

static gboolean log_view_leave_notify(GtkWidget *widget, GdkEvent *event __attribute__((unused)), gpointer user_data __attribute__((unused)))
{
    if (!widget) return FALSE;
    GdkWindow *win = gtk_widget_get_window(widget);
    if (!win) return FALSE;
    gdk_window_set_cursor(win, NULL);
    return FALSE;
}

void ui_set_log_interactive(App *app, gboolean interactive)
{
    if (!app || !app->log_view) return;
    // Set focus and cursor visibility
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app->log_view), interactive);
    gtk_widget_set_can_focus(app->log_view, interactive);

    // Ensure event masks include enter/leave and button events
    gtk_widget_add_events(app->log_view, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

    // Connect or disconnect handlers based on interactive flag
    if (interactive) {
        // Remove swallowing handlers if present
        if (app->log_event_press_handler_id) {
            g_signal_handler_disconnect(app->log_view, app->log_event_press_handler_id);
            app->log_event_press_handler_id = 0;
        }
        if (app->log_event_release_handler_id) {
            g_signal_handler_disconnect(app->log_view, app->log_event_release_handler_id);
            app->log_event_release_handler_id = 0;
        }
        if (app->log_event_enter_handler_id) {
            g_signal_handler_disconnect(app->log_view, app->log_event_enter_handler_id);
            app->log_event_enter_handler_id = 0;
        }
        if (app->log_event_leave_handler_id) {
            g_signal_handler_disconnect(app->log_view, app->log_event_leave_handler_id);
            app->log_event_leave_handler_id = 0;
        }
    } else {
        // Add handlers if missing
        if (!app->log_event_press_handler_id) {
            app->log_event_press_handler_id = g_signal_connect(app->log_view, "button-press-event", G_CALLBACK(log_view_button_event), app);
        }
        if (!app->log_event_release_handler_id) {
            app->log_event_release_handler_id = g_signal_connect(app->log_view, "button-release-event", G_CALLBACK(log_view_button_event), app);
        }
        if (!app->log_event_enter_handler_id) {
            app->log_event_enter_handler_id = g_signal_connect(app->log_view, "enter-notify-event", G_CALLBACK(log_view_enter_notify), app);
        }
        if (!app->log_event_leave_handler_id) {
            app->log_event_leave_handler_id = g_signal_connect(app->log_view, "leave-notify-event", G_CALLBACK(log_view_leave_notify), app);
        }
    }
}
