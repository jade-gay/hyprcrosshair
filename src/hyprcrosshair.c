// hyprcrosshair.c
#include <gtk/gtk.h>
#include <adwaita.h>
#include <gtk4-layer-shell.h>
#include <gdk/gdk.h>
#include <gdk/wayland/gdkwayland.h>
#include <cairo.h>
#include <math.h>
#include <glib.h>
#include <glib/gstdio.h>

typedef enum {
    STYLE_CROSS = 0,
    STYLE_X,
    STYLE_CIRCLE,
    STYLE_DOT,
    STYLE_CROSS_DOT,
    STYLE_COUNT
} CrosshairStyle;

typedef struct {
    double r, g, b, a;
    double thickness;
    double size;
    double gap;
    gboolean show_outline;
    double outline_thickness;
    double or, og, ob, oa;
    double outline_opacity;
    CrosshairStyle style;
    double offset_x;
    double offset_y;
} CrosshairConfig;

typedef struct {
    AdwApplication *app;
    GtkWindow *overlay;
    GtkDrawingArea *drawing_area;

    AdwPreferencesWindow *prefs;
    GtkDropDown *style_dropdown;
    GtkScale *thickness_scale;
    GtkScale *size_scale;
    GtkScale *gap_scale;
    GtkScale *opacity_scale;

    GtkColorDialogButton *color_button;
    GtkSwitch *outline_switch;
    GtkScale *outline_thickness_scale;
    GtkScale *outline_opacity_scale;
    GtkColorDialogButton *outline_color_button;

    GtkDropDown *monitor_dropdown;

    GtkSpinButton *posx_spin;
    GtkSpinButton *posy_spin;

    GPtrArray *monitors;
    CrosshairConfig cfg;
    gboolean overlay_visible;
    gboolean using_layer_shell;
} AppState;

static void queue_redraw(AppState *st) {
    if (st && st->drawing_area) {
        gtk_widget_queue_draw(GTK_WIDGET(st->drawing_area));
    }
}

static char* hc_get_config_path(void) {
    const char *cfgdir = g_get_user_config_dir();
    char *dir = g_build_filename(cfgdir, "hyprcrosshair", NULL);
    g_mkdir_with_parents(dir, 0700);
    char *path = g_build_filename(dir, "hyprcrosshair.conf", NULL);
    g_free(dir);
    return path;
}

static void save_config(AppState *st) {
    if (!st) return;
    GKeyFile *kf = g_key_file_new();
    const char *grp = "Crosshair";
    g_key_file_set_double(kf, grp, "r", st->cfg.r);
    g_key_file_set_double(kf, grp, "g", st->cfg.g);
    g_key_file_set_double(kf, grp, "b", st->cfg.b);
    g_key_file_set_double(kf, grp, "a", st->cfg.a);

    g_key_file_set_double(kf, grp, "thickness", st->cfg.thickness);
    g_key_file_set_double(kf, grp, "size", st->cfg.size);
    g_key_file_set_double(kf, grp, "gap", st->cfg.gap);

    g_key_file_set_boolean(kf, grp, "show_outline", st->cfg.show_outline);
    g_key_file_set_double(kf, grp, "outline_thickness", st->cfg.outline_thickness);
    g_key_file_set_double(kf, grp, "or", st->cfg.or);
    g_key_file_set_double(kf, grp, "og", st->cfg.og);
    g_key_file_set_double(kf, grp, "ob", st->cfg.ob);
    g_key_file_set_double(kf, grp, "oa", st->cfg.oa);
    g_key_file_set_double(kf, grp, "outline_opacity", st->cfg.outline_opacity);

    g_key_file_set_integer(kf, grp, "style", (int)st->cfg.style);

    g_key_file_set_double(kf, grp, "offset_x", st->cfg.offset_x);
    g_key_file_set_double(kf, grp, "offset_y", st->cfg.offset_y);

    gsize len = 0;
    GError *err = NULL;
    char *data = g_key_file_to_data(kf, &len, &err);
    if (data && len > 0) {
        char *path = hc_get_config_path();
        g_file_set_contents(path, data, len, NULL);
        g_free(path);
    }
    g_clear_error(&err);
    g_free(data);
    g_key_file_unref(kf);
}

