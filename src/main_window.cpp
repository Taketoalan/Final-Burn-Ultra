#include "main_window.hpp"

#include <algorithm>
#include <cairo.h>
#include <cctype>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkkeysyms.h>
#include <sstream>
#include <tuple>
#include <utility>

namespace {

void addClass(GtkWidget* widget, const char* name) {
    gtk_style_context_add_class(gtk_widget_get_style_context(widget), name);
}

std::string trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string lower(const std::string& value) {
    gchar* lowered = g_utf8_strdown(value.c_str(), -1);
    std::string result = lowered ? lowered : value;
    g_free(lowered);
    return result;
}

std::string baseName(const std::string& path) {
    const auto slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string prettySystem(const std::string& system) {
    return system.empty() ? "ARCADE" : system;
}

std::string truncated(const std::string& text, glong maxCharacters) {
    if (g_utf8_strlen(text.c_str(), -1) <= maxCharacters) {
        return text;
    }
    const char* end = g_utf8_offset_to_pointer(text.c_str(), maxCharacters);
    return std::string(text.c_str(), static_cast<size_t>(end - text.c_str())) + "...";
}

void setLabel(GtkWidget* label, const std::string& text) {
    gtk_label_set_text(GTK_LABEL(label), text.c_str());
}

void setStatus(GtkWidget* label, const std::string& text) {
    setLabel(label, text);
}

} // namespace

MainWindow::MainWindow(GtkApplication* application) : application_(application) {
    config_.load();
    buildUi();

    g_signal_connect(window_, "key-press-event", G_CALLBACK(on_key_press), this);
    gtk_widget_show_all(window_);
    g_idle_add(&MainWindow::on_scan_idle, this);
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUi() {
    window_ = gtk_application_window_new(application_);
    gtk_window_set_title(GTK_WINDOW(window_), "FB Neo Ultra v.1.56");

    int defaultWidth = 1240;
    int defaultHeight = 800;
    GdkDisplay* display = gdk_display_get_default();
    GdkMonitor* monitor = display ? gdk_display_get_primary_monitor(display) : nullptr;
    if (monitor) {
        GdkRectangle workarea;
        gdk_monitor_get_workarea(monitor, &workarea);
        defaultWidth = std::min(defaultWidth, static_cast<int>(workarea.width * 0.9));
        defaultHeight = std::min(defaultHeight, static_cast<int>(workarea.height * 0.8));
    }
    gtk_window_set_default_size(GTK_WINDOW(window_), defaultWidth, defaultHeight);
    gtk_window_set_position(GTK_WINDOW(window_), GTK_WIN_POS_CENTER);
    gtk_window_set_icon_name(GTK_WINDOW(window_), "fbneo-ultra");
    gtk_widget_set_name(window_, "fbneo-window");

    GError* iconError = nullptr;
    GdkPixbuf* icon = gdk_pixbuf_new_from_resource_at_scale(
        "/org/fbneo/ultra/fbneo-ultra.svg", 128, 128, TRUE, &iconError);
    if (icon) {
        gtk_window_set_icon(GTK_WINDOW(window_), icon);
        g_object_unref(icon);
    }
    if (iconError) {
        g_error_free(iconError);
    }

    gtk_window_set_titlebar(GTK_WINDOW(window_), buildHeader());

    GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window_), root);

    GtkWidget* mainPaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(mainPaned), 236);

    GtkWidget* sidebar = buildSidebar();
    gtk_widget_set_size_request(sidebar, 236, -1);
    gtk_paned_pack1(GTK_PANED(mainPaned), sidebar, FALSE, FALSE);

    GtkWidget* rightSide = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(rightSide), buildSearchBar(), FALSE, FALSE, 0);

    GtkWidget* contentPaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(contentPaned), 760);
    gtk_paned_pack1(GTK_PANED(contentPaned), buildListArea(), TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(contentPaned), buildPreviewPanel(), FALSE, FALSE);
    gtk_box_pack_start(GTK_BOX(rightSide), contentPaned, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(rightSide), buildStatusBar(), FALSE, FALSE, 0);

    gtk_paned_pack2(GTK_PANED(mainPaned), rightSide, TRUE, FALSE);
    gtk_box_pack_start(GTK_BOX(root), mainPaned, TRUE, TRUE, 0);
}

GtkWidget* MainWindow::buildHeader() {
    GtkWidget* header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "FB Neo Ultra");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), "Frontend para FinalBurn Neo");

    GtkWidget* titleBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(titleBox, GTK_ALIGN_CENTER);

    GError* logoError = nullptr;
    GdkPixbuf* logoPixbuf = gdk_pixbuf_new_from_resource_at_scale(
        "/org/fbneo/ultra/fbneo-ultra.svg", 28, 28, TRUE, &logoError);
    GtkWidget* logo = logoPixbuf ? gtk_image_new_from_pixbuf(logoPixbuf)
                                 : gtk_image_new_from_icon_name("applications-games-symbolic",
                                                                GTK_ICON_SIZE_BUTTON);
    if (logoPixbuf) {
        g_object_unref(logoPixbuf);
    }
    if (logoError) {
        g_error_free(logoError);
    }
    gtk_box_pack_start(GTK_BOX(titleBox), logo, FALSE, FALSE, 0);

    GtkWidget* title = gtk_label_new("FB NEO ULTRA");
    addClass(title, "app-title");
    gtk_box_pack_start(GTK_BOX(titleBox), title, FALSE, FALSE, 0);

    GtkWidget* badge = gtk_label_new("v1.56");
    addClass(badge, "version-badge");
    gtk_box_pack_start(GTK_BOX(titleBox), badge, FALSE, FALSE, 0);

    gtk_header_bar_set_custom_title(GTK_HEADER_BAR(header), titleBox);

    GtkWidget* menuButton = gtk_menu_button_new();
    gtk_button_set_relief(GTK_BUTTON(menuButton), GTK_RELIEF_NONE);
    GtkWidget* menuIcon = gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(menuButton), menuIcon);
    addClass(menuButton, "header-menu-button");
    gtk_widget_set_tooltip_text(menuButton, "Menú principal");

    GtkWidget* menu = gtk_menu_new();
    auto addItem = [&](const char* text, GCallback callback) {
        GtkWidget* item = gtk_menu_item_new_with_label(text);
        g_signal_connect(item, "activate", callback, this);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    };
    auto addSeparator = [&]() {
        GtkWidget* item = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    };

    addItem("Actualizar biblioteca", G_CALLBACK(on_menu_refresh));
    addItem("Preferencias", G_CALLBACK(on_menu_settings));
    addSeparator();
    addItem("Generar metadatos DAT", G_CALLBACK(on_menu_generate));
    addItem("Acerca de FB Neo Ultra", G_CALLBACK(on_menu_about));
    addSeparator();
    addItem("Salir", G_CALLBACK(on_menu_quit));

    gtk_widget_show_all(menu);
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(menuButton), menu);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), menuButton);

    GtkWidget* shortcut = gtk_label_new("F5 actualizar  ·  Ctrl+F buscar");
    addClass(shortcut, "header-hint");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), shortcut);

    return header;
}

GtkWidget* MainWindow::buildSidebar() {
    GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    addClass(scrolled, "sidebar");

    sidebarList_ = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(sidebarList_), GTK_SELECTION_SINGLE);
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(sidebarList_), TRUE);
    addClass(sidebarList_, "sidebar-list");
    g_signal_connect(sidebarList_, "row-selected", G_CALLBACK(on_sidebar_changed), this);

    gtk_container_add(GTK_CONTAINER(scrolled), sidebarList_);
    return scrolled;
}

