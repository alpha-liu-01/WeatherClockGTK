#include "ui.h"
#include "style.h"
#include "clock.h"
#include "weather.h"
#include "config.h"

static void on_location_update(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    AppData *data = (AppData *)user_data;
    if (!data) {
        return;
    }

    data->retry_count = 0;
    data->retry_delay = 0;
    data->is_retrying = FALSE;
    if (data->retry_timer_id != 0) {
        g_source_remove(data->retry_timer_id);
        data->retry_timer_id = 0;
    }

    update_location_from_entries(data);
    fetch_weather(data);
}

static void on_exit_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    GtkApplication *app = (GtkApplication *)user_data;
    g_application_quit(G_APPLICATION(app));
}

static gboolean on_settings_window_close_request(GtkWindow *window, gpointer user_data) {
    (void)window;
    AppData *data = (AppData *)user_data;
    if (data && data->settings_window) {
        gtk_widget_set_visible(data->settings_window, FALSE);
    }
    return TRUE;
}

static void on_settings_toggle(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    AppData *data = (AppData *)user_data;
    if (!data || !data->settings_window) {
        return;
    }

    if (gtk_widget_get_visible(data->settings_window)) {
        gtk_widget_set_visible(data->settings_window, FALSE);
    } else {
        gtk_widget_set_visible(data->settings_window, TRUE);
        gtk_window_present(GTK_WINDOW(data->settings_window));
    }
}

static void create_settings_window(AppData *data) {
    if (!data) {
        return;
    }

    data->settings_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(data->settings_window), "Settings - Weather Clock");
    gtk_window_set_default_size(GTK_WINDOW(data->settings_window), 450, 160);
    gtk_window_set_resizable(GTK_WINDOW(data->settings_window), TRUE);
    gtk_window_set_modal(GTK_WINDOW(data->settings_window), FALSE);
    gtk_widget_set_name(data->settings_window, "settings-window");

    g_signal_connect(data->settings_window, "close-request",
                     G_CALLBACK(on_settings_window_close_request), data);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(main_box, 15);
    gtk_widget_set_margin_bottom(main_box, 15);
    gtk_widget_set_margin_start(main_box, 15);
    gtk_widget_set_margin_end(main_box, 15);
    gtk_window_set_child(GTK_WINDOW(data->settings_window), main_box);

    GtkWidget *title_label = gtk_label_new("Location Settings");
    gtk_widget_add_css_class(title_label, "settings-title");
    gtk_widget_set_halign(title_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_box), title_label);

    GtkWidget *location_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(location_box, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(location_box, "location-box");

    GtkWidget *lat_label = gtk_label_new("Latitude:");
    data->lat_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(data->lat_entry), "52.52");
    gtk_editable_set_text(GTK_EDITABLE(data->lat_entry), data->location_lat ? data->location_lat : "");

    GtkWidget *lon_label = gtk_label_new("Longitude:");
    data->lon_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(data->lon_entry), "13.41");
    gtk_editable_set_text(GTK_EDITABLE(data->lon_entry), data->location_lon ? data->location_lon : "");

    GtkWidget *update_btn = gtk_button_new_with_label("Update Location");
    g_signal_connect(update_btn, "clicked", G_CALLBACK(on_location_update), data);

    gtk_box_append(GTK_BOX(location_box), lat_label);
    gtk_box_append(GTK_BOX(location_box), data->lat_entry);
    gtk_box_append(GTK_BOX(location_box), lon_label);
    gtk_box_append(GTK_BOX(location_box), data->lon_entry);
    gtk_box_append(GTK_BOX(location_box), update_btn);

    gtk_box_append(GTK_BOX(main_box), location_box);

    GtkWidget *close_btn = gtk_button_new_with_label("Close");
    gtk_widget_set_halign(close_btn, GTK_ALIGN_END);
    gtk_widget_add_css_class(close_btn, "exit-button");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_settings_toggle), data);
    gtk_box_append(GTK_BOX(main_box), close_btn);

    gtk_widget_set_visible(data->settings_window, FALSE);
}

