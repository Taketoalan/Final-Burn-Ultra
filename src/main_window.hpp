#pragma once

#include "config.hpp"
#include "database.hpp"
#include "scanner.hpp"

#include <gtk/gtk.h>

#include <string>
#include <unordered_map>
#include <vector>

class MainWindow {
public:
    explicit MainWindow(GtkApplication* application);
    ~MainWindow();

    GtkWidget* widget() const { return window_; }

private:
    enum Column {
        COL_STAR,
        COL_TITLE,
        COL_SYSTEM,
        COL_YEAR,
        COL_MANUFACTURER,
        COL_ROM,
        COL_PATH,
        COL_FAVORITE,
        COL_SEARCH,
        COL_COUNT,
    };

    void buildUi();
    GtkWidget* buildHeader();
    GtkWidget* buildSidebar();
    GtkWidget* buildSearchBar();
    GtkWidget* buildListArea();
    GtkWidget* buildPreviewPanel();
    GtkWidget* buildStatusBar();

    void scanLibrary();
    void ensureDatabase();
    bool generateMetadata(bool quiet = false);
    void populateModel();
    void rebuildSidebar();
    void refilter();
    void selectFirstVisible();
    void selectRom(const std::string& rom);
    void updatePreview();
    void clearPreview();
    void updateStatus();
    void showSettings();
    void showAbout();
    void showMessage(const std::string& title, const std::string& message,
                     GtkMessageType type = GTK_MESSAGE_ERROR);
    void launchSelected();
    void launchGame(const Game& game);
    void toggleFavorite(bool value);
    void updateFilterCount();

    Game* findGame(const std::string& rom);
    const Game* findGame(const std::string& rom) const;
    bool matchesSearch(const std::string& search) const;
    GdkPixbuf* artworkFor(const std::string& rom) const;
    GdkPixbuf* makePlaceholder(const std::string& title, const std::string& subtitle) const;

    static gboolean on_filter_visible(GtkTreeModel* model, GtkTreeIter* iter, gpointer data);
    static void on_selection_changed(GtkTreeSelection* selection, gpointer data);
    static void on_row_activated(GtkTreeView* view, GtkTreePath* path,
                                 GtkTreeViewColumn* column, gpointer data);
    static void on_search_changed(GtkSearchEntry* entry, gpointer data);
    static void on_search_activate(GtkEntry* entry, gpointer data);
    static void on_sidebar_changed(GtkListBox* box, GtkListBoxRow* row, gpointer data);
    static void on_play_clicked(GtkButton* button, gpointer data);
    static void on_favorite_toggled(GtkToggleButton* button, gpointer data);
    static void on_refresh_clicked(GtkButton* button, gpointer data);
    static void on_menu_refresh(GtkMenuItem* item, gpointer data);
    static void on_menu_settings(GtkMenuItem* item, gpointer data);
    static void on_menu_generate(GtkMenuItem* item, gpointer data);
    static void on_menu_about(GtkMenuItem* item, gpointer data);
    static void on_menu_quit(GtkMenuItem* item, gpointer data);
    static gboolean on_key_press(GtkWidget* widget, GdkEventKey* event, gpointer data);
    static gboolean on_scan_idle(gpointer data);
    static void on_child_exit(GPid pid, gint status, gpointer data);

    GtkApplication* application_ = nullptr;
    GtkWidget* window_ = nullptr;
    GtkWidget* searchEntry_ = nullptr;
    GtkWidget* sidebarList_ = nullptr;
    GtkWidget* stack_ = nullptr;
    GtkListStore* store_ = nullptr;
    GtkTreeModel* filterModel_ = nullptr;
    GtkTreeModel* sortModel_ = nullptr;
    GtkTreeView* treeView_ = nullptr;
    GtkWidget* previewImage_ = nullptr;
    GtkWidget* titleLabel_ = nullptr;
    GtkWidget* systemBadge_ = nullptr;
    GtkWidget* yearBadge_ = nullptr;
    GtkWidget* manufacturerLabel_ = nullptr;
    GtkWidget* pathLabel_ = nullptr;
    GtkWidget* statusLabel_ = nullptr;
    GtkWidget* countLabel_ = nullptr;
    GtkWidget* emulatorLabel_ = nullptr;
    GtkWidget* playButton_ = nullptr;
    GtkToggleButton* favoriteButton_ = nullptr;

    Config config_;
    Database database_;
    Scanner scanner_;
    std::vector<Game> games_;
    std::unordered_map<std::string, size_t> gameIndex_;
    std::vector<std::string> currentSystems_;
    std::string currentFilter_ = "all";
    std::string searchQuery_;
    std::string selectedRom_;
    size_t metadataCount_ = 0;
    bool updatingPreview_ = false;
    bool rebuildingSidebar_ = false;
    bool busy_ = false;
};