GtkWidget* MainWindow::buildSearchBar() {
    GtkWidget* bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    addClass(bar, "toolbar");
    gtk_widget_set_margin_start(bar, 16);
    gtk_widget_set_margin_end(bar, 16);
    gtk_widget_set_margin_top(bar, 12);
    gtk_widget_set_margin_bottom(bar, 12);

    searchEntry_ = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(searchEntry_),
                                   "Buscar por título, ROM, sistema, año o fabricante…");
    gtk_widget_set_hexpand(searchEntry_, TRUE);
    addClass(searchEntry_, "search-entry");
    g_signal_connect(searchEntry_, "changed", G_CALLBACK(on_search_changed), this);
    g_signal_connect(searchEntry_, "activate", G_CALLBACK(on_search_activate), this);
    gtk_box_pack_start(GTK_BOX(bar), searchEntry_, TRUE, TRUE, 0);

    GtkWidget* refresh = gtk_button_new();
    GtkWidget* refreshIcon = gtk_image_new_from_icon_name("view-refresh-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(refresh), refreshIcon);
    gtk_button_set_relief(GTK_BUTTON(refresh), GTK_RELIEF_NONE);
    addClass(refresh, "icon-button");
    gtk_widget_set_tooltip_text(refresh, "Actualizar biblioteca (F5)");
    g_signal_connect(refresh, "clicked", G_CALLBACK(on_refresh_clicked), this);
    gtk_box_pack_start(GTK_BOX(bar), refresh, FALSE, FALSE, 0);

    countLabel_ = gtk_label_new("0 juegos");
    addClass(countLabel_, "toolbar-count");
    gtk_box_pack_end(GTK_BOX(bar), countLabel_, FALSE, FALSE, 0);

    return bar;
}

