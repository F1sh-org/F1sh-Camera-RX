#include "f1sh_camera_rx.h"
#include <math.h>

static gboolean on_rotate_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    App *app = (App*)user_data;
    if (!app) return FALSE;

    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    // Clear background
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);

    // Scale drawing to widget size so it looks good on different DPIs
    double scale = fmin((double)w, (double)h) / 260.0;
    double bw = 60.0 * scale, bh = 140.0 * scale; // base vertical rectangle sizes
    double lens_r = 14.0 * scale;

    // Center and rotate
    double cx = w / 2.0;
    double cy = h / 2.0;
    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    double angle = (double)app->config.rotate * (G_PI / 2.0);
    cairo_rotate(cr, angle);

    // Compute rectangle position (upper half)
    double y_offset = -bh * 0.4; // move up to appear in the upper half
    double x = -bw / 2.0;
    double y = -bh / 2.0 + y_offset;

    // Draw filled body and outline
    cairo_set_line_width(cr, 2.0 * scale);
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9); // fill color
    cairo_rectangle(cr, x, y, bw, bh);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_stroke(cr);

    // Lens: outer (light) and inner (dark)
    double lens_x = x + bw * 0.5; // center horizontally
    double lens_y = y + bh * 0.25; // center of the upper half vertically
    cairo_set_source_rgb(cr, 0.75, 0.75, 0.75);
    cairo_arc(cr, lens_x, lens_y, lens_r, 0, 2 * G_PI);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_stroke(cr);

    double inner_r = lens_r * 0.45;
    cairo_arc(cr, lens_x, lens_y, inner_r, 0, 2 * G_PI);
    cairo_fill(cr);

    cairo_restore(cr);
    return FALSE;
}

static void on_rotate_radio_toggled(GtkToggleButton *toggle, gpointer user_data) {
    if (!gtk_toggle_button_get_active(toggle)) return;
    App *app = (App*)user_data;
    if (!app) return;

    gpointer val = g_object_get_data(G_OBJECT(toggle), "rotate-val");
    int rotate = GPOINTER_TO_INT(val);
    app->config.rotate = rotate;
    if (app->rotate_spin && GTK_IS_SPIN_BUTTON(app->rotate_spin)) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->rotate_spin), app->config.rotate);
    }

    // Queue redraw of the drawing area if present
    if (app->rotate_window) {
        GtkWidget *draw = (GtkWidget*)g_object_get_data(G_OBJECT(app->rotate_window), "rotate-draw");
        if (draw) gtk_widget_queue_draw(draw);
    }

    char status[64];
    snprintf(status, sizeof(status), "Rotate set to %d", app->config.rotate);
    ui_update_status(app, status);
}

static void on_ok_clicked(GtkButton *button __attribute__((unused)), gpointer user_data) {
    App *app = (App*)user_data;
    if (!app) return;
    on_widget_destroy(app->rotate_window, app);
    app->rotate_window = NULL;
    on_open_stream_clicked(NULL, app);
}

void ui_rotate(App *app) {
    if (!app) return;

    app->rotate_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app->rotate_window), "Rotate");
    gtk_window_set_transient_for(GTK_WINDOW(app->rotate_window), GTK_WINDOW(app->window));
    gtk_window_set_modal(GTK_WINDOW(app->rotate_window), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(app->rotate_window), 600, 400);
    gtk_window_set_resizable(GTK_WINDOW(app->rotate_window), FALSE);
    gtk_window_set_position(GTK_WINDOW(app->rotate_window), GTK_WIN_POS_CENTER);
    g_signal_connect(app->rotate_window, "destroy", G_CALLBACK(on_widget_destroy), app);

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add(GTK_CONTAINER(app->rotate_window), outer);
    gtk_widget_set_margin_top(outer, 12);
    gtk_widget_set_margin_bottom(outer, 12);
    gtk_widget_set_margin_start(outer, 12);
    gtk_widget_set_margin_end(outer, 12);

    // Layout: drawing area on left, controls on the right
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(outer), hbox, TRUE, TRUE, 0);

    // Drawing area
    GtkWidget *draw = gtk_drawing_area_new();
    gtk_widget_set_size_request(draw, 300, 220);
    gtk_widget_set_hexpand(draw, FALSE);
    gtk_widget_set_vexpand(draw, FALSE);
    g_signal_connect(draw, "draw", G_CALLBACK(on_rotate_draw), app);
    g_object_set_data(G_OBJECT(app->rotate_window), "rotate-draw", draw);
    gtk_box_pack_start(GTK_BOX(hbox), draw, FALSE, FALSE, 0);

    // Control column (radio buttons + OK)
    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(hbox), controls, TRUE, TRUE, 0);

    const char *labels[4] = { "0: None", "1: 90°", "2: 180°", "3: 270°" };
    GSList *group = NULL;
    for (int i = 0; i < 4; ++i) {
        GtkWidget *rb = gtk_radio_button_new_with_label(group, labels[i]);
        group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(rb));
        g_object_set_data(G_OBJECT(rb), "rotate-val", GINT_TO_POINTER(i));
        g_signal_connect(rb, "toggled", G_CALLBACK(on_rotate_radio_toggled), app);
        if (app->config.rotate == i) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rb), TRUE);
        gtk_box_pack_start(GTK_BOX(controls), rb, FALSE, FALSE, 0);
    }

    GtkWidget *ok = gtk_button_new_with_label("OK");
    g_signal_connect(ok, "clicked", G_CALLBACK(on_ok_clicked), app);
    gtk_box_pack_start(GTK_BOX(outer), ok, FALSE, FALSE, 8);

    gtk_widget_show_all(app->rotate_window);
}