static void load_config(AppState *st) {
    if (!st) return;
    char *path = hc_get_config_path();
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_free(path);
        return;
    }
    GKeyFile *kf = g_key_file_new();
    GError *err = NULL;
    if (!g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, &err)) {
        g_clear_error(&err);
        g_key_file_unref(kf);
        g_free(path);
        return;
    }
    const char *grp = "Crosshair";
    if (g_key_file_has_key(kf, grp, "r", NULL)) st->cfg.r = g_key_file_get_double(kf, grp, "r", NULL);
    if (g_key_file_has_key(kf, grp, "g", NULL)) st->cfg.g = g_key_file_get_double(kf, grp, "g", NULL);
    if (g_key_file_has_key(kf, grp, "b", NULL)) st->cfg.b = g_key_file_get_double(kf, grp, "b", NULL);
    if (g_key_file_has_key(kf, grp, "a", NULL)) st->cfg.a = g_key_file_get_double(kf, grp, "a", NULL);

    if (g_key_file_has_key(kf, grp, "thickness", NULL)) st->cfg.thickness = g_key_file_get_double(kf, grp, "thickness", NULL);
    if (g_key_file_has_key(kf, grp, "size", NULL)) st->cfg.size = g_key_file_get_double(kf, grp, "size", NULL);
    if (g_key_file_has_key(kf, grp, "gap", NULL)) st->cfg.gap = g_key_file_get_double(kf, grp, "gap", NULL);

    if (g_key_file_has_key(kf, grp, "show_outline", NULL)) st->cfg.show_outline = g_key_file_get_boolean(kf, grp, "show_outline", NULL);
    if (g_key_file_has_key(kf, grp, "outline_thickness", NULL)) st->cfg.outline_thickness = g_key_file_get_double(kf, grp, "outline_thickness", NULL);
    if (g_key_file_has_key(kf, grp, "or", NULL)) st->cfg.or = g_key_file_get_double(kf, grp, "or", NULL);
    if (g_key_file_has_key(kf, grp, "og", NULL)) st->cfg.og = g_key_file_get_double(kf, grp, "og", NULL);
    if (g_key_file_has_key(kf, grp, "ob", NULL)) st->cfg.ob = g_key_file_get_double(kf, grp, "ob", NULL);
    if (g_key_file_has_key(kf, grp, "oa", NULL)) st->cfg.oa = g_key_file_get_double(kf, grp, "oa", NULL);
    if (g_key_file_has_key(kf, grp, "outline_opacity", NULL)) st->cfg.outline_opacity = g_key_file_get_double(kf, grp, "outline_opacity", NULL);

    if (g_key_file_has_key(kf, grp, "style", NULL)) {
        int s = g_key_file_get_integer(kf, grp, "style", NULL);
        if (s < 0) s = 0;
        if (s >= (int)STYLE_COUNT) s = (int)STYLE_CROSS;
        st->cfg.style = (CrosshairStyle)s;
    }

    if (g_key_file_has_key(kf, grp, "offset_x", NULL)) st->cfg.offset_x = g_key_file_get_double(kf, grp, "offset_x", NULL);
    if (g_key_file_has_key(kf, grp, "offset_y", NULL)) st->cfg.offset_y = g_key_file_get_double(kf, grp, "offset_y", NULL);

    g_key_file_unref(kf);
    g_free(path);
}

static gboolean layer_shell_supported(void) {
#ifdef GDK_WINDOWING_WAYLAND
    GdkDisplay *display = gdk_display_get_default();
    if (GDK_IS_WAYLAND_DISPLAY(display) && gtk_layer_is_supported())
        return TRUE;
#endif
    return FALSE;
}

static void set_overlay_monitor(AppState *st, int idx) {
    if (!st || !st->overlay || !st->monitors) return;
    if (idx < 0 || (guint)idx >= st->monitors->len) return;
    if (!st->using_layer_shell) return;
    GdkMonitor *mon = g_ptr_array_index(st->monitors, idx);
    if (mon) {
        gtk_layer_set_monitor(GTK_WINDOW(st->overlay), mon);
    }
}