GtkWidget* MainWindow::buildListArea() {
    stack_ = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack_), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(stack_), 160);

    store_ = gtk_list_store_new(COL_COUNT,
                                G_TYPE_STRING,
                                G_TYPE_STRING,
                                G_TYPE_STRING,
                                G_TYPE_STRING,
                                G_TYPE_STRING,
                                G_TYPE_STRING,
                                G_TYPE_STRING,
                                G_TYPE_BOOLEAN,
                                G_TYPE_STRING);

    filterModel_ = gtk_tree_model_filter_new(GTK_TREE_MODEL(store_), nullptr);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filterModel_),
                                           on_filter_visible, this, nullptr);

    sortModel_ = gtk_tree_model_sort_new_with_model(filterModel_);
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(sortModel_), COL_TITLE,
                                         GTK_SORT_ASCENDING);

    treeView_ = GTK_TREE_VIEW(gtk_tree_view_new_with_model(sortModel_));
    gtk_tree_view_set_headers_visible(treeView_, TRUE);
    gtk_tree_view_set_grid_lines(treeView_, GTK_TREE_VIEW_GRID_LINES_NONE);
    gtk_tree_view_set_enable_search(treeView_, FALSE);
    gtk_tree_view_set_tooltip_column(treeView_, COL_TITLE);
    addClass(GTK_WIDGET(treeView_), "game-list");

    GtkCellRenderer* starRenderer = gtk_cell_renderer_text_new();
    g_object_set(starRenderer, "xalign", 0.5, "foreground", "#ffc857", "ypad", 9, nullptr);
    GtkTreeViewColumn* starColumn = gtk_tree_view_column_new_with_attributes(
        "", starRenderer, "text", COL_STAR, nullptr);
    gtk_tree_view_column_set_sort_column_id(starColumn, COL_FAVORITE);
    gtk_tree_view_column_set_fixed_width(starColumn, 36);
    gtk_tree_view_column_set_resizable(starColumn, FALSE);
    gtk_tree_view_append_column(treeView_, starColumn);

    GtkCellRenderer* titleRenderer = gtk_cell_renderer_text_new();
    g_object_set(titleRenderer, "ellipsize", PANGO_ELLIPSIZE_END, "weight", 600,
                 "foreground", "#e8f0f7", "ypad", 10, nullptr);
    GtkTreeViewColumn* titleColumn = gtk_tree_view_column_new_with_attributes(
        "Juego", titleRenderer, "text", COL_TITLE, nullptr);
    gtk_tree_view_column_set_sort_column_id(titleColumn, COL_TITLE);
    gtk_tree_view_column_set_expand(titleColumn, TRUE);
    gtk_tree_view_column_set_min_width(titleColumn, 240);
    gtk_tree_view_column_set_resizable(titleColumn, TRUE);
    gtk_tree_view_append_column(treeView_, titleColumn);

    GtkCellRenderer* systemRenderer = gtk_cell_renderer_text_new();
    g_object_set(systemRenderer, "ellipsize", PANGO_ELLIPSIZE_END,
                 "foreground", "#7ec8e8", "ypad", 10, nullptr);
    GtkTreeViewColumn* systemColumn = gtk_tree_view_column_new_with_attributes(
        "Sistema", systemRenderer, "text", COL_SYSTEM, nullptr);
    gtk_tree_view_column_set_sort_column_id(systemColumn, COL_SYSTEM);
    gtk_tree_view_column_set_fixed_width(systemColumn, 142);
    gtk_tree_view_column_set_resizable(systemColumn, TRUE);
    gtk_tree_view_append_column(treeView_, systemColumn);

    GtkCellRenderer* yearRenderer = gtk_cell_renderer_text_new();
    g_object_set(yearRenderer, "xalign", 0.5, "foreground", "#8291a0", "ypad", 10, nullptr);
    GtkTreeViewColumn* yearColumn = gtk_tree_view_column_new_with_attributes(
        "Año", yearRenderer, "text", COL_YEAR, nullptr);
    gtk_tree_view_column_set_sort_column_id(yearColumn, COL_YEAR);
    gtk_tree_view_column_set_fixed_width(yearColumn, 70);
    gtk_tree_view_column_set_resizable(yearColumn, FALSE);
    gtk_tree_view_append_column(treeView_, yearColumn);

    GtkTreeSelection* selection = gtk_tree_view_get_selection(treeView_);
    g_signal_connect(selection, "changed", G_CALLBACK(on_selection_changed), this);
    g_signal_connect(treeView_, "row-activated", G_CALLBACK(on_row_activated), this);

    GtkWidget* scrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    addClass(scrolled, "list-scroll");
    gtk_container_add(GTK_CONTAINER(scrolled), GTK_WIDGET(treeView_));
    gtk_stack_add_named(GTK_STACK(stack_), scrolled, "games");

    GtkWidget* empty = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_halign(empty, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(empty, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(empty, 48);
    gtk_widget_set_margin_end(empty, 48);
    addClass(empty, "empty-state");

    GtkWidget* emptyIcon = gtk_image_new_from_icon_name("applications-games-symbolic",
                                                        GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(emptyIcon), 96);
    addClass(emptyIcon, "empty-icon");
    gtk_box_pack_start(GTK_BOX(empty), emptyIcon, FALSE, FALSE, 0);

    GtkWidget* emptyTitle = gtk_label_new("Tu biblioteca está vacía");
    addClass(emptyTitle, "empty-title");
    gtk_box_pack_start(GTK_BOX(empty), emptyTitle, FALSE, FALSE, 0);

    GtkWidget* emptySubtitle = gtk_label_new(
        "Añade las carpetas donde guardas tus ROMs de FBNeo y pulsa actualizar. "
        "FB Neo Ultra detectará automáticamente los juegos y sus datos.");
    gtk_label_set_line_wrap(GTK_LABEL(emptySubtitle), TRUE);
    gtk_label_set_justify(GTK_LABEL(emptySubtitle), GTK_JUSTIFY_CENTER);
    gtk_label_set_max_width_chars(GTK_LABEL(emptySubtitle), 54);
    addClass(emptySubtitle, "empty-subtitle");
    gtk_box_pack_start(GTK_BOX(empty), emptySubtitle, FALSE, FALSE, 0);

    GtkWidget* configure = gtk_button_new_with_label("Configurar biblioteca");
    addClass(configure, "settings-button");
    g_signal_connect(configure, "clicked", G_CALLBACK(on_menu_settings), this);
    gtk_widget_set_halign(configure, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(empty), configure, FALSE, FALSE, 8);

    gtk_stack_add_named(GTK_STACK(stack_), empty, "empty");
    gtk_stack_set_visible_child_name(GTK_STACK(stack_), "empty");

    return stack_;
}

GtkWidget* MainWindow::buildPreviewPanel() {
    GtkWidget* panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(panel, 332, -1);
    gtk_widget_set_margin_start(panel, 16);
    gtk_widget_set_margin_end(panel, 16);
    gtk_widget_set_margin_top(panel, 14);
    gtk_widget_set_margin_bottom(panel, 14);
    addClass(panel, "preview-panel");

    GtkWidget* artFrame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(artFrame, 300, 180);
    gtk_widget_set_halign(artFrame, GTK_ALIGN_CENTER);
    addClass(artFrame, "art-frame");

    previewImage_ = gtk_image_new();
    gtk_widget_set_size_request(previewImage_, 300, 180);
    gtk_widget_set_halign(previewImage_, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(previewImage_, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(artFrame), previewImage_, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(panel), artFrame, FALSE, FALSE, 0);

    titleLabel_ = gtk_label_new("Selecciona un juego");
    gtk_label_set_xalign(GTK_LABEL(titleLabel_), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(titleLabel_), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(titleLabel_), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(GTK_LABEL(titleLabel_), 32);
    addClass(titleLabel_, "game-title");
    gtk_box_pack_start(GTK_BOX(panel), titleLabel_, FALSE, FALSE, 0);

    GtkWidget* badges = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    systemBadge_ = gtk_label_new("SISTEMA");
    addClass(systemBadge_, "badge");
    yearBadge_ = gtk_label_new("—");
    addClass(yearBadge_, "badge-alt");
    gtk_box_pack_start(GTK_BOX(badges), systemBadge_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(badges), yearBadge_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(panel), badges, FALSE, FALSE, 0);

    manufacturerLabel_ = gtk_label_new("Elige un título de la lista");
    gtk_label_set_xalign(GTK_LABEL(manufacturerLabel_), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(manufacturerLabel_), PANGO_ELLIPSIZE_END);
    addClass(manufacturerLabel_, "meta-label");
    gtk_box_pack_start(GTK_BOX(panel), manufacturerLabel_, FALSE, FALSE, 0);

    GtkWidget* separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    addClass(separator, "preview-separator");
    gtk_box_pack_start(GTK_BOX(panel), separator, FALSE, FALSE, 4);

    GtkWidget* pathTitle = gtk_label_new("ARCHIVO");
    gtk_label_set_xalign(GTK_LABEL(pathTitle), 0.0);
    addClass(pathTitle, "path-title");
    gtk_box_pack_start(GTK_BOX(panel), pathTitle, FALSE, FALSE, 0);

    pathLabel_ = gtk_label_new("Sin selección");
    gtk_label_set_xalign(GTK_LABEL(pathLabel_), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(pathLabel_), PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_selectable(GTK_LABEL(pathLabel_), TRUE);
    addClass(pathLabel_, "path-label");
    gtk_box_pack_start(GTK_BOX(panel), pathLabel_, FALSE, FALSE, 0);

    GtkWidget* spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_pack_start(GTK_BOX(panel), spacer, TRUE, TRUE, 0);

    playButton_ = gtk_button_new();
    gtk_widget_set_size_request(playButton_, -1, 44);
    GtkWidget* playBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(playBox, GTK_ALIGN_CENTER);
    GtkWidget* playIcon = gtk_image_new_from_icon_name("media-playback-start-symbolic",
                                                       GTK_ICON_SIZE_BUTTON);
    GtkWidget* playText = gtk_label_new("JUGAR");
    addClass(playText, "play-label");
    gtk_box_pack_start(GTK_BOX(playBox), playIcon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(playBox), playText, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(playButton_), playBox);
    addClass(playButton_, "play-button");
    gtk_widget_set_tooltip_text(playButton_, "Iniciar el juego seleccionado (Enter)");
    gtk_widget_set_sensitive(playButton_, FALSE);
    g_signal_connect(playButton_, "clicked", G_CALLBACK(on_play_clicked), this);
    gtk_box_pack_start(GTK_BOX(panel), playButton_, FALSE, FALSE, 0);

    favoriteButton_ = GTK_TOGGLE_BUTTON(gtk_toggle_button_new_with_label("☆ Favorito"));
    addClass(GTK_WIDGET(favoriteButton_), "favorite-button");
    gtk_widget_set_sensitive(GTK_WIDGET(favoriteButton_), FALSE);
    g_signal_connect(favoriteButton_, "toggled", G_CALLBACK(on_favorite_toggled), this);
    gtk_box_pack_start(GTK_BOX(panel), GTK_WIDGET(favoriteButton_), FALSE, FALSE, 0);

    GdkPixbuf* placeholder = makePlaceholder("FB NEO ULTRA", "Selecciona un juego");
    gtk_image_set_from_pixbuf(GTK_IMAGE(previewImage_), placeholder);
    g_object_unref(placeholder);

    return panel;
}

GtkWidget* MainWindow::buildStatusBar() {
    GtkWidget* bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    addClass(bar, "status-bar");
    gtk_widget_set_margin_start(bar, 16);
    gtk_widget_set_margin_end(bar, 16);
    gtk_widget_set_margin_top(bar, 6);
    gtk_widget_set_margin_bottom(bar, 8);

    statusLabel_ = gtk_label_new("Iniciando FB Neo Ultra…");
    gtk_label_set_xalign(GTK_LABEL(statusLabel_), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(statusLabel_), PANGO_ELLIPSIZE_END);
    addClass(statusLabel_, "status-text");
    gtk_box_pack_start(GTK_BOX(bar), statusLabel_, TRUE, TRUE, 0);

    emulatorLabel_ = gtk_label_new("FBNeo no encontrado");
    gtk_label_set_xalign(GTK_LABEL(emulatorLabel_), 1.0);
    gtk_label_set_ellipsize(GTK_LABEL(emulatorLabel_), PANGO_ELLIPSIZE_MIDDLE);
    addClass(emulatorLabel_, "status-emulator");
    gtk_box_pack_end(GTK_BOX(bar), emulatorLabel_, FALSE, FALSE, 0);

    return bar;
}

void MainWindow::ensureDatabase() {
    if (database_.load(config_.datDir)) {
        return;
    }
    if (config_.autoDat && !config_.emulatorPath.empty()) {
        generateMetadata(true);
        config_.datDir = Config::detectDatDir();
        database_.load(config_.datDir);
    }
}

bool MainWindow::generateMetadata(bool quiet) {
    if (config_.emulatorPath.empty() ||
        !g_file_test(config_.emulatorPath.c_str(), G_FILE_TEST_IS_EXECUTABLE)) {
        if (!quiet) {
            showMessage("FBNeo no encontrado",
                        "Configura la ruta del ejecutable de FBNeo en Preferencias "
                        "para poder generar los metadatos.");
        }
        return false;
    }

    setStatus(statusLabel_, "Generando metadatos DAT con FBNeo…");
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }

    gchar* argv[] = {
        const_cast<gchar*>(config_.emulatorPath.c_str()),
        const_cast<gchar*>("-dat"),
        nullptr,
    };
    GError* error = nullptr;
    const gboolean ok = g_spawn_sync(nullptr, argv, nullptr, G_SPAWN_SEARCH_PATH,
                                     nullptr, nullptr, nullptr, nullptr, nullptr, &error);
    if (!ok) {
        if (!quiet) {
            showMessage("No se pudieron generar los metadatos",
                        error ? error->message : "Error desconocido al ejecutar FBNeo.");
        }
        if (error) {
            g_error_free(error);
        }
        return false;
    }

    config_.datDir = Config::detectDatDir();
    database_.load(config_.datDir);
    if (!quiet && database_.loaded()) {
        const std::string message = "Se generaron los metadatos de " +
                                    std::to_string(database_.size()) + " juegos.";
        showMessage("Metadatos actualizados", message, GTK_MESSAGE_INFO);
    }
    return database_.loaded();
}

void MainWindow::scanLibrary() {
    setStatus(statusLabel_, "Escaneando biblioteca de ROMs…");
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }

    config_.ensureDefaults();
    if (config_.syncRomPaths) {
        config_.syncRomPathsToFbneoIni();
    }
    ensureDatabase();

    metadataCount_ = 0;
    games_ = scanner_.scan(config_, database_);
    gameIndex_.clear();
    for (size_t i = 0; i < games_.size(); ++i) {
        gameIndex_[games_[i].rom] = i;
        if (database_.find(games_[i].rom)) {
            ++metadataCount_;
        }
    }

    const std::string previous = selectedRom_;
    populateModel();
    rebuildSidebar();
    refilter();
    if (!previous.empty()) {
        selectRom(previous);
    } else {
        selectFirstVisible();
    }
    updateStatus();
    updateFilterCount();
}

void MainWindow::populateModel() {
    gtk_list_store_clear(store_);
    for (const Game& game : games_) {
        GtkTreeIter iter;
        gtk_list_store_append(store_, &iter);
        gtk_list_store_set(store_, &iter,
                           COL_STAR, game.favorite ? "★" : "",
                           COL_TITLE, game.title.c_str(),
                           COL_SYSTEM, game.system.c_str(),
                           COL_YEAR, game.year.c_str(),
                           COL_MANUFACTURER, game.manufacturer.c_str(),
                           COL_ROM, game.rom.c_str(),
                           COL_PATH, game.path.c_str(),
                           COL_FAVORITE, game.favorite,
                           COL_SEARCH, game.search.c_str(),
                           -1);
    }
}

void MainWindow::rebuildSidebar() {
    rebuildingSidebar_ = true;

    GList* children = gtk_container_get_children(GTK_CONTAINER(sidebarList_));
    std::vector<GtkWidget*> widgets;
    for (GList* item = children; item; item = item->next) {
        widgets.push_back(GTK_WIDGET(item->data));
    }
    g_list_free(children);
    for (GtkWidget* widget : widgets) {
        gtk_widget_destroy(widget);
    }

    auto addSection = [&](const char* text) {
        GtkWidget* row = gtk_list_box_row_new();
        gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
        GtkWidget* label = gtk_label_new(text);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_widget_set_margin_start(label, 14);
        gtk_widget_set_margin_end(label, 14);
        gtk_widget_set_margin_top(label, 16);
        gtk_widget_set_margin_bottom(label, 6);
        addClass(label, "section-label");
        gtk_container_add(GTK_CONTAINER(row), label);
        gtk_widget_show_all(row);
        gtk_list_box_insert(GTK_LIST_BOX(sidebarList_), row, -1);
    };

    auto addRow = [&](const std::string& key, const char* iconName,
                      const std::string& label, size_t count) {
        GtkWidget* row = gtk_list_box_row_new();
        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
        gtk_widget_set_margin_start(box, 12);
        gtk_widget_set_margin_end(box, 12);
        gtk_widget_set_margin_top(box, 7);
        gtk_widget_set_margin_bottom(box, 7);
        addClass(box, "sidebar-item");

        GtkWidget* icon = gtk_image_new_from_icon_name(iconName, GTK_ICON_SIZE_MENU);
        gtk_widget_set_opacity(icon, 0.82);
        addClass(icon, "sidebar-icon");
        GtkWidget* text = gtk_label_new(label.c_str());
        gtk_label_set_xalign(GTK_LABEL(text), 0.0);
        gtk_label_set_ellipsize(GTK_LABEL(text), PANGO_ELLIPSIZE_END);
        gtk_widget_set_hexpand(text, TRUE);
        addClass(text, "sidebar-text");
        GtkWidget* counter = gtk_label_new(std::to_string(count).c_str());
        addClass(counter, "sidebar-count");

        gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(box), text, TRUE, TRUE, 0);
        gtk_box_pack_end(GTK_BOX(box), counter, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(row), box);
        g_object_set_data_full(G_OBJECT(row), "filter-key", g_strdup(key.c_str()), g_free);
        gtk_widget_show_all(row);
        gtk_list_box_insert(GTK_LIST_BOX(sidebarList_), row, -1);
    };

    addSection("BIBLIOTECA");
    addRow("all", "view-grid-symbolic", "Todos los juegos", games_.size());

    size_t favoriteCount = 0;
    std::unordered_map<std::string, size_t> systemCounts;
    for (const Game& game : games_) {
        if (game.favorite) {
            ++favoriteCount;
        }
        ++systemCounts[game.system];
    }
    addRow("favorites", "emblem-favorite-symbolic", "Favoritos", favoriteCount);

    if (!systemCounts.empty()) {
        addSection("SISTEMAS");
        std::vector<std::string> systems;
        systems.reserve(systemCounts.size());
        for (const auto& entry : systemCounts) {
            systems.push_back(entry.first);
        }
        auto rank = [](const std::string& system) {
            if (system == "Arcade") return 0;
            if (system == "CPS1") return 1;
            if (system == "CPS2") return 2;
            if (system == "CPS3") return 3;
            if (system == "Neo Geo") return 4;
            return 10;
        };
        std::sort(systems.begin(), systems.end(), [&](const std::string& a, const std::string& b) {
            const int rankA = rank(a);
            const int rankB = rank(b);
            if (rankA != rankB) {
                return rankA < rankB;
            }
            return lower(a) < lower(b);
        });
        currentSystems_ = systems;
        for (const std::string& system : systems) {
            addRow("system:" + system, "applications-games-symbolic", system,
                   systemCounts[system]);
        }
    } else {
        currentSystems_.clear();
    }

    bool filterExists = currentFilter_ == "all" || currentFilter_ == "favorites";
    if (!filterExists && currentFilter_.rfind("system:", 0) == 0) {
        const std::string system = currentFilter_.substr(7);
        filterExists = std::find(currentSystems_.begin(), currentSystems_.end(), system) !=
                       currentSystems_.end();
    }
    if (!filterExists) {
        currentFilter_ = "all";
    }

    GtkWidget* selectedRow = nullptr;
    GList* rows = gtk_container_get_children(GTK_CONTAINER(sidebarList_));
    for (GList* item = rows; item; item = item->next) {
        GtkWidget* row = GTK_WIDGET(item->data);
        const char* key = static_cast<const char*>(g_object_get_data(G_OBJECT(row), "filter-key"));
        if (key && currentFilter_ == key) {
            selectedRow = row;
            break;
        }
    }
    g_list_free(rows);

    rebuildingSidebar_ = false;

    if (!selectedRow) {
        GtkListBoxRow* first = gtk_list_box_get_row_at_index(GTK_LIST_BOX(sidebarList_), 1);
        if (first) {
            gtk_list_box_select_row(GTK_LIST_BOX(sidebarList_), first);
        }
    } else {
        gtk_list_box_select_row(GTK_LIST_BOX(sidebarList_), GTK_LIST_BOX_ROW(selectedRow));
    }
}

void MainWindow::refilter() {
    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filterModel_));

    GtkTreeSelection* selection = gtk_tree_view_get_selection(treeView_);
    GtkTreeIter iter;
    bool visible = false;
    if (gtk_tree_selection_get_selected(selection, nullptr, &iter)) {
        GtkTreeIter filterIter;
        gtk_tree_model_sort_convert_iter_to_child_iter(
            GTK_TREE_MODEL_SORT(sortModel_), &filterIter, &iter);
        visible = on_filter_visible(GTK_TREE_MODEL(filterModel_), &filterIter, this);
    }
    if (!visible) {
        selectFirstVisible();
    }

    updateFilterCount();
}

