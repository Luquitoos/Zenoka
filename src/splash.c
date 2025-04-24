#include <gtk/gtk.h>
#include <gdk/win32/gdkwin32.h>
#include <windows.h>
#include "splash.h"

typedef struct {
    GtkWidget *window;
    guint timeout_id;
    GdkPixbufAnimationIter *anim_iter;
    GtkWidget *picture;
    guint anim_id;
    GtkApplication *app;
    void (*callback)(GtkApplication*);
} SplashScreen;

static void close_splash_screen(SplashScreen *splash) {
    if (splash->anim_id > 0)  g_source_remove(splash->anim_id);
    if (splash->timeout_id > 0) g_source_remove(splash->timeout_id);
    if (splash->anim_iter)      g_object_unref(splash->anim_iter);
    gtk_window_destroy(GTK_WINDOW(splash->window));
    if (splash->callback) splash->callback(splash->app);
    g_free(splash);
}

static gboolean auto_close(gpointer data) {
    close_splash_screen((SplashScreen*)data);
    return G_SOURCE_REMOVE;
}

static gboolean animate_gif(gpointer data) {
    SplashScreen *s = data;
    if (s->anim_iter) {
        GdkPixbuf *frame = gdk_pixbuf_animation_iter_get_pixbuf(s->anim_iter);
        GdkTexture *tex  = gdk_texture_new_for_pixbuf(frame);
        gtk_picture_set_paintable(GTK_PICTURE(s->picture), GDK_PAINTABLE(tex));
        g_object_unref(tex);
        int delay = gdk_pixbuf_animation_iter_get_delay_time(s->anim_iter);
        gdk_pixbuf_animation_iter_advance(s->anim_iter, NULL);
        s->anim_id = g_timeout_add(delay, animate_gif, s);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static void setup_glassmorphism_splash() {
    GtkCssProvider *p = gtk_css_provider_new();
    gtk_css_provider_load_from_string(p,
        "#glass-background { background-color: rgba(0, 0, 0, 0.93); }"
        "window { background-color: transparent; border-radius:15px; }"
        "box    { background-color: transparent; }"
    );
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(p),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(p);
}

static void on_window_map(GtkWidget *widget, G_GNUC_UNUSED gpointer data) {
    const int win_w = 400, win_h = 300;
    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(widget));
    HWND hwnd = gdk_win32_surface_get_handle(surface);

    MONITORINFO mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfo(hmon, &mi);

    int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - win_w) / 2;
    int y = mi.rcWork.top  + (mi.rcWork.bottom - mi.rcWork.top - win_h) / 2;

    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void create_splash_screen(GtkApplication *app, void (*callback)(GtkApplication*)) {
    SplashScreen *splash = g_new0(SplashScreen, 1);
    const int win_w = 400, win_h = 300;

    splash->app = app;
    splash->callback = callback;

    splash->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(splash->window), "Splash Screen");
    gtk_window_set_default_size(GTK_WINDOW(splash->window), win_w, win_h);
    gtk_window_set_resizable(GTK_WINDOW(splash->window), FALSE);
    gtk_window_set_decorated(GTK_WINDOW(splash->window), FALSE);

    g_signal_connect(splash->window, "map", G_CALLBACK(on_window_map), NULL);
    setup_glassmorphism_splash(splash->window);

    // Fundo translúcido
    GtkWidget *background = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(background, TRUE);
    gtk_widget_set_vexpand(background, TRUE);
    gtk_widget_set_name(background, "glass-background");

    // Conteúdo central com o GIF
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_margin_top(box, 10);
    gtk_widget_set_margin_bottom(box, 10);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

    GError *error = NULL;
    GdkPixbufAnimation *anim = gdk_pixbuf_animation_new_from_file("icons/Logo_splash.gif", &error);
    if (error) {
        g_warning("Erro ao carregar GIF: %s", error->message);
        g_clear_error(&error);
        GtkWidget *img = gtk_image_new_from_icon_name("image-missing");
        gtk_image_set_pixel_size(GTK_IMAGE(img), 128);
        gtk_box_append(GTK_BOX(box), img);
    } else {
        splash->picture = gtk_picture_new();
        gtk_widget_set_size_request(splash->picture, 128, 128);

        GdkPixbuf *first = gdk_pixbuf_animation_get_static_image(anim);
        GdkTexture *tex  = gdk_texture_new_for_pixbuf(first);
        gtk_picture_set_paintable(GTK_PICTURE(splash->picture), GDK_PAINTABLE(tex));
        g_object_unref(tex);

        splash->anim_iter = gdk_pixbuf_animation_get_iter(anim, NULL);
        gtk_box_append(GTK_BOX(box), splash->picture);
        splash->anim_id = g_timeout_add(100, animate_gif, splash);

        g_object_unref(anim);
    }

    // Usando GtkOverlay para fundo transparente + conteúdo sólido por cima
    GtkWidget *overlay = gtk_overlay_new();
    gtk_widget_set_hexpand(overlay, TRUE);
    gtk_widget_set_vexpand(overlay, TRUE);

    gtk_overlay_set_child(GTK_OVERLAY(overlay), background);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), box);

    gtk_window_set_child(GTK_WINDOW(splash->window), overlay);
    gtk_window_present(GTK_WINDOW(splash->window));

    splash->timeout_id = g_timeout_add(2400, auto_close, splash);
}
