#include "main_window.hpp"

#include <gtk/gtk.h>

#include <memory>

namespace {

std::unique_ptr<MainWindow> mainWindow;

void loadStyle() {
    GtkCssProvider* provider = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(provider, "/org/fbneo/ultra/style.css");
    GdkScreen* screen = gdk_screen_get_default();
    if (screen) {
        gtk_style_context_add_provider_for_screen(
            screen, GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    g_object_unref(provider);
}

void onActivate(GtkApplication* application, gpointer) {
    if (!mainWindow) {
        GtkSettings* settings = gtk_settings_get_default();
        if (settings) {
            g_object_set(settings, "gtk-application-prefer-dark-theme", TRUE, nullptr);
        }
        loadStyle();
        mainWindow = std::make_unique<MainWindow>(application);
    }
    gtk_window_present(GTK_WINDOW(mainWindow->widget()));
}

} // namespace

int main(int argc, char** argv) {
    g_set_prgname("fbneo-ultra");
    g_set_application_name("FB Neo Ultra");

    GtkApplication* application = gtk_application_new(
        "org.fbneo.ultra", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(application, "activate", G_CALLBACK(onActivate), nullptr);

    const int status = g_application_run(G_APPLICATION(application), argc, argv);
    g_object_unref(application);
    mainWindow.reset();
    return status;
}