void MainWindow::selectFirstVisible() {
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_first(sortModel_, &iter)) {
        GtkTreeSelection* selection = gtk_tree_view_get_selection(treeView_);
        gtk_tree_selection_unselect_all(selection);
        clearPreview();
        return;
    }
    GtkTreeSelection* selection = gtk_tree_view_get_selection(treeView_);
    gtk_tree_selection_select_iter(selection, &iter);
    GtkTreePath* path = gtk_tree_model_get_path(sortModel_, &iter);
    if (path) {
        gtk_tree_view_scroll_to_cell(treeView_, path, nullptr, TRUE, 0.5f, 0.0f);
        gtk_tree_path_free(path);
    }
}

void MainWindow::selectRom(const std::string& rom) {
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_first(sortModel_, &iter)) {
        selectFirstVisible();
        return;
    }

    do {
        gchar* current = nullptr;
        gtk_tree_model_get(sortModel_, &iter, COL_ROM, &current, -1);
        const bool match = current && rom == current;
        g_free(current);
        if (match) {
            GtkTreeSelection* selection = gtk_tree_view_get_selection(treeView_);
            gtk_tree_selection_select_iter(selection, &iter);
            GtkTreePath* path = gtk_tree_model_get_path(sortModel_, &iter);
            if (path) {
                gtk_tree_view_scroll_to_cell(treeView_, path, nullptr, TRUE, 0.5f, 0.0f);
                gtk_tree_path_free(path);
            }
            return;
        }
    } while (gtk_tree_model_iter_next(sortModel_, &iter));

    selectFirstVisible();
}