static void set_click_through_and_transparent(GtkWidget *widget) {
    if (!gtk_widget_get_realized(widget))
        gtk_widget_realize(widget);
    GdkSurface *surf = gtk_native_get_surface(GTK_NATIVE(widget));
    if (surf) {
        cairo_region_t *empty = cairo_region_create();
        gdk_surface_set_input_region(surf, empty);
        cairo_region_destroy(empty);
        gdk_surface_set_opaque_region(surf, NULL);
    }
#if GTK_CHECK_VERSION(4,10,0)
    gtk_widget_set_can_target(widget, FALSE);
#endif
}

static void on_realize_configure_surface(GtkWidget *w, gpointer user_data) {
    (void)user_data;
    set_click_through_and_transparent(w);
}

static void stroke_with_outline(cairo_t *cr, const CrosshairConfig *c, double base_line_width) {
    double oa_eff = c->oa * c->outline_opacity;
    if (c->show_outline && c->outline_thickness > 0.0 && oa_eff > 0.0) {
        cairo_save(cr);
        cairo_set_source_rgba(cr, c->or, c->og, c->ob, oa_eff);
        cairo_set_line_width(cr, base_line_width + 2.0 * c->outline_thickness);
        cairo_stroke_preserve(cr);
        cairo_restore(cr);
    }
    cairo_set_source_rgba(cr, c->r, c->g, c->b, c->a);
    cairo_set_line_width(cr, base_line_width);
    cairo_stroke(cr);
}

static void draw_crosshair_cairo(cairo_t *cr, int width, int height, const CrosshairConfig *c, double center_dx, double center_dy) {
    double cx = width / 2.0 + c->offset_x + center_dx;
    double cy = height / 2.0 + c->offset_y + center_dy;

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    double half_t = c->thickness / 2.0;
    double align = fmod(half_t, 1.0) == 0.5 ? 0.5 : 0.0;

    double size = c->size;
    double gap = c->gap;

    switch (c->style) {
        case STYLE_CROSS:
        case STYLE_CROSS_DOT: {
            cairo_save(cr);
            cairo_new_path(cr);
            cairo_move_to(cr, cx - gap - size + align, cy + align);
            cairo_line_to(cr, cx - gap + align, cy + align);
            cairo_move_to(cr, cx + gap + align, cy + align);
            cairo_line_to(cr, cx + gap + size + align, cy + align);
            stroke_with_outline(cr, c, c->thickness);
            cairo_restore(cr);

            cairo_save(cr);
            cairo_new_path(cr);
            cairo_move_to(cr, cx + align, cy - gap - size + align);
            cairo_line_to(cr, cx + align, cy - gap + align);
            cairo_move_to(cr, cx + align, cy + gap + align);
            cairo_line_to(cr, cx + align, cy + gap + size + align);
            stroke_with_outline(cr, c, c->thickness);
            cairo_restore(cr);

            if (c->style == STYLE_CROSS_DOT) {
                double r = fmax(1.0, c->thickness * 0.75);
                cairo_new_path(cr);
                cairo_arc(cr, cx, cy, r, 0, 2 * G_PI);
                double oa_eff = c->oa * c->outline_opacity;
                if (c->show_outline && c->outline_thickness > 0.0 && oa_eff > 0.0) {
                    cairo_save(cr);
                    cairo_set_source_rgba(cr, c->or, c->og, c->ob, oa_eff);
                    cairo_set_line_width(cr, c->outline_thickness * 2.0);
                    cairo_stroke_preserve(cr);
                    cairo_restore(cr);
                }
                cairo_set_source_rgba(cr, c->r, c->g, c->b, c->a);
                cairo_fill(cr);
            }
        } break;

        case STYLE_X: {
            cairo_save(cr);
            cairo_new_path(cr);
            cairo_move_to(cr, cx - gap - size + align, cy - gap - size + align);
            cairo_line_to(cr, cx - gap + align, cy - gap + align);
            cairo_move_to(cr, cx + gap + align, cy + gap + align);
            cairo_line_to(cr, cx + gap + size + align, cy + gap + size + align);
            stroke_with_outline(cr, c, c->thickness);
            cairo_restore(cr);

            cairo_save(cr);
            cairo_new_path(cr);
            cairo_move_to(cr, cx - gap - size + align, cy + gap + size + align);
            cairo_line_to(cr, cx - gap + align, cy + gap + align);
            cairo_move_to(cr, cx + gap + align, cy - gap + align);
            cairo_line_to(cr, cx + gap + size + align, cy - gap - size + align);
            stroke_with_outline(cr, c, c->thickness);
            cairo_restore(cr);
        } break;

        case STYLE_CIRCLE: {
            cairo_new_path(cr);
            cairo_arc(cr, cx, cy, fmax(1.0, size), 0, 2 * G_PI);
            stroke_with_outline(cr, c, c->thickness);
        } break;

        case STYLE_DOT: {
            double r = fmax(1.0, size * 0.2 + c->thickness * 0.6);
            cairo_new_path(cr);
            cairo_arc(cr, cx, cy, r, 0, 2 * G_PI);
            double oa_eff = c->oa * c->outline_opacity;
            if (c->show_outline && c->outline_thickness > 0.0 && oa_eff > 0.0) {
                cairo_save(cr);
                cairo_set_source_rgba(cr, c->or, c->og, c->ob, oa_eff);
                cairo_set_line_width(cr, c->outline_thickness * 2.0);
                cairo_stroke_preserve(cr);
                cairo_restore(cr);
            }
            cairo_set_source_rgba(cr, c->r, c->g, c->b, c->a);
            cairo_fill(cr);
        } break;

        default:
            break;
    }
}

