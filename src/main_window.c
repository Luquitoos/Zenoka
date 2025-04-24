#include <gtk/gtk.h>
#include <gdk/win32/gdkwin32.h>
#include <windows.h>
#include <string.h>
#include "main_window.h"
#include <regex.h>

typedef struct {
    GtkWidget *window;
    GtkWidget *main_box;
    GtkWidget *flip_button;
    GtkWidget *flip_button_container;
    GtkWidget *title_bar;
    GtkWidget *minimize_button;
    GtkWidget *maximize_button;
    GtkWidget *close_button;
    GtkWidget *unflip_button;
    gboolean titlebar_flipped;

    // Novos campos
    GtkWidget *title_section;
    GtkWidget *link_bar;
    GtkWidget *link_entry;
    GtkWidget *player_container;
    GtkWidget *player_frame;
    GtkWidget *time_start_entry;
    GtkWidget *time_end_entry;
    GtkWidget *export_button;
    gboolean player_view_active;

    GtkWidget *flip_gif;
    GtkWidget *minimize_gif;
    GtkWidget *maximize_gif;
    GtkWidget *close_gif;
    GtkWidget *unflip_gif;
} MainWindow;

typedef struct {
    GdkPixbufAnimationIter *iter;
    GtkWidget *image;
    GdkPaintable *paintable;
} AnimationData;

static gboolean window_dragging = FALSE;
static int drag_start_x = 0;
static int drag_start_y = 0;
static POINT initial_cursor_pos = {0, 0};


static gboolean animation_timeout(AnimationData *data) {
    if (!data || !data->iter || !data->image)
        return G_SOURCE_REMOVE;

    if (gdk_pixbuf_animation_iter_advance(data->iter, NULL)) {
        GdkPixbuf *frame = gdk_pixbuf_animation_iter_get_pixbuf(data->iter);
        if (data->paintable)
            g_object_unref(data->paintable);
        data->paintable = GDK_PAINTABLE(gdk_texture_new_for_pixbuf(frame));
        gtk_image_set_from_paintable(GTK_IMAGE(data->image), data->paintable);
    }

    return G_SOURCE_CONTINUE;
}

static void free_animation_data(gpointer user_data) {
    AnimationData *data = (AnimationData *)user_data;
    if (data) {
        if (data->iter)
            g_object_unref(data->iter);
        if (data->paintable)
            g_object_unref(data->paintable);
        g_free(data);
    }
}

static gboolean on_unflip_done(gpointer data) {
    MainWindow *m = data;
    gtk_widget_set_visible(m->title_bar, FALSE);
    gtk_widget_set_visible(m->flip_button_container, TRUE);
    return G_SOURCE_REMOVE;
}

static void flip_titlebar(MainWindow *m, gboolean show) {
    if (show && !m->titlebar_flipped) {
        gtk_widget_set_visible(m->flip_button_container, FALSE);
        gtk_widget_remove_css_class(m->title_bar, "flip-out");
        gtk_widget_add_css_class(m->title_bar, "flip-in");
        gtk_widget_set_visible(m->title_bar, TRUE);
        m->titlebar_flipped = TRUE;
    } else if (!show && m->titlebar_flipped) {
        gtk_widget_remove_css_class(m->title_bar, "flip-in");
        gtk_widget_add_css_class(m->title_bar, "flip-out");
        g_timeout_add(300, on_unflip_done, m);
        m->titlebar_flipped = FALSE;
    }
}

static void on_flip_button_clicked(GtkButton *button, MainWindow *m) {
    (void)button;
    flip_titlebar(m, TRUE);
}

static void on_unflip_button_clicked(GtkButton *button, MainWindow *m) {
    (void)button;
    window_dragging = FALSE;
    flip_titlebar(m, FALSE);
}