void MainWindow::updatePreview() {
    GtkTreeSelection* selection = gtk_tree_view_get_selection(treeView_);
    GtkTreeModel* model = nullptr;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        clearPreview();
        return;
    }

    gchar* title = nullptr;
    gchar* system = nullptr;
    gchar* year = nullptr;
    gchar* manufacturer = nullptr;
    gchar* rom = nullptr;
    gchar* path = nullptr;
    gboolean favorite = FALSE;

    gtk_tree_model_get(model, &iter,
                       COL_TITLE, &title,
                       COL_SYSTEM, &system,
                       COL_YEAR, &year,
                       COL_MANUFACTURER, &manufacturer,
                       COL_ROM, &rom,
                       COL_PATH, &path,
                       COL_FAVORITE, &favorite,
                       -1);

    selectedRom_ = rom ? rom : "";
    setLabel(titleLabel_, title && *title ? title : "Juego sin título");
    setLabel(systemBadge_, prettySystem(system ? system : ""));
    setLabel(yearBadge_, year && *year ? year : "—");
    if (manufacturer && *manufacturer) {
        setLabel(manufacturerLabel_, std::string("Fabricante: ") + manufacturer);
    } else {
        setLabel(manufacturerLabel_, "Fabricante desconocido");
    }
    setLabel(pathLabel_, path && *path ? path : "Sin archivo");
    if (path && *path) {
        gtk_widget_set_tooltip_text(pathLabel_, path);
    } else {
        gtk_widget_set_tooltip_text(pathLabel_, nullptr);
    }

    gtk_widget_set_sensitive(playButton_, TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(favoriteButton_), TRUE);
    updatingPreview_ = true;
    gtk_toggle_button_set_active(favoriteButton_, favorite);
    updatingPreview_ = false;
    gtk_button_set_label(GTK_BUTTON(favoriteButton_),
                         favorite ? "★ Favorito" : "☆ Favorito");

    GdkPixbuf* artwork = artworkFor(selectedRom_);
    if (!artwork) {
        std::string subtitle = system ? system : "ARCADE";
        if (year && *year) {
            subtitle += "  ·  " + std::string(year);
        }
        artwork = makePlaceholder(title && *title ? title : "FB NEO ULTRA", subtitle);
    }
    gtk_image_set_from_pixbuf(GTK_IMAGE(previewImage_), artwork);
    g_object_unref(artwork);

    g_free(title);
    g_free(system);
    g_free(year);
    g_free(manufacturer);
    g_free(rom);
    g_free(path);
}

void MainWindow::clearPreview() {
    selectedRom_.clear();
    setLabel(titleLabel_, "Selecciona un juego");
    setLabel(systemBadge_, "SISTEMA");
    setLabel(yearBadge_, "—");
    setLabel(manufacturerLabel_, "Elige un título de la lista");
    setLabel(pathLabel_, "Sin selección");
    gtk_widget_set_tooltip_text(pathLabel_, nullptr);
    gtk_widget_set_sensitive(playButton_, FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(favoriteButton_), FALSE);
    updatingPreview_ = true;
    gtk_toggle_button_set_active(favoriteButton_, FALSE);
    updatingPreview_ = false;
    gtk_button_set_label(GTK_BUTTON(favoriteButton_), "☆ Favorito");

    GdkPixbuf* placeholder = makePlaceholder("FB NEO ULTRA", "Selecciona un juego");
    gtk_image_set_from_pixbuf(GTK_IMAGE(previewImage_), placeholder);
    g_object_unref(placeholder);
}

void MainWindow::updateStatus() {
    if (games_.empty()) {
        setStatus(statusLabel_, "No se encontraron ROMs. Añade tus carpetas en Preferencias.");
        gtk_stack_set_visible_child_name(GTK_STACK(stack_), "empty");
    } else {
        const std::string message = "Biblioteca lista · " + std::to_string(games_.size()) +
                                    " juegos · " + std::to_string(metadataCount_) +
                                    " con metadatos" +
                                    (config_.emulatorPath.empty() ? " · FBNeo no encontrado" : "");
        setStatus(statusLabel_, message);
        gtk_stack_set_visible_child_name(GTK_STACK(stack_), "games");
    }

    if (config_.emulatorPath.empty()) {
        setLabel(emulatorLabel_, "FBNeo no encontrado");
        gtk_widget_set_tooltip_text(emulatorLabel_, "Configura la ruta de FBNeo en Preferencias");
    } else {
        setLabel(emulatorLabel_, baseName(config_.emulatorPath));
        gtk_widget_set_tooltip_text(emulatorLabel_, config_.emulatorPath.c_str());
    }
}

void MainWindow::updateFilterCount() {
    if (!countLabel_) {
        return;
    }
    const int visible = sortModel_ ? gtk_tree_model_iter_n_children(sortModel_, nullptr) : 0;
    const std::string text = std::to_string(visible) + " de " +
                             std::to_string(games_.size()) + " juegos";
    setLabel(countLabel_, text);
}

void MainWindow::launchSelected() {
    if (selectedRom_.empty()) {
        return;
    }
    const Game* game = findGame(selectedRom_);
    if (!game) {
        return;
    }
    launchGame(*game);
}

void MainWindow::launchGame(const Game& game) {
    if (config_.emulatorPath.empty() ||
        !g_file_test(config_.emulatorPath.c_str(), G_FILE_TEST_IS_EXECUTABLE)) {
        GtkWidget* dialog = gtk_message_dialog_new(
            GTK_WINDOW(window_), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_NONE,
            "No se encontró el ejecutable de FBNeo");
        gtk_message_dialog_format_secondary_text(
            GTK_MESSAGE_DIALOG(dialog),
            "Selecciona el ejecutable de FinalBurn Neo para poder iniciar los juegos.");
        gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancelar", GTK_RESPONSE_CANCEL);
        gtk_dialog_add_button(GTK_DIALOG(dialog), "Abrir preferencias", GTK_RESPONSE_OK);
        const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        if (response == GTK_RESPONSE_OK) {
            showSettings();
        }
        return;
    }

    config_.syncRomPathsToFbneoIni();

    std::vector<std::string> arguments;
    arguments.push_back(config_.emulatorPath);
    if (config_.fullscreen) {
        arguments.push_back("-fullscreen");
    }

    if (!config_.extraArgs.empty()) {
        gchar** parsed = nullptr;
        gint parsedCount = 0;
        GError* parseError = nullptr;
        if (g_shell_parse_argv(config_.extraArgs.c_str(), &parsedCount, &parsed, &parseError)) {
            for (gint i = 0; i < parsedCount; ++i) {
                arguments.emplace_back(parsed[i]);
            }
            g_strfreev(parsed);
        }
        if (parseError) {
            g_error_free(parseError);
        }
    }

    arguments.push_back(game.rom);

    std::vector<gchar*> argv;
    argv.reserve(arguments.size() + 1);
    for (std::string& argument : arguments) {
        argv.push_back(const_cast<gchar*>(argument.c_str()));
    }
    argv.push_back(nullptr);

    gchar* workingDirectory = g_path_get_dirname(config_.emulatorPath.c_str());
    GPid pid = 0;
    GError* error = nullptr;
    const gboolean launched = g_spawn_async(
        workingDirectory, argv.data(), nullptr,
        GSpawnFlags(G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD),
        nullptr, nullptr, &pid, &error);
    g_free(workingDirectory);

    if (!launched) {
        showMessage("No se pudo iniciar FBNeo",
                    error ? error->message : "Error desconocido al ejecutar el emulador.");
        if (error) {
            g_error_free(error);
        }
        return;
    }

    busy_ = true;
    gtk_widget_set_sensitive(playButton_, FALSE);
    setStatus(statusLabel_, "Ejecutando " + game.title + "…");
    gtk_window_iconify(GTK_WINDOW(window_));
    g_child_watch_add(pid, &MainWindow::on_child_exit, this);
}

