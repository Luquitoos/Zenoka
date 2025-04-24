#include <gtk/gtk.h>
#include "splash.h"
#include "main_window.h"

// Função callback que será chamada quando o splash terminar
static void on_splash_finished(GtkApplication *app) {
    // Cria a janela principal após o splash terminar
    create_main_window(app);
}

// Função de ativação do aplicativo
static void activate(GtkApplication *app, G_GNUC_UNUSED gpointer user_data) {
    // Inicia o splash screen, passando o callback para quando terminar
    create_splash_screen(app, on_splash_finished);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.example.videoeditor", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}