static void setup_glassmorphism() {
    GtkCssProvider *p = gtk_css_provider_new();
    gtk_css_provider_load_from_string(p,
        "#glass-background { background-color: rgba(0, 0, 0, 0.93); }"
        "window { background-color: transparent; border-radius:15px; }"
        "box    { background-color: transparent; border-radius:15px;}"
        ".titlebar-container { background-color: rgba(60, 56, 63, 1); border-radius:12px; }"
        ".flip-button        { background-color: rgba(24, 24, 24, 1.0); border-radius:50%; min-width:24px; min-height:24px;}"
        ".control-button     { background-color: transparent; min-width:30px; min-height:30px; border-radius:15px; margin:0 5px; padding:0; }"
        ".minimize-button    { color:#ffcc00; }"
        ".maximize-button    { color:#00cc00; }"
        ".close-button       { color:#ff3333; }"
        ".unflip-button      { color:#3399ff; }"
        ".button-icon        { background-color: transparent; }"
        "@keyframes flip-in  { from { opacity:0; transform: translateY(-30px) rotateX(-90deg); } to { opacity:1; transform: translateY(0) rotateX(0); } }"
        "@keyframes flip-out { from { opacity:1; transform: translateY(0) rotateX(0); } to { opacity:0; transform: translateY(-30px) rotateX(-90deg); } }"
        ".flip-in  { animation: flip-in 300ms ease-out; transform-origin: top; }"
        ".flip-out { animation: flip-out 300ms ease-in;  transform-origin: top; }"
        ".main-title { color: white; font-size: 50px; font-weight: bold; }"
        ".subtitle { color: #8c52ff; font-size: 24px; font-style: italic; }"
        ".link-button { background-color: rgba(255,255,255,0.08); border-radius: 20px; min-width: 40px; min-height: 40px; padding: 4px; }"
        ".link-entry { background-color:rgb(140, 82, 255); color: black; border-radius: 12px; padding: 6px 10px; border: none; }"
        "entry.link-entry placeholder { color: white; opacity: 0.7; }"
        "#player-frame { background-color: #8c52ff; border-radius: 10px; }"
    );
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(p),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(p);
}
static void center_window_on_monitor(GtkWidget *widget) {
    const int win_w = 1200, win_h = 800;
    GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(widget));
    HWND hwnd = gdk_win32_surface_get_handle(surface);
    MONITORINFO mi;
    memset(&mi, 0, sizeof(mi)); mi.cbSize = sizeof(mi);
    HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    GetMonitorInfo(hmon, &mi);
    int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - win_w) / 2;
    int y = mi.rcWork.top  + (mi.rcWork.bottom - mi.rcWork.top - win_h) / 2;
    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

static void on_window_map(GtkWidget *widget, gpointer data) {
    (void)data;
    center_window_on_monitor(widget);
}

static void on_window_closed(GtkWindow *window, gpointer user_data) {
    (void)window;
    MainWindow *m = user_data;
    g_free(m);
}

static gboolean on_drag_handle_button_press(GtkGestureClick *gesture, int n_press,
    double x, double y, GtkWidget *window) {
    (void)gesture; (void)x; (void)y;
    if (n_press == 1) {
        window_dragging = TRUE;
        GetCursorPos(&initial_cursor_pos);
        GdkSurface *s = gtk_native_get_surface(GTK_NATIVE(window));
        HWND hwnd = gdk_win32_surface_get_handle(s);
        RECT r; GetWindowRect(hwnd, &r);
        drag_start_x = initial_cursor_pos.x - r.left;
        drag_start_y = initial_cursor_pos.y - r.top;
        return TRUE;
    }
return FALSE;
}

static gboolean on_drag_handle_button_release(GtkGestureClick *gesture, int n_press,
    double x, double y, GtkWidget *window) {
    (void)gesture; (void)n_press; (void)x; (void)y; (void)window;
    window_dragging = FALSE;
    return TRUE;
}

static gboolean on_window_motion_notify(GtkEventControllerMotion *controller,
    double x, double y, GtkWidget *window) {
    (void)controller; (void)x; (void)y;
    if (window_dragging) {
        POINT p; GetCursorPos(&p);
        GdkSurface *s = gtk_native_get_surface(GTK_NATIVE(window));
        HWND hwnd = gdk_win32_surface_get_handle(s);
        SetWindowPos(hwnd, NULL, p.x - drag_start_x, p.y - drag_start_y,
        0, 0, SWP_NOSIZE | SWP_NOZORDER);
        return TRUE;
    }
return FALSE;
}

static void on_minimize_clicked(GtkButton *button, GtkWindow *window) {
    (void)button;
    window_dragging = FALSE;
    gtk_window_minimize(window);
}

static void on_maximize_clicked(GtkButton *button, GtkWindow *window) {
    (void)button;
    window_dragging = FALSE;
    if (gtk_window_is_maximized(window)) gtk_window_unmaximize(window);
    else gtk_window_maximize(window);
}