void MainWindow::toggleFavorite(bool value) {
    if (selectedRom_.empty()) {
        return;
    }

    const bool currentlyFavorite = config_.isFavorite(selectedRom_);
    if (currentlyFavorite != value) {
        config_.toggleFavorite(selectedRom_);
    }
    config_.save();

    if (Game* game = findGame(selectedRom_)) {
        game->favorite = config_.isFavorite(selectedRom_);
    }

    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store_), &iter)) {
        do {
            gchar* rom = nullptr;
            gtk_tree_model_get(GTK_TREE_MODEL(store_), &iter, COL_ROM, &rom, -1);
            const bool match = rom && selectedRom_ == rom;
            g_free(rom);
            if (match) {
                const bool favorite = config_.isFavorite(selectedRom_);
                gtk_list_store_set(store_, &iter,
                                   COL_STAR, favorite ? "★" : "",
                                   COL_FAVORITE, favorite,
                                   -1);
                break;
            }
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store_), &iter));
    }

    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filterModel_));
    updateFilterCount();

    if (currentFilter_ == "favorites" && !config_.isFavorite(selectedRom_)) {
        selectFirstVisible();
    }
}

Game* MainWindow::findGame(const std::string& rom) {
    const auto it = gameIndex_.find(rom);
    if (it == gameIndex_.end()) {
        return nullptr;
    }
    return &games_[it->second];
}

const Game* MainWindow::findGame(const std::string& rom) const {
    const auto it = gameIndex_.find(rom);
    if (it == gameIndex_.end()) {
        return nullptr;
    }
    return &games_[it->second];
}

bool MainWindow::matchesSearch(const std::string& search) const {
    if (searchQuery_.empty()) {
        return true;
    }

    std::istringstream stream(searchQuery_);
    std::string token;
    while (stream >> token) {
        if (search.find(token) == std::string::npos) {
            return false;
        }
    }
    return true;
}

GdkPixbuf* MainWindow::artworkFor(const std::string& rom) const {
    if (rom.empty()) {
        return nullptr;
    }

    const std::string normalized = lower(rom);
    const std::vector<std::string> extensions = {".png", ".jpg", ".jpeg", ".gif", ".bmp", ".webp"};

    for (const std::string& directory : config_.artworkDirs()) {
        for (const std::string& extension : extensions) {
            const std::string path = directory + "/" + normalized + extension;
            if (g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR)) {
                GError* error = nullptr;
                GdkPixbuf* pixbuf = gdk_pixbuf_new_from_file_at_scale(
                    path.c_str(), 300, 180, TRUE, &error);
                if (pixbuf) {
                    return pixbuf;
                }
                if (error) {
                    g_error_free(error);
                }
            }
        }
    }
    return nullptr;
}

GdkPixbuf* MainWindow::makePlaceholder(const std::string& title,
                                       const std::string& subtitle) const {
    constexpr int width = 300;
    constexpr int height = 180;

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* cairo = cairo_create(surface);

    cairo_set_source_rgb(cairo, 0.035, 0.055, 0.075);
    cairo_paint(cairo);

    cairo_pattern_t* gradient = cairo_pattern_create_linear(0, 0, width, height);
    cairo_pattern_add_color_stop_rgba(gradient, 0.0, 0.0, 0.55, 0.68, 0.22);
    cairo_pattern_add_color_stop_rgba(gradient, 1.0, 0.05, 0.07, 0.10, 0.0);
    cairo_set_source(cairo, gradient);
    cairo_paint(cairo);
    cairo_pattern_destroy(gradient);

    cairo_set_source_rgba(cairo, 0.20, 0.65, 0.80, 0.10);
    cairo_set_line_width(cairo, 1.0);
    for (int x = 12; x < width; x += 24) {
        cairo_move_to(cairo, x, 0);
        cairo_line_to(cairo, x, height);
    }
    for (int y = 12; y < height; y += 24) {
        cairo_move_to(cairo, 0, y);
        cairo_line_to(cairo, width, y);
    }
    cairo_stroke(cairo);

    cairo_set_source_rgba(cairo, 0.0, 0.88, 1.0, 0.55);
    cairo_set_line_width(cairo, 1.5);
    cairo_rectangle(cairo, 10.5, 10.5, width - 21, height - 21);
    cairo_stroke(cairo);

    auto drawCentered = [&](const std::string& text, double size, double y,
                            double r, double g, double b, double a, bool bold) {
        cairo_select_font_face(cairo, "Sans",
                               CAIRO_FONT_SLANT_NORMAL,
                               bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cairo, size);
        cairo_text_extents_t extents;
        cairo_text_extents(cairo, text.c_str(), &extents);
        cairo_set_source_rgba(cairo, r, g, b, a);
        cairo_move_to(cairo, (width - extents.width) / 2.0 - extents.x_bearing, y);
        cairo_show_text(cairo, text.c_str());
    };

    drawCentered("FB NEO ULTRA", 25, 84, 0.0, 0.88, 1.0, 0.95, true);
    const std::string shortSubtitle = truncated(subtitle, 34);
    drawCentered(shortSubtitle, 12, 116, 0.78, 0.87, 0.93, 0.82, false);
    drawCentered(truncated(title, 30), 15, 146, 0.95, 0.72, 0.30, 0.90, true);

    cairo_destroy(cairo);
    GdkPixbuf* result = gdk_pixbuf_get_from_surface(surface, 0, 0, width, height);
    cairo_surface_destroy(surface);
    return result;
}