static void on_window_realize_fullscreen(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    AppData *data = (AppData *)user_data;
    if (data && data->window && gtk_widget_get_realized(data->window)) {
        gtk_window_fullscreen(GTK_WINDOW(data->window));
        update_clock(data);
        gtk_widget_queue_draw(data->window);
    }
}

void activate(GtkApplication *app, gpointer user_data) {
    AppData *data = (AppData *)user_data;

    data->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(data->window), "Weather Clock");
    gtk_window_set_resizable(GTK_WINDOW(data->window), TRUE);
    gtk_widget_set_name(data->window, "main-window");

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    gtk_widget_set_margin_start(main_box, 20);
    gtk_widget_set_margin_end(main_box, 20);
    gtk_window_set_child(GTK_WINDOW(data->window), main_box);

    gtk_widget_set_hexpand(main_box, TRUE);
    gtk_widget_set_vexpand(main_box, TRUE);

    /* Button box at the top (Settings and Exit) */
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(button_box, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(button_box, 5);

    GtkWidget *settings_btn = gtk_button_new_with_label("Settings");
    gtk_widget_add_css_class(settings_btn, "exit-button");
    g_signal_connect(settings_btn, "clicked", G_CALLBACK(on_settings_toggle), data);
    gtk_box_append(GTK_BOX(button_box), settings_btn);

    GtkWidget *exit_btn = gtk_button_new_with_label("Exit");
    gtk_widget_add_css_class(exit_btn, "exit-button");
    g_signal_connect(exit_btn, "clicked", G_CALLBACK(on_exit_clicked), app);
    gtk_box_append(GTK_BOX(button_box), exit_btn);

    gtk_box_append(GTK_BOX(main_box), button_box);

    create_settings_window(data);

    /* Clock section */
    GtkWidget *clock_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(clock_box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(clock_box, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(clock_box, TRUE);

    data->clock_label = gtk_label_new("00:00:00");
    gtk_widget_add_css_class(data->clock_label, "clock-time");
    gtk_label_set_selectable(GTK_LABEL(data->clock_label), FALSE);
    gtk_box_append(GTK_BOX(clock_box), data->clock_label);

    data->date_label = gtk_label_new("Monday, January 1, 2024");
    gtk_widget_add_css_class(data->date_label, "clock-date");
    gtk_box_append(GTK_BOX(clock_box), data->date_label);

    gtk_box_append(GTK_BOX(main_box), clock_box);

    /* Weather section */
    GtkWidget *weather_section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(weather_section, "weather-section");

    GtkWidget *weather_title = gtk_label_new("Hourly Weather Forecast");
    gtk_widget_add_css_class(weather_title, "weather-title");
    gtk_box_append(GTK_BOX(weather_section), weather_title);

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);

    data->weather_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(data->weather_box, "weather-container");
    gtk_widget_set_halign(data->weather_box, GTK_ALIGN_CENTER);
    gtk_box_set_homogeneous(GTK_BOX(data->weather_box), TRUE);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), data->weather_box);

    gtk_box_append(GTK_BOX(weather_section), scrolled);
    gtk_box_append(GTK_BOX(main_box), weather_section);

    /* Load CSS */
    data->css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(data->css_provider, APP_CSS);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(data->css_provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    /* Initialize clock and timers */
    update_clock(data);

    data->clock_timer_id = g_timeout_add_seconds(1, update_clock_callback, data);

    guint seconds_until_hour = seconds_until_next_hour();
    data->weather_timer_id = g_timeout_add_seconds(seconds_until_hour, update_weather_callback, data);

    fetch_weather(data);

    g_signal_connect(data->window, "realize", G_CALLBACK(on_window_realize_fullscreen), data);

    gtk_widget_set_visible(data->window, TRUE);
}