static void draw_cb(GtkDrawingArea *area, cairo_t *cr, int width, int height, AppState *st) {
    (void)area;

    cairo_save(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    cairo_restore(cr);

    double dx = 0.0, dy = 0.0;
    if (st->monitors && st->monitors->len > 0 && st->monitor_dropdown) {
        guint idx = gtk_drop_down_get_selected(st->monitor_dropdown);
        if (idx < st->monitors->len) {
            GdkMonitor *mon = g_ptr_array_index(st->monitors, idx);
            if (mon) {
                GdkRectangle geo = {0};
                gdk_monitor_get_geometry(mon, &geo);
                dx = (width  - geo.width)  / 2.0;
                dy = (height - geo.height) / 2.0;
            }
        }
    }

    draw_crosshair_cairo(cr, width, height, &st->cfg, dx, dy);
}

static void on_color_changed(GtkColorDialogButton *btn, GParamSpec *pspec, AppState *st) {
    (void)pspec;
    const GdkRGBA *rgba = gtk_color_dialog_button_get_rgba(btn);
    if (rgba) {
        st->cfg.r = rgba->red;
        st->cfg.g = rgba->green;
        st->cfg.b = rgba->blue;
        st->cfg.a = rgba->alpha;
        save_config(st);
        queue_redraw(st);
    }
}

static void on_outline_color_changed(GtkColorDialogButton *btn, GParamSpec *pspec, AppState *st) {
    (void)pspec;
    const GdkRGBA *rgba = gtk_color_dialog_button_get_rgba(btn);
    if (rgba) {
        st->cfg.or = rgba->red;
        st->cfg.og = rgba->green;
        st->cfg.ob = rgba->blue;
        st->cfg.oa = rgba->alpha;
        save_config(st);
        queue_redraw(st);
    }
}

static void on_scale_value(GtkRange *range, AppState *st) {
    double v = gtk_range_get_value(range);
    if (range == GTK_RANGE(st->thickness_scale)) st->cfg.thickness = v;
    else if (range == GTK_RANGE(st->size_scale)) st->cfg.size = v;
    else if (range == GTK_RANGE(st->gap_scale)) st->cfg.gap = v;
    else if (range == GTK_RANGE(st->opacity_scale)) st->cfg.a = v;
    else if (range == GTK_RANGE(st->outline_thickness_scale)) st->cfg.outline_thickness = v;
    else if (range == GTK_RANGE(st->outline_opacity_scale)) st->cfg.outline_opacity = v;
    save_config(st);
    queue_redraw(st);
}

static void on_outline_toggled(GtkSwitch *sw, GParamSpec *pspec, AppState *st) {
    (void)pspec;
    st->cfg.show_outline = gtk_switch_get_active(sw);
    save_config(st);
    queue_redraw(st);
}

static void on_style_changed(GObject *obj, GParamSpec *pspec, AppState *st) {
    (void)obj; (void)pspec;
    guint idx = gtk_drop_down_get_selected(st->style_dropdown);
    if (idx >= STYLE_COUNT) idx = STYLE_CROSS;
    st->cfg.style = (CrosshairStyle)idx;
    save_config(st);
    queue_redraw(st);
}

static void update_default_size_to_monitor(AppState *st) {
    if (!st || !st->overlay) return;
    if (st->using_layer_shell) return;

    GdkRectangle geo = {0};
    gboolean have_geo = FALSE;
    int idx = 0;
    if (st->monitor_dropdown) idx = (int)gtk_drop_down_get_selected(st->monitor_dropdown);
    if (st->monitors && st->monitors->len > 0) {
        GdkMonitor *mon = NULL;
        if ((guint)idx < st->monitors->len) mon = g_ptr_array_index(st->monitors, idx);
        else mon = g_ptr_array_index(st->monitors, 0);
        if (mon) {
            gdk_monitor_get_geometry(mon, &geo);
            have_geo = TRUE;
        }
    }
    if (!have_geo) {
        GdkDisplay *display = gdk_display_get_default();
        GListModel *mons = gdk_display_get_monitors(display);
        guint n = g_list_model_get_n_items(mons);
        if (n > 0) {
            GdkMonitor *m = GDK_MONITOR(g_list_model_get_item(mons, 0));
            if (m) {
                gdk_monitor_get_geometry(m, &geo);
                g_object_unref(m);
                have_geo = TRUE;
            }
        }
    }
    if (have_geo) {
        gtk_window_set_default_size(GTK_WINDOW(st->overlay), geo.width, geo.height);
        gtk_widget_set_size_request(GTK_WIDGET(st->drawing_area), geo.width, geo.height);
    }
}

static void on_monitor_changed(GObject *obj, GParamSpec *pspec, AppState *st) {
    (void)obj; (void)pspec;
    int idx = (int) gtk_drop_down_get_selected(st->monitor_dropdown);
    set_overlay_monitor(st, idx);
    if (!st->using_layer_shell)
        update_default_size_to_monitor(st);
    queue_redraw(st);
}

static void on_position_changed(GtkSpinButton *spin, AppState *st) {
    if (spin == st->posx_spin) {
        st->cfg.offset_x = gtk_spin_button_get_value(spin);
    } else if (spin == st->posy_spin) {
        st->cfg.offset_y = gtk_spin_button_get_value(spin);
    }
    save_config(st);
    queue_redraw(st);
}

static GtkWidget* labeled_row_widget(const char *title, GtkWidget *child) {
    AdwActionRow *row = ADW_ACTION_ROW(adw_action_row_new());
    adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
    gtk_widget_add_css_class(GTK_WIDGET(row), "flat");
    gtk_widget_set_hexpand(GTK_WIDGET(row), TRUE);
    gtk_widget_set_halign(GTK_WIDGET(row), GTK_ALIGN_FILL);
    gtk_widget_set_valign(GTK_WIDGET(row), GTK_ALIGN_CENTER);
    adw_action_row_add_suffix(row, child);
    gtk_widget_set_valign(child, GTK_ALIGN_CENTER);
    return GTK_WIDGET(row);
}

static void populate_monitors(AppState *st) {
    if (st->monitors) {
        g_ptr_array_free(st->monitors, TRUE);
    }
    st->monitors = g_ptr_array_new_with_free_func(g_object_unref);

    GdkDisplay *display = gdk_display_get_default();
    GListModel *mons = gdk_display_get_monitors(display);
    guint n = g_list_model_get_n_items(mons);

    GStrv names = g_new0(char*, n + 1);
    guint default_index = 0;
    for (guint i = 0; i < n; i++) {
        GdkMonitor *m = GDK_MONITOR(g_list_model_get_item(mons, i));
        g_ptr_array_add(st->monitors, m);

        const char *desc = gdk_monitor_get_description(m);
        const char *conn = gdk_monitor_get_connector(m);
        GString *label = g_string_new("");
        if (desc && *desc) g_string_append(label, desc);
        if (conn && *conn) {
            if (label->len) g_string_append(label, " - ");
            g_string_append(label, conn);
        }
        if (label->len == 0) g_string_append(label, "Monitor");
        names[i] = g_string_free(label, FALSE);
    }

    GtkStringList *list = gtk_string_list_new((const char * const*)names);
    gtk_drop_down_set_model(st->monitor_dropdown, G_LIST_MODEL(list));
    gtk_drop_down_set_selected(st->monitor_dropdown, default_index);

    for (guint i = 0; i < n; i++) g_free(names[i]);
    g_free(names);
}

static GtkWidget* build_preferences(AppState *st) {
    st->prefs = ADW_PREFERENCES_WINDOW(adw_preferences_window_new());
    gtk_window_set_application(GTK_WINDOW(st->prefs), GTK_APPLICATION(st->app));
    gtk_window_set_title(GTK_WINDOW(st->prefs), "HyprCrosshair");

    AdwPreferencesPage *page = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
    adw_preferences_window_add(st->prefs, page);

    AdwPreferencesGroup *style_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(style_group, "Style");
    adw_preferences_page_add(page, style_group);

    st->style_dropdown = GTK_DROP_DOWN(gtk_drop_down_new_from_strings(
        (const char *[]){"Cross", "X", "Circle", "Dot", "Cross + Dot", NULL}
    ));
    gtk_drop_down_set_selected(st->style_dropdown, st->cfg.style);
    g_signal_connect(st->style_dropdown, "notify::selected", G_CALLBACK(on_style_changed), st);
    gtk_widget_set_size_request(GTK_WIDGET(st->style_dropdown), 220, -1);
    gtk_widget_set_hexpand(GTK_WIDGET(st->style_dropdown), TRUE);
    adw_preferences_group_add(style_group, labeled_row_widget("Type", GTK_WIDGET(st->style_dropdown)));

    st->color_button = GTK_COLOR_DIALOG_BUTTON(gtk_color_dialog_button_new(gtk_color_dialog_new()));
    {
        GdkRGBA rgba = { st->cfg.r, st->cfg.g, st->cfg.b, st->cfg.a };
        gtk_color_dialog_button_set_rgba(st->color_button, &rgba);
    }
    g_signal_connect(st->color_button, "notify::rgba", G_CALLBACK(on_color_changed), st);
    adw_preferences_group_add(style_group, labeled_row_widget("Color", GTK_WIDGET(st->color_button)));

    st->thickness_scale = GTK_SCALE(gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1.0, 20.0, 0.5));
    gtk_range_set_value(GTK_RANGE(st->thickness_scale), st->cfg.thickness);
    gtk_widget_set_hexpand(GTK_WIDGET(st->thickness_scale), TRUE);
    gtk_scale_set_draw_value(st->thickness_scale, TRUE);
    g_signal_connect(st->thickness_scale, "value-changed", G_CALLBACK(on_scale_value), st);
    adw_preferences_group_add(style_group, labeled_row_widget("Thickness", GTK_WIDGET(st->thickness_scale)));

    st->size_scale = GTK_SCALE(gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 2.0, 400.0, 1.0));
    gtk_range_set_value(GTK_RANGE(st->size_scale), st->cfg.size);
    gtk_scale_set_draw_value(st->size_scale, TRUE);
    g_signal_connect(st->size_scale, "value-changed", G_CALLBACK(on_scale_value), st);
    adw_preferences_group_add(style_group, labeled_row_widget("Size", GTK_WIDGET(st->size_scale)));

    st->gap_scale = GTK_SCALE(gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 150.0, 1.0));
    gtk_range_set_value(GTK_RANGE(st->gap_scale), st->cfg.gap);
    gtk_scale_set_draw_value(st->gap_scale, TRUE);
    g_signal_connect(st->gap_scale, "value-changed", G_CALLBACK(on_scale_value), st);
    adw_preferences_group_add(style_group, labeled_row_widget("Gap", GTK_WIDGET(st->gap_scale)));

    st->opacity_scale = GTK_SCALE(gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.05, 1.0, 0.01));
    gtk_range_set_value(GTK_RANGE(st->opacity_scale), st->cfg.a);
    gtk_scale_set_draw_value(st->opacity_scale, TRUE);
    g_signal_connect(st->opacity_scale, "value-changed", G_CALLBACK(on_scale_value), st);
    adw_preferences_group_add(style_group, labeled_row_widget("Opacity", GTK_WIDGET(st->opacity_scale)));

    AdwPreferencesGroup *outline_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(outline_group, "Outline");
    adw_preferences_page_add(page, outline_group);

    st->outline_switch = GTK_SWITCH(gtk_switch_new());
    gtk_switch_set_active(st->outline_switch, st->cfg.show_outline);
    g_signal_connect(st->outline_switch, "notify::active", G_CALLBACK(on_outline_toggled), st);
    adw_preferences_group_add(outline_group, labeled_row_widget("Enable Outline", GTK_WIDGET(st->outline_switch)));

    st->outline_color_button = GTK_COLOR_DIALOG_BUTTON(gtk_color_dialog_button_new(gtk_color_dialog_new()));
    {
        GdkRGBA rgba = { st->cfg.or, st->cfg.og, st->cfg.ob, st->cfg.oa };
        gtk_color_dialog_button_set_rgba(st->outline_color_button, &rgba);
    }
    g_signal_connect(st->outline_color_button, "notify::rgba", G_CALLBACK(on_outline_color_changed), st);
    adw_preferences_group_add(outline_group, labeled_row_widget("Outline Color", GTK_WIDGET(st->outline_color_button)));

    st->outline_thickness_scale = GTK_SCALE(gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.5, 10.0, 0.5));
    gtk_range_set_value(GTK_RANGE(st->outline_thickness_scale), st->cfg.outline_thickness);
    gtk_scale_set_draw_value(st->outline_thickness_scale, TRUE);
    g_signal_connect(st->outline_thickness_scale, "value-changed", G_CALLBACK(on_scale_value), st);
    adw_preferences_group_add(outline_group, labeled_row_widget("Outline Thickness", GTK_WIDGET(st->outline_thickness_scale)));

    st->outline_opacity_scale = GTK_SCALE(gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01));
    gtk_range_set_value(GTK_RANGE(st->outline_opacity_scale), st->cfg.outline_opacity);
    gtk_scale_set_draw_value(st->outline_opacity_scale, TRUE);
    g_signal_connect(st->outline_opacity_scale, "value-changed", G_CALLBACK(on_scale_value), st);
    adw_preferences_group_add(outline_group, labeled_row_widget("Outline Opacity", GTK_WIDGET(st->outline_opacity_scale)));

    AdwPreferencesGroup *position_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(position_group, "Position (0,0 is screen center)");
    adw_preferences_page_add(page, position_group);

    GtkAdjustment *adj_x = gtk_adjustment_new(0.0, -4000.0, 4000.0, 1.0, 10.0, 0.0);
    st->posx_spin = GTK_SPIN_BUTTON(gtk_spin_button_new(adj_x, 1.0, 0));
    gtk_spin_button_set_value(st->posx_spin, st->cfg.offset_x);
    g_signal_connect(st->posx_spin, "value-changed", G_CALLBACK(on_position_changed), st);
    adw_preferences_group_add(position_group, labeled_row_widget("X Offset (px)", GTK_WIDGET(st->posx_spin)));

    GtkAdjustment *adj_y = gtk_adjustment_new(0.0, -4000.0, 4000.0, 1.0, 10.0, 0.0);
    st->posy_spin = GTK_SPIN_BUTTON(gtk_spin_button_new(adj_y, 1.0, 0));
    gtk_spin_button_set_value(st->posy_spin, st->cfg.offset_y);
    g_signal_connect(st->posy_spin, "value-changed", G_CALLBACK(on_position_changed), st);
    adw_preferences_group_add(position_group, labeled_row_widget("Y Offset (px)", GTK_WIDGET(st->posy_spin)));

    AdwPreferencesGroup *display_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
    adw_preferences_group_set_title(display_group, "Display");
    adw_preferences_page_add(page, display_group);

    st->monitor_dropdown = GTK_DROP_DOWN(gtk_drop_down_new(NULL, NULL));
    populate_monitors(st);
    g_signal_connect(st->monitor_dropdown, "notify::selected", G_CALLBACK(on_monitor_changed), st);
    adw_preferences_group_add(display_group, labeled_row_widget("Monitor", GTK_WIDGET(st->monitor_dropdown)));

    return GTK_WIDGET(st->prefs);
}