void MainWindow::showSettings() {
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Preferencias — FB Neo Ultra", GTK_WINDOW(window_),
        GtkDialogFlags(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        "Cancelar", GTK_RESPONSE_CANCEL, "Guardar", GTK_RESPONSE_OK, nullptr);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 720, 620);
    addClass(dialog, "settings-dialog");

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 18);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_container_add(GTK_CONTAINER(content), box);

    GtkWidget* heading = gtk_label_new("RUTAS Y EJECUCIÓN");
    gtk_label_set_xalign(GTK_LABEL(heading), 0.0);
    addClass(heading, "settings-heading");
    gtk_box_pack_start(GTK_BOX(box), heading, FALSE, FALSE, 0);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 0);

    auto addPathRow = [&](int row, const char* labelText, const char* initial,
                          bool folder, const char* dialogTitle, GtkWidget** targetEntry) -> GtkWidget* {
        GtkWidget* label = gtk_label_new(labelText);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);

        GtkWidget* entry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(entry), initial ? initial : "");
        gtk_widget_set_hexpand(entry, TRUE);
        gtk_grid_attach(GTK_GRID(grid), entry, 1, row, 1, 1);
        *targetEntry = entry;

        GtkWidget* browse = gtk_button_new();
        GtkWidget* icon = gtk_image_new_from_icon_name(
            folder ? "folder-open-symbolic" : "document-open-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(browse), icon);
        gtk_button_set_relief(GTK_BUTTON(browse), GTK_RELIEF_NONE);
        addClass(browse, "icon-button");
        gtk_grid_attach(GTK_GRID(grid), browse, 2, row, 1, 1);

        g_signal_connect_data(browse, "clicked",
            reinterpret_cast<GCallback>(+[](GtkButton*, gpointer data) {
                auto* payload = static_cast<std::tuple<GtkWidget*, bool, const char*, GtkWidget*>*>(data);
                GtkWidget* entryWidget = std::get<0>(*payload);
                const bool chooseFolder = std::get<1>(*payload);
                const char* title = std::get<2>(*payload);
                GtkWindow* parent = GTK_WINDOW(std::get<3>(*payload));

                GtkWidget* chooser = gtk_file_chooser_dialog_new(
                    title, parent,
                    chooseFolder ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER : GTK_FILE_CHOOSER_ACTION_OPEN,
                    "Cancelar", GTK_RESPONSE_CANCEL, "Seleccionar", GTK_RESPONSE_ACCEPT, nullptr);
                const char* current = gtk_entry_get_text(GTK_ENTRY(entryWidget));
                if (current && *current) {
                    if (chooseFolder) {
                        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), current);
                    } else {
                        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(chooser), current);
                    }
                }
                if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
                    gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
                    if (filename) {
                        gtk_entry_set_text(GTK_ENTRY(entryWidget), filename);
                        g_free(filename);
                    }
                }
                gtk_widget_destroy(chooser);
            }),
            new std::tuple<GtkWidget*, bool, const char*, GtkWidget*>(entry, folder, dialogTitle, dialog),
            +[](gpointer data, GClosure*) {
                delete static_cast<std::tuple<GtkWidget*, bool, const char*, GtkWidget*>*>(data);
            }, static_cast<GConnectFlags>(0));
        return browse;
    };

    GtkWidget* emulatorEntry = nullptr;
    GtkWidget* datEntry = nullptr;
    GtkWidget* artworkEntry = nullptr;
    addPathRow(0, "Ejecutable FBNeo", config_.emulatorPath.c_str(), false,
               "Seleccionar ejecutable de FBNeo", &emulatorEntry);
    addPathRow(1, "Carpeta DAT", config_.datDir.c_str(), true,
               "Seleccionar carpeta de metadatos DAT", &datEntry);
    addPathRow(2, "Carpeta de artwork", config_.artworkDir.c_str(), true,
               "Seleccionar carpeta de imágenes", &artworkEntry);

    GtkWidget* argsLabel = gtk_label_new("Argumentos extra");
    gtk_label_set_xalign(GTK_LABEL(argsLabel), 0.0);
    gtk_grid_attach(GTK_GRID(grid), argsLabel, 0, 3, 1, 1);
    GtkWidget* argsEntry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(argsEntry), "-novsync -integerscale");
    gtk_entry_set_text(GTK_ENTRY(argsEntry), config_.extraArgs.c_str());
    gtk_widget_set_hexpand(argsEntry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), argsEntry, 1, 3, 2, 1);

    GtkWidget* romHeading = gtk_label_new("CARPETAS DE ROMS");
    gtk_label_set_xalign(GTK_LABEL(romHeading), 0.0);
    addClass(romHeading, "settings-heading");
    gtk_box_pack_start(GTK_BOX(box), romHeading, FALSE, FALSE, 0);

    GtkWidget* romScrolled = gtk_scrolled_window_new(nullptr, nullptr);
    gtk_widget_set_size_request(romScrolled, -1, 150);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(romScrolled), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    addClass(romScrolled, "settings-roms");

    GtkWidget* romList = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(romList), GTK_SELECTION_SINGLE);
    gtk_container_add(GTK_CONTAINER(romScrolled), romList);
    gtk_box_pack_start(GTK_BOX(box), romScrolled, TRUE, TRUE, 0);

    auto appendRomRow = [&](const std::string& path) {
        GtkWidget* row = gtk_list_box_row_new();
        GtkWidget* label = gtk_label_new(path.c_str());
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_MIDDLE);
        gtk_widget_set_margin_start(label, 10);
        gtk_widget_set_margin_end(label, 10);
        gtk_widget_set_margin_top(label, 7);
        gtk_widget_set_margin_bottom(label, 7);
        gtk_widget_set_tooltip_text(label, path.c_str());
        g_object_set_data_full(G_OBJECT(row), "rom-path", g_strdup(path.c_str()), g_free);
        gtk_container_add(GTK_CONTAINER(row), label);
        gtk_widget_show_all(row);
        gtk_list_box_insert(GTK_LIST_BOX(romList), row, -1);
    };

    for (const std::string& directory : config_.romDirs) {
        appendRomRow(directory);
    }

    GtkWidget* romButtons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget* addFolder = gtk_button_new_with_label("Añadir carpeta");
    GtkWidget* removeFolder = gtk_button_new_with_label("Quitar seleccionada");
    gtk_box_pack_start(GTK_BOX(romButtons), addFolder, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(romButtons), removeFolder, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), romButtons, FALSE, FALSE, 0);

    g_signal_connect_data(addFolder, "clicked",
        reinterpret_cast<GCallback>(+[](GtkButton*, gpointer data) {
            auto* payload = static_cast<std::pair<GtkWidget*, GtkWidget*>*>(data);
            GtkWidget* parent = std::get<0>(*payload);
            GtkWidget* list = std::get<1>(*payload);
            GtkWidget* chooser = gtk_file_chooser_dialog_new(
                "Añadir carpeta de ROMs", GTK_WINDOW(parent),
                GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                "Cancelar", GTK_RESPONSE_CANCEL, "Añadir", GTK_RESPONSE_ACCEPT, nullptr);
            if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
                gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
                if (filename) {
                    GtkWidget* row = gtk_list_box_row_new();
                    GtkWidget* label = gtk_label_new(filename);
                    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
                    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_MIDDLE);
                    gtk_widget_set_margin_start(label, 10);
                    gtk_widget_set_margin_end(label, 10);
                    gtk_widget_set_margin_top(label, 7);
                    gtk_widget_set_margin_bottom(label, 7);
                    gtk_widget_set_tooltip_text(label, filename);
                    g_object_set_data_full(G_OBJECT(row), "rom-path", g_strdup(filename), g_free);
                    gtk_container_add(GTK_CONTAINER(row), label);
                    gtk_widget_show_all(row);
                    gtk_list_box_insert(GTK_LIST_BOX(list), row, -1);
                    g_free(filename);
                }
            }
            gtk_widget_destroy(chooser);
        }),
        new std::pair<GtkWidget*, GtkWidget*>(dialog, romList),
        +[](gpointer data, GClosure*) {
            delete static_cast<std::pair<GtkWidget*, GtkWidget*>*>(data);
        }, static_cast<GConnectFlags>(0));

    g_signal_connect_data(removeFolder, "clicked",
        reinterpret_cast<GCallback>(+[](GtkButton*, gpointer data) {
            GtkWidget* list = static_cast<GtkWidget*>(data);
            GtkListBoxRow* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(list));
            if (row) {
                gtk_widget_destroy(GTK_WIDGET(row));
            }
        }),
        romList, nullptr, static_cast<GConnectFlags>(0));

    GtkWidget* fullscreen = gtk_check_button_new_with_label(
        "Iniciar los juegos en pantalla completa");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fullscreen), config_.fullscreen);
    gtk_box_pack_start(GTK_BOX(box), fullscreen, FALSE, FALSE, 4);

    GtkWidget* sync = gtk_check_button_new_with_label(
        "Sincronizar carpetas con la configuración de FBNeo");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sync), config_.syncRomPaths);
    gtk_box_pack_start(GTK_BOX(box), sync, FALSE, FALSE, 0);

    GtkWidget* autoDat = gtk_check_button_new_with_label(
        "Generar metadatos DAT automáticamente si faltan");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autoDat), config_.autoDat);
    gtk_box_pack_start(GTK_BOX(box), autoDat, FALSE, FALSE, 0);

    GtkWidget* hint = gtk_label_new(
        "Las ROMs y la BIOS no están incluidas. Usa únicamente copias que poseas legalmente.");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0);
    gtk_label_set_line_wrap(GTK_LABEL(hint), TRUE);
    addClass(hint, "settings-hint");
    gtk_box_pack_start(GTK_BOX(box), hint, FALSE, FALSE, 8);

    gtk_widget_show_all(dialog);
    const gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_OK) {
        config_.emulatorPath = trim(gtk_entry_get_text(GTK_ENTRY(emulatorEntry)));
        config_.datDir = trim(gtk_entry_get_text(GTK_ENTRY(datEntry)));
        config_.artworkDir = trim(gtk_entry_get_text(GTK_ENTRY(artworkEntry)));
        config_.extraArgs = trim(gtk_entry_get_text(GTK_ENTRY(argsEntry)));
        config_.fullscreen = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(fullscreen));
        config_.syncRomPaths = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sync));
        config_.autoDat = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(autoDat));

        config_.romDirs.clear();
        GList* rows = gtk_container_get_children(GTK_CONTAINER(romList));
        for (GList* item = rows; item; item = item->next) {
            const char* path = static_cast<const char*>(
                g_object_get_data(G_OBJECT(item->data), "rom-path"));
            if (path && *path) {
                config_.romDirs.emplace_back(path);
            }
        }
        g_list_free(rows);

        config_.ensureDefaults();
        config_.save();
        if (config_.syncRomPaths) {
            config_.syncRomPathsToFbneoIni();
        }
        gtk_widget_destroy(dialog);
        scanLibrary();
    } else {
        gtk_widget_destroy(dialog);
    }
}