static void on_close_clicked(GtkButton *button, GtkWindow *window) {
    (void)button;
    gtk_window_close(window);
}

static GtkWidget *create_styled_button(const char *icon_name, const char *css_class) G_GNUC_UNUSED;
static GtkWidget *create_styled_button(const char *icon_name, const char *css_class) {
    GtkWidget *button = gtk_button_new();
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    gtk_widget_add_css_class(button, "control-button");
    if (css_class)
        gtk_widget_add_css_class(button, css_class);
    gtk_button_set_child(GTK_BUTTON(button), icon);
    return button;
}

static GtkWidget *create_button_with_image(const char *icon_name, const char *css_class,
    const char *image_path, int width, int height) {
    GtkWidget *button = gtk_button_new();
    gtk_widget_add_css_class(button, "control-button");
    if (css_class)
        gtk_widget_add_css_class(button, css_class);

    GtkWidget *overlay = gtk_overlay_new();
    GtkWidget *invisible_icon = gtk_image_new_from_icon_name(icon_name);
    gtk_widget_set_opacity(invisible_icon, 0.0);
    gtk_widget_set_size_request(invisible_icon, width, height);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), invisible_icon);

    if (image_path && g_file_test(image_path, G_FILE_TEST_EXISTS)) {
        const char *ext = strrchr(image_path, '.');

    if (ext && g_ascii_strcasecmp(ext, ".gif") == 0) {
        GdkPixbufAnimation *animation = gdk_pixbuf_animation_new_from_file(image_path, NULL);
        if (animation) {
            GdkPixbufAnimationIter *iter = gdk_pixbuf_animation_get_iter(animation, NULL);
            GdkPixbuf *frame = gdk_pixbuf_animation_iter_get_pixbuf(iter);
            GdkPaintable *paintable = GDK_PAINTABLE(gdk_texture_new_for_pixbuf(frame));
            GtkWidget *gif_image = gtk_image_new_from_paintable(paintable);
            gtk_widget_set_size_request(gif_image, width, height);
            gtk_overlay_add_overlay(GTK_OVERLAY(overlay), gif_image);
            gtk_widget_set_halign(gif_image, GTK_ALIGN_CENTER);
            gtk_widget_set_valign(gif_image, GTK_ALIGN_CENTER);

            AnimationData *data = g_new0(AnimationData, 1);
            data->iter = iter;
            data->image = gif_image;
            data->paintable = paintable;

            g_timeout_add_full(G_PRIORITY_DEFAULT,
            gdk_pixbuf_animation_iter_get_delay_time(iter),
            (GSourceFunc)animation_timeout,
            data,
            free_animation_data);
            g_object_unref(animation);
        }
    } else {
        GtkWidget *static_image = gtk_image_new_from_file(image_path);
        gtk_widget_set_size_request(static_image, width, height);
        gtk_overlay_add_overlay(GTK_OVERLAY(overlay), static_image);
        gtk_widget_set_halign(static_image, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(static_image, GTK_ALIGN_CENTER);
    }
    } else {
        GtkWidget *fallback_icon = gtk_image_new_from_icon_name(icon_name);
        gtk_widget_set_size_request(fallback_icon, width, height);
        gtk_overlay_add_overlay(GTK_OVERLAY(overlay), fallback_icon);
        gtk_widget_set_halign(fallback_icon, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(fallback_icon, GTK_ALIGN_CENTER);
        if (image_path)
            g_print("Imagem não encontrada: %s\n", image_path);
    }

    gtk_button_set_child(GTK_BUTTON(button), overlay);
    return button;
}

static void setup_titlebar(MainWindow *m) {
    m->flip_button_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(m->flip_button_container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(m->flip_button_container, GTK_ALIGN_START);
    gtk_widget_set_margin_top(m->flip_button_container, 5);

    m->flip_button = create_button_with_image("view-more-symbolic", "flip-button", "G:/video_clipper/icons/flip_icon.gif", 10, 10);
    gtk_box_append(GTK_BOX(m->flip_button_container), m->flip_button);

    m->title_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(m->title_bar, "titlebar-container");
    gtk_widget_set_visible(m->title_bar, FALSE);
    gtk_widget_set_size_request(m->title_bar, 250, 36);
    gtk_widget_set_halign(m->title_bar, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(m->title_bar, GTK_ALIGN_START);
    gtk_widget_set_margin_top(m->title_bar, 5);
    gtk_widget_set_margin_bottom(m->title_bar, 10);

    m->minimize_button = create_button_with_image("window-minimize-symbolic", "minimize-button", "G:/video_clipper/icons/minimize_icon.gif", 20, 20);
    m->maximize_button = create_button_with_image("window-maximize-symbolic", "maximize-button", "G:/video_clipper/icons/maximize_icon.gif", 20, 20);
    m->close_button = create_button_with_image("window-close-symbolic", "close-button", "G:/video_clipper/icons/close_icon.gif", 25, 25);
    m->unflip_button = create_button_with_image("go-up-symbolic", "unflip-button", "G:/video_clipper/icons/unflip_icon.gif", 10, 10);

    // Criamos três seções (esquerda, centro, direita)
    GtkWidget *left_section = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *center_section = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *right_section = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    // As seções laterais devem expandir igualmente
    gtk_widget_set_hexpand(left_section, TRUE);
    gtk_widget_set_hexpand(right_section, TRUE);

    // Importante: centralizamos os controles na seção central
    gtk_widget_set_halign(center_section, GTK_ALIGN_CENTER);
    
    // Incluímos todos os botões na seção central
    gtk_box_append(GTK_BOX(center_section), m->minimize_button);
    gtk_box_append(GTK_BOX(center_section), m->maximize_button);
    gtk_box_append(GTK_BOX(center_section), m->close_button);
    gtk_box_append(GTK_BOX(center_section), m->unflip_button);

    // Ao invés de adicionar um placeholder apenas à seção direita,
    // criamos placeholders idênticos para ambas as seções laterais
    GtkWidget *left_placeholder = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(left_placeholder, 16, 16);
    gtk_box_append(GTK_BOX(left_section), left_placeholder);
    
    GtkWidget *right_placeholder = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(right_placeholder, 16, 16);
    gtk_box_append(GTK_BOX(right_section), right_placeholder);

    // Adicionamos as seções à titlebar
    gtk_box_append(GTK_BOX(m->title_bar), left_section);
    gtk_box_append(GTK_BOX(m->title_bar), center_section);
    gtk_box_append(GTK_BOX(m->title_bar), right_section);

    // Configuramos o controle de movimento para a janela
    GtkEventController *motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(on_window_motion_notify), m->window);
    gtk_widget_add_controller(m->window, motion);

    // Adicionamos o gesto de clique para a titlebar inteira
    GtkGesture *drag = gtk_gesture_click_new();
    g_signal_connect(drag, "pressed", G_CALLBACK(on_drag_handle_button_press), m->window);
    g_signal_connect(drag, "released", G_CALLBACK(on_drag_handle_button_release), m->window);
    gtk_widget_add_controller(m->title_bar, GTK_EVENT_CONTROLLER(drag));

    // Conectamos os sinais dos botões
    g_signal_connect(m->flip_button, "clicked", G_CALLBACK(on_flip_button_clicked), m);
    g_signal_connect(m->unflip_button, "clicked", G_CALLBACK(on_unflip_button_clicked), m);
    g_signal_connect(m->minimize_button, "clicked", G_CALLBACK(on_minimize_clicked), m->window);
    g_signal_connect(m->maximize_button, "clicked", G_CALLBACK(on_maximize_clicked), m->window);
    g_signal_connect(m->close_button, "clicked", G_CALLBACK(on_close_clicked), m->window);

    m->titlebar_flipped = FALSE;
}

static void open_link(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    (void)gesture;
    (void)n_press;
    (void)x;
    (void)y;

    const char *url = user_data;
    GtkUriLauncher *launcher = gtk_uri_launcher_new(url);
    gtk_uri_launcher_launch(launcher, NULL, NULL, NULL, NULL);  // Últimos 2: GCancellable, GAsyncReadyCallback
    g_object_unref(launcher);
}

static gboolean is_valid_link(const char *url) {
    return (g_str_has_prefix(url, "http://") || g_str_has_prefix(url, "https://")) &&
           (strstr(url, "youtube.com") || strstr(url, "twitch.tv") || strstr(url, "tiktok.com") ||
            strstr(url, "instagram.com") || strstr(url, "spotify.com") || strstr(url, "twitter.com") ||
            strstr(url, "x.com"));
}

static void on_link_submitted(GtkWidget *widget, gpointer user_data) {
    MainWindow *m = (MainWindow *)user_data;
    const char *url = gtk_editable_get_text(GTK_EDITABLE(m->link_entry));

    if (!is_valid_link(url)) {
        g_print("Link inválido: %s\n", url);
        return;
    }

    // Oculta título e subtítulo
    gtk_widget_set_visible(m->title_section, FALSE);

    // Move barra de link para cima
    gtk_widget_set_margin_top(m->link_bar, 10);

    // Cria frame para player
    m->player_frame = gtk_frame_new(NULL);
    gtk_widget_set_size_request(m->player_frame, 580, 330);
    gtk_widget_set_halign(m->player_frame, GTK_ALIGN_START);
    gtk_widget_set_valign(m->player_frame, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(m->player_frame, 20);
    gtk_widget_set_margin_top(m->player_frame, 20);
    gtk_widget_set_name(m->player_frame, "player-frame");

    // Entradas de tempo
    m->time_start_entry = gtk_entry_new();
    m->time_end_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(m->time_start_entry), "00:00:00:00");
    gtk_entry_set_placeholder_text(GTK_ENTRY(m->time_end_entry), "00:00:00:00");
    gtk_widget_set_size_request(m->time_start_entry, 100, 30);
    gtk_widget_set_size_request(m->time_end_entry, 100, 30);

    GtkWidget *time_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(time_box), m->time_start_entry);
    gtk_box_append(GTK_BOX(time_box), m->time_end_entry);
    gtk_widget_set_halign(time_box, GTK_ALIGN_START);

    // Botão de exportação (com gif)
    m->export_button = create_button_with_image("media-playback-start-symbolic", NULL,
                                                "G:/video_clipper/icons/export_icon.gif", 40, 40);
    gtk_widget_set_halign(m->export_button, GTK_ALIGN_END);

    GtkWidget *bottom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_halign(bottom_box, GTK_ALIGN_FILL);
    gtk_widget_set_margin_top(bottom_box, 10);
    gtk_box_append(GTK_BOX(bottom_box), time_box);
    gtk_box_append(GTK_BOX(bottom_box), m->export_button);

    m->player_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_append(GTK_BOX(m->player_container), m->player_frame);
    gtk_box_append(GTK_BOX(m->player_container), bottom_box);

    gtk_box_append(GTK_BOX(m->main_box), m->player_container);
    gtk_widget_show_all(m->player_container);

    m->player_view_active = TRUE;
}

void create_main_window(GtkApplication *app) {
    MainWindow *m = g_new0(MainWindow, 1);
    const int win_w = 1200, win_h = 800;

    m->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(m->window), "Zenoka");
    gtk_window_set_default_size(GTK_WINDOW(m->window), win_w, win_h);
    gtk_window_set_resizable(GTK_WINDOW(m->window), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(m->window), FALSE);

    setup_glassmorphism();
    setup_titlebar(m);

    g_signal_connect(m->window, "map", G_CALLBACK(on_window_map), NULL);
    g_signal_connect(m->window, "destroy", G_CALLBACK(on_window_closed), m);

    m->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(m->main_box, TRUE);

    GtkWidget *top_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *overlay = gtk_overlay_new();
    GtkWidget *base = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(base, 10, 40);
    gtk_overlay_set_child(GTK_OVERLAY(overlay), base);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), m->flip_button_container);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), m->title_bar);
    gtk_box_append(GTK_BOX(top_container), overlay);

    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(content_box, 20);
    gtk_widget_set_margin_end(content_box, 20);
    gtk_widget_set_margin_top(content_box, 0);
    gtk_widget_set_margin_bottom(content_box, 0);
    gtk_widget_set_vexpand(content_box, TRUE);

    GtkWidget *logo_container = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *logo = gtk_image_new_from_file("G:/video_clipper/icons/logo.png");
    gtk_widget_set_size_request(logo, 100, 80);
    gtk_box_append(GTK_BOX(logo_container), logo);
    GtkWidget *logo_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(logo_spacer, TRUE);
    gtk_box_append(GTK_BOX(logo_container), logo_spacer);
    gtk_box_append(GTK_BOX(content_box), logo_container);

    GtkWidget *middle_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_valign(middle_box, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(middle_box, TRUE);
    gtk_widget_set_margin_top(middle_box, -100);

    GtkWidget *title_section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(title_section, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(title_section, 10);
    GtkWidget *title_label = gtk_label_new("Video/Audio Clipper");
    gtk_widget_add_css_class(title_label, "main-title");
    GtkWidget *subtitle_label = gtk_label_new("Zenoka");
    gtk_widget_add_css_class(subtitle_label, "subtitle");
    gtk_box_append(GTK_BOX(title_section), title_label);
    gtk_box_append(GTK_BOX(title_section), subtitle_label);
    gtk_box_append(GTK_BOX(middle_box), title_section);

    GtkWidget *link_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(link_bar, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(link_bar, 15);
    gtk_widget_set_margin_bottom(link_bar, 20);

    GtkWidget *link_entry = gtk_entry_new();
    g_signal_connect(link_entry, "activate", G_CALLBACK(on_link_submitted), m);
    gtk_widget_set_size_request(link_entry, 400, 40);
    gtk_widget_add_css_class(link_entry, "link-entry");
    gtk_entry_set_placeholder_text(GTK_ENTRY(link_entry), "Cole um link do YouTube, Twitch, Instagram, TikTok, Spotify etc...");

    GtkWidget *link_button1 = create_button_with_image("face-smile-symbolic", NULL, "G:/video_clipper/icons/link_icon.gif", 22, 22);
    g_signal_connect(link_button1, "clicked", G_CALLBACK(on_link_submitted), m);
    GtkWidget *link_button2 = create_button_with_image("face-smile-symbolic", NULL, "G:/video_clipper/icons/select_file.gif", 22, 22);

    gtk_widget_add_css_class(link_button1, "link-button");
    gtk_widget_add_css_class(link_button2, "link-button");

    gtk_box_append(GTK_BOX(link_bar), link_entry);
    gtk_box_append(GTK_BOX(link_bar), link_button1);
    gtk_box_append(GTK_BOX(link_bar), link_button2);
    gtk_box_append(GTK_BOX(middle_box), link_bar);

    gtk_box_append(GTK_BOX(middle_box), m->main_box);
    gtk_box_append(GTK_BOX(content_box), middle_box);
    gtk_box_append(GTK_BOX(top_container), content_box);

    GtkWidget *overlay_root = gtk_overlay_new();
    gtk_widget_set_hexpand(overlay_root, TRUE);
    gtk_widget_set_vexpand(overlay_root, TRUE);
    GtkWidget *background = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(background, TRUE);
    gtk_widget_set_vexpand(background, TRUE);
    gtk_widget_set_name(background, "glass-background");
    gtk_overlay_set_child(GTK_OVERLAY(overlay_root), background);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay_root), top_container);

    // --- Rodapé com imagens clicáveis ---
    GtkWidget *footer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_bottom(footer_box, 10);
    gtk_widget_set_margin_start(footer_box, 10);
    gtk_widget_set_valign(footer_box, GTK_ALIGN_END);
    gtk_widget_set_halign(footer_box, GTK_ALIGN_START);

    // GitHub
    GtkWidget *github_img = gtk_image_new_from_file("G:/video_clipper/icons/Github.png");
    gtk_widget_set_size_request(github_img, 60, 60);
    GtkGestureClick *click_github = GTK_GESTURE_CLICK(gtk_gesture_click_new());
    g_signal_connect(click_github, "released", G_CALLBACK(open_link), "https://github.com/Luquitoos");
    gtk_widget_add_controller(github_img, GTK_EVENT_CONTROLLER(click_github));
    gtk_box_append(GTK_BOX(footer_box), github_img);

    // BuyMeACoffee
    GtkWidget *coffee_img = gtk_image_new_from_file("G:/video_clipper/icons/coffee.png");
    gtk_widget_set_size_request(coffee_img, 150, 150);
    GtkGestureClick *click_coffee = GTK_GESTURE_CLICK(gtk_gesture_click_new());
    g_signal_connect(click_coffee, "released", G_CALLBACK(open_link), "https://buymeacoffee.com/luquitoos");
    gtk_widget_add_controller(coffee_img, GTK_EVENT_CONTROLLER(click_coffee));
    gtk_box_append(GTK_BOX(footer_box), coffee_img);

    gtk_overlay_add_overlay(GTK_OVERLAY(overlay_root), footer_box);

    gtk_window_set_child(GTK_WINDOW(m->window), overlay_root);
    gtk_window_present(GTK_WINDOW(m->window));
}