static GtkWidget* build_overlay(AppState *st) {
    GtkWindow *w = GTK_WINDOW(gtk_window_new());
    gtk_window_set_application(w, GTK_APPLICATION(st->app));
    gtk_window_set_decorated(w, FALSE);
    gtk_window_set_resizable(w, TRUE);
    gtk_window_set_title(w, "HyprCrosshair Overlay");

    st->using_layer_shell = layer_shell_supported();

    if (st->using_layer_shell) {
        gtk_layer_init_for_window(w);
        gtk_layer_set_layer(w, GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_namespace(w, "hyprcrosshair");
        gtk_layer_set_keyboard_mode(w, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
        gtk_layer_set_exclusive_zone(w, 0);
        gtk_layer_set_anchor(w, GTK_LAYER_SHELL_EDGE_TOP, TRUE);
        gtk_layer_set_anchor(w, GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
        gtk_layer_set_anchor(w, GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        gtk_layer_set_anchor(w, GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    } else {
        gtk_window_fullscreen(w);
    }

    st->drawing_area = GTK_DRAWING_AREA(gtk_drawing_area_new());
    gtk_widget_set_hexpand(GTK_WIDGET(st->drawing_area), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(st->drawing_area), TRUE);

    gtk_widget_add_css_class(GTK_WIDGET(w), "hypr-overlay");
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css,
        ".hypr-overlay, .hypr-overlay * { background: transparent; background-color: transparent; box-shadow: none; }");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(css);

    gtk_drawing_area_set_draw_func(st->drawing_area, (GtkDrawingAreaDrawFunc)draw_cb, st, NULL);
    gtk_window_set_child(w, GTK_WIDGET(st->drawing_area));

    g_signal_connect(w, "realize", G_CALLBACK(on_realize_configure_surface), st);

    return GTK_WIDGET(w);
}

static void apply_default_config(AppState *st) {
    st->cfg.r = 0.15; st->cfg.g = 0.85; st->cfg.b = 0.35; st->cfg.a = 0.95;
    st->cfg.thickness = 2.0;
    st->cfg.size = 40.0;
    st->cfg.gap = 8.0;
    st->cfg.style = STYLE_CROSS_DOT;

    st->cfg.show_outline = TRUE;
    st->cfg.outline_thickness = 1.5;
    st->cfg.or = 0.0; st->cfg.og = 0.0; st->cfg.ob = 0.0; st->cfg.oa = 0.9;
    st->cfg.outline_opacity = 1.0;

    st->cfg.offset_x = 0.0;
    st->cfg.offset_y = 0.0;

    st->overlay_visible = TRUE;
}

static void on_quit(GSimpleAction *action, GVariant *param, gpointer user_data) {
    (void)action; (void)param;
    AppState *st = user_data;
    save_config(st);
    g_application_quit(G_APPLICATION(st->app));
}

static void on_toggle_overlay(GSimpleAction *action, GVariant *param, gpointer user_data) {
    (void)action; (void)param;
    AppState *st = user_data;
    st->overlay_visible = !st->overlay_visible;
    gtk_widget_set_visible(GTK_WIDGET(st->overlay), st->overlay_visible);
    set_click_through_and_transparent(GTK_WIDGET(st->overlay));
}

static void app_activate(GApplication *app, gpointer user_data) {
    (void)user_data;
    AppState *st = g_new0(AppState, 1);
    st->app = ADW_APPLICATION(app);
    apply_default_config(st);
    load_config(st);

    GtkWidget *ov = build_overlay(st);
    st->overlay = GTK_WINDOW(ov);

    GtkWidget *prefs = build_preferences(st);
    (void)prefs;

    set_overlay_monitor(st, gtk_drop_down_get_selected(st->monitor_dropdown));
    if (!st->using_layer_shell)
        update_default_size_to_monitor(st);

    gtk_widget_set_visible(ov, TRUE);
    set_click_through_and_transparent(ov);

    gtk_window_present(GTK_WINDOW(st->prefs));

    const GActionEntry entries[] = {
        { .name = "quit", .activate = on_quit },
        { .name = "toggle-overlay", .activate = on_toggle_overlay },
    };
    g_action_map_add_action_entries(G_ACTION_MAP(app), entries, G_N_ELEMENTS(entries), st);
}

int main(int argc, char **argv) {
    g_set_prgname("hyprcrosshair");
    adw_init();

    AdwApplication *app = adw_application_new("dev.hyprcrosshair.app", G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app, "activate", G_CALLBACK(app_activate), NULL);

    const char *accels_quit[] = { "<Ctrl>Q", NULL };
    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.quit", accels_quit);

    const char *accels_toggle[] = { "<Ctrl>T", NULL };
    gtk_application_set_accels_for_action(GTK_APPLICATION(app), "app.toggle-overlay", accels_toggle);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}