#ifndef SPLASH_H
#define SPLASH_H

#include <gtk/gtk.h>

// Função para criar e exibir a splash screen
// Recebe um callback que será chamado quando a splash screen terminar
void create_splash_screen(GtkApplication *app, void (*callback)(GtkApplication*));

#endif // SPLASH_H