void MainWindow::showAbout() {
    const gchar* authors[] = {"FB Neo Ultra", nullptr};
    GError* error = nullptr;
    GdkPixbuf* logo = gdk_pixbuf_new_from_resource_at_scale(
        "/org/fbneo/ultra/fbneo-ultra.svg", 128, 128, TRUE, &error);
    if (error) {
        g_error_free(error);
    }

    gtk_show_about_dialog(
        GTK_WINDOW(window_),
        "program-name", "FB Neo Ultra",
        "version", "1.56",
        "comments", "Frontend GTK3 para FinalBurn Neo.\n\n"
                    "Biblioteca de ROMs, filtros por sistema, favoritos, previews y "
                    "lanzamiento directo del emulador FBNeo.",
        "logo", logo,
        "authors", authors,
        "title", "Acerca de FB Neo Ultra",
        nullptr);

    if (logo) {
        g_object_unref(logo);
    }
}

void MainWindow::showMessage(const std::string& title, const std::string& message,
                             GtkMessageType type) {
    GtkWidget* dialog = gtk_message_dialog_new(
        GTK_WINDOW(window_), GTK_DIALOG_MODAL, type, GTK_BUTTONS_OK, "%s", title.c_str());
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", message.c_str());
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

gboolean MainWindow::on_filter_visible(GtkTreeModel* model, GtkTreeIter* iter, gpointer data) {
    auto* self = static_cast<MainWindow*>(data);
    gchar* search = nullptr;
    gchar* system = nullptr;
    gboolean favorite = FALSE;
    gtk_tree_model_get(model, iter,
                       COL_SEARCH, &search,
                       COL_SYSTEM, &system,
                       COL_FAVORITE, &favorite,
                       -1);

    bool visible = self->matchesSearch(search ? search : "");
    if (visible) {
        if (self->currentFilter_ == "favorites") {
            visible = favorite;
        } else if (self->currentFilter_.rfind("system:", 0) == 0) {
            visible = system && self->currentFilter_.substr(7) == system;
        }
    }

    g_free(search);
    g_free(system);
    return visible;
}

void MainWindow::on_selection_changed(GtkTreeSelection* selection, gpointer data) {
    (void)selection;
    auto* self = static_cast<MainWindow*>(data);
    self->updatePreview();
}

void MainWindow::on_row_activated(GtkTreeView* view, GtkTreePath* path,
                                  GtkTreeViewColumn* column, gpointer data) {
    (void)view;
    (void)path;
    (void)column;
    auto* self = static_cast<MainWindow*>(data);
    self->launchSelected();
}

void MainWindow::on_search_changed(GtkSearchEntry* entry, gpointer data) {
    auto* self = static_cast<MainWindow*>(data);
    self->searchQuery_ = lower(gtk_entry_get_text(GTK_ENTRY(entry)));
    self->refilter();
}

void MainWindow::on_search_activate(GtkEntry* entry, gpointer data) {
    (void)entry;
    auto* self = static_cast<MainWindow*>(data);
    self->selectFirstVisible();
    gtk_widget_grab_focus(GTK_WIDGET(self->treeView_));
}

void MainWindow::on_sidebar_changed(GtkListBox* box, GtkListBoxRow* row, gpointer data) {
    (void)box;
    auto* self = static_cast<MainWindow*>(data);
    if (self->rebuildingSidebar_ || !row) {
        return;
    }
    const char* key = static_cast<const char*>(g_object_get_data(G_OBJECT(row), "filter-key"));
    if (!key) {
        return;
    }
    if (self->currentFilter_ != key) {
        self->currentFilter_ = key;
        self->refilter();
    }
}

void MainWindow::on_play_clicked(GtkButton* button, gpointer data) {
    (void)button;
    auto* self = static_cast<MainWindow*>(data);
    self->launchSelected();
}

void MainWindow::on_favorite_toggled(GtkToggleButton* button, gpointer data) {
    auto* self = static_cast<MainWindow*>(data);
    if (self->updatingPreview_) {
        return;
    }
    self->toggleFavorite(gtk_toggle_button_get_active(button));
    gtk_button_set_label(GTK_BUTTON(button),
                         gtk_toggle_button_get_active(button) ? "★ Favorito" : "☆ Favorito");
}

void MainWindow::on_refresh_clicked(GtkButton* button, gpointer data) {
    (void)button;
    auto* self = static_cast<MainWindow*>(data);
    self->scanLibrary();
}

void MainWindow::on_menu_refresh(GtkMenuItem* item, gpointer data) {
    (void)item;
    auto* self = static_cast<MainWindow*>(data);
    self->scanLibrary();
}

void MainWindow::on_menu_settings(GtkMenuItem* item, gpointer data) {
    (void)item;
    auto* self = static_cast<MainWindow*>(data);
    self->showSettings();
}

void MainWindow::on_menu_generate(GtkMenuItem* item, gpointer data) {
    (void)item;
    auto* self = static_cast<MainWindow*>(data);
    if (self->generateMetadata(false)) {
        self->scanLibrary();
    }
}

void MainWindow::on_menu_about(GtkMenuItem* item, gpointer data) {
    (void)item;
    auto* self = static_cast<MainWindow*>(data);
    self->showAbout();
}

void MainWindow::on_menu_quit(GtkMenuItem* item, gpointer data) {
    (void)item;
    auto* self = static_cast<MainWindow*>(data);
    g_application_quit(G_APPLICATION(self->application_));
}

gboolean MainWindow::on_key_press(GtkWidget* widget, GdkEventKey* event, gpointer data) {
    (void)widget;
    auto* self = static_cast<MainWindow*>(data);
    const guint key = gdk_keyval_to_lower(event->keyval);

    if ((event->state & GDK_CONTROL_MASK) != 0) {
        if (key == GDK_KEY_r) {
            self->scanLibrary();
            return TRUE;
        }
        if (key == GDK_KEY_f) {
            gtk_widget_grab_focus(self->searchEntry_);
            return TRUE;
        }
        if (key == GDK_KEY_comma) {
            self->showSettings();
            return TRUE;
        }
        if (key == GDK_KEY_q) {
            g_application_quit(G_APPLICATION(self->application_));
            return TRUE;
        }
    }

    if (key == GDK_KEY_F5) {
        self->scanLibrary();
        return TRUE;
    }

    if (key == GDK_KEY_Escape) {
        const char* text = gtk_entry_get_text(GTK_ENTRY(self->searchEntry_));
        if (text && *text) {
            gtk_entry_set_text(GTK_ENTRY(self->searchEntry_), "");
            return TRUE;
        }
    }

    return FALSE;
}

gboolean MainWindow::on_scan_idle(gpointer data) {
    auto* self = static_cast<MainWindow*>(data);
    self->scanLibrary();
    return G_SOURCE_REMOVE;
}

void MainWindow::on_child_exit(GPid pid, gint status, gpointer data) {
    (void)status;
    auto* self = static_cast<MainWindow*>(data);
    g_spawn_close_pid(pid);
    self->busy_ = false;
    gtk_window_deiconify(GTK_WINDOW(self->window_));
    gtk_window_present(GTK_WINDOW(self->window_));
    self->updateStatus();
    if (!self->selectedRom_.empty()) {
        gtk_widget_set_sensitive(self->playButton_, TRUE);
    }
}
