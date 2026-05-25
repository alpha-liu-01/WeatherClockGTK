#include "ui.h"
#include "style.h"
#include "clock.h"
#include "weather.h"
#include "config.h"

#define WEATHER_BOX_SPACING 8
#define WEATHER_SECTION_PADDING 24
#define WEATHER_COMPACT_THRESHOLD 130
#define WEATHER_ULTRA_COMPACT_THRESHOLD 95

static void set_weather_container_class(AppData *data, const char *class_name, gboolean enable) {
    if (!data || !data->weather_box) {
        return;
    }

    gboolean has_class = gtk_widget_has_css_class(data->weather_box, class_name);
    if (enable && !has_class) {
        gtk_widget_add_css_class(data->weather_box, class_name);
    } else if (!enable && has_class) {
        gtk_widget_remove_css_class(data->weather_box, class_name);
    }
}

static int weather_row_width(AppData *data) {
    int box_width = gtk_widget_get_width(data->weather_box);
    if (box_width > 0) {
        return box_width;
    }

    int scrolled_width = gtk_widget_get_width(data->weather_scrolled);
    if (scrolled_width <= 0) {
        return 0;
    }

    int usable = scrolled_width - WEATHER_SECTION_PADDING;
    return usable > 0 ? usable : scrolled_width;
}

void weather_layout_update(AppData *data) {
    if (!data || !data->weather_scrolled || !data->weather_box) {
        return;
    }

    int row_width = weather_row_width(data);
    if (row_width <= 0) {
        return;
    }

    int per_column = (row_width - WEATHER_BOX_SPACING * (WEATHER_HOUR_COUNT - 1)) / WEATHER_HOUR_COUNT;
    if (per_column < 0) {
        per_column = 0;
    }

    gboolean want_compact = per_column < WEATHER_COMPACT_THRESHOLD;
    gboolean want_ultra = per_column < WEATHER_ULTRA_COMPACT_THRESHOLD;

    gboolean had_compact = gtk_widget_has_css_class(data->weather_box, "weather-compact");
    gboolean had_ultra = gtk_widget_has_css_class(data->weather_box, "weather-ultra-compact");

    set_weather_container_class(data, "weather-compact", want_compact);
    set_weather_container_class(data, "weather-ultra-compact", want_ultra);

    if (had_compact != want_compact || had_ultra != want_ultra) {
        gtk_widget_queue_resize(data->weather_box);
    }
}

static gboolean weather_layout_update_idle(gpointer user_data) {
    weather_layout_update((AppData *)user_data);
    return G_SOURCE_REMOVE;
}

void weather_layout_update_later(AppData *data) {
    if (!data) {
        return;
    }
    g_idle_add(weather_layout_update_idle, data);
}

static void on_weather_scrolled_notify_width(GObject *object, GParamSpec *pspec, gpointer user_data) {
    (void)object;
    (void)pspec;
    g_idle_add(weather_layout_update_idle, user_data);
}

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

static void on_language_changed(GObject *object, GParamSpec *pspec, gpointer user_data) {
    (void)object;
    (void)pspec;
    AppData *data = (AppData *)user_data;
    if (!data || !data->language_dropdown) {
        return;
    }

    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(data->language_dropdown));
    AppLanguage new_lang = (selected == 1) ? APP_LANG_ZH_CN : APP_LANG_EN;
    if (new_lang == data->language) {
        return;
    }

    data->language = new_lang;
    save_language_to_config(data);
    ui_refresh_translations(data);
    update_clock(data);
    fetch_weather(data);
}

void ui_refresh_translations(AppData *data) {
    if (!data) {
        return;
    }

    if (data->window) {
        gtk_window_set_title(GTK_WINDOW(data->window), i18n_(data, I18N_MAIN_TITLE));
    }
    if (data->settings_window) {
        gtk_window_set_title(GTK_WINDOW(data->settings_window), i18n_(data, I18N_SETTINGS_TITLE));
    }
    if (data->location_title_label) {
        gtk_label_set_text(GTK_LABEL(data->location_title_label), i18n_(data, I18N_LOCATION_SETTINGS));
    }
    if (data->lang_label) {
        gtk_label_set_text(GTK_LABEL(data->lang_label), i18n_(data, I18N_LANGUAGE));
    }
    if (data->lat_label) {
        gtk_label_set_text(GTK_LABEL(data->lat_label), i18n_(data, I18N_LATITUDE));
    }
    if (data->lon_label) {
        gtk_label_set_text(GTK_LABEL(data->lon_label), i18n_(data, I18N_LONGITUDE));
    }
    if (data->update_location_btn) {
        gtk_button_set_label(GTK_BUTTON(data->update_location_btn), i18n_(data, I18N_UPDATE_LOCATION));
    }
    if (data->settings_close_btn) {
        gtk_button_set_label(GTK_BUTTON(data->settings_close_btn), i18n_(data, I18N_CLOSE));
    }
    if (data->settings_btn) {
        gtk_button_set_label(GTK_BUTTON(data->settings_btn), i18n_(data, I18N_SETTINGS_BTN));
    }
    if (data->exit_btn) {
        gtk_button_set_label(GTK_BUTTON(data->exit_btn), i18n_(data, I18N_EXIT));
    }
    if (data->weather_title_label) {
        gtk_label_set_text(GTK_LABEL(data->weather_title_label), i18n_(data, I18N_WEATHER_FORECAST_TITLE));
    }

    if (data->language_dropdown) {
        g_signal_handlers_block_by_func(data->language_dropdown,
                                        G_CALLBACK(on_language_changed), data);
        GtkStringList *list = GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(data->language_dropdown)));
        if (list) {
            gtk_string_list_splice(list, 0, 2,
                                   (const char *[]) {
                                       i18n_(data, I18N_LANG_OPTION_EN),
                                       i18n_(data, I18N_LANG_OPTION_ZH),
                                       NULL
                                   });
        }
        g_signal_handlers_unblock_by_func(data->language_dropdown,
                                          G_CALLBACK(on_language_changed), data);
    }
}

static void create_settings_window(AppData *data) {
    if (!data) {
        return;
    }

    data->settings_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(data->settings_window), i18n_(data, I18N_SETTINGS_TITLE));
    gtk_window_set_default_size(GTK_WINDOW(data->settings_window), 450, 240);
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

    data->location_title_label = gtk_label_new(i18n_(data, I18N_LOCATION_SETTINGS));
    gtk_widget_add_css_class(data->location_title_label, "settings-title");
    gtk_widget_set_halign(data->location_title_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_box), data->location_title_label);

    GtkWidget *language_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(language_box, GTK_ALIGN_START);

    data->lang_label = gtk_label_new(i18n_(data, I18N_LANGUAGE));
    GtkStringList *lang_list = gtk_string_list_new((const char *[]) {
        i18n_(data, I18N_LANG_OPTION_EN),
        i18n_(data, I18N_LANG_OPTION_ZH),
        NULL
    });
    data->language_dropdown = gtk_drop_down_new(G_LIST_MODEL(lang_list), NULL);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(data->language_dropdown),
                               data->language == APP_LANG_ZH_CN ? 1 : 0);
    g_signal_connect(data->language_dropdown, "notify::selected",
                     G_CALLBACK(on_language_changed), data);

    gtk_box_append(GTK_BOX(language_box), data->lang_label);
    gtk_box_append(GTK_BOX(language_box), data->language_dropdown);
    gtk_box_append(GTK_BOX(main_box), language_box);

    GtkWidget *location_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(location_box, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(location_box, "location-box");

    data->lat_label = gtk_label_new(i18n_(data, I18N_LATITUDE));
    data->lat_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(data->lat_entry), "52.52");
    gtk_editable_set_text(GTK_EDITABLE(data->lat_entry), data->location_lat ? data->location_lat : "");

    data->lon_label = gtk_label_new(i18n_(data, I18N_LONGITUDE));
    data->lon_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(data->lon_entry), "13.41");
    gtk_editable_set_text(GTK_EDITABLE(data->lon_entry), data->location_lon ? data->location_lon : "");

    data->update_location_btn = gtk_button_new_with_label(i18n_(data, I18N_UPDATE_LOCATION));
    g_signal_connect(data->update_location_btn, "clicked", G_CALLBACK(on_location_update), data);

    gtk_box_append(GTK_BOX(location_box), data->lat_label);
    gtk_box_append(GTK_BOX(location_box), data->lat_entry);
    gtk_box_append(GTK_BOX(location_box), data->lon_label);
    gtk_box_append(GTK_BOX(location_box), data->lon_entry);
    gtk_box_append(GTK_BOX(location_box), data->update_location_btn);

    gtk_box_append(GTK_BOX(main_box), location_box);

    data->settings_close_btn = gtk_button_new_with_label(i18n_(data, I18N_CLOSE));
    gtk_widget_set_halign(data->settings_close_btn, GTK_ALIGN_END);
    gtk_widget_add_css_class(data->settings_close_btn, "exit-button");
    g_signal_connect(data->settings_close_btn, "clicked", G_CALLBACK(on_settings_toggle), data);
    gtk_box_append(GTK_BOX(main_box), data->settings_close_btn);

    gtk_widget_set_visible(data->settings_window, FALSE);
}

static void on_window_realize_fullscreen(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    AppData *data = (AppData *)user_data;
    if (data && data->window && gtk_widget_get_realized(data->window)) {
        gtk_window_fullscreen(GTK_WINDOW(data->window));
        update_clock(data);
        weather_layout_update_later(data);
        gtk_widget_queue_draw(data->window);
    }
}

void activate(GtkApplication *app, gpointer user_data) {
    AppData *data = (AppData *)user_data;

    data->window = gtk_application_window_new(app);
    gtk_window_set_icon_name(GTK_WINDOW(data->window), "com.weatherclock.app");
    gtk_window_set_title(GTK_WINDOW(data->window), i18n_(data, I18N_MAIN_TITLE));
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

    data->settings_btn = gtk_button_new_with_label(i18n_(data, I18N_SETTINGS_BTN));
    gtk_widget_add_css_class(data->settings_btn, "exit-button");
    g_signal_connect(data->settings_btn, "clicked", G_CALLBACK(on_settings_toggle), data);
    gtk_box_append(GTK_BOX(button_box), data->settings_btn);

    data->exit_btn = gtk_button_new_with_label(i18n_(data, I18N_EXIT));
    gtk_widget_add_css_class(data->exit_btn, "exit-button");
    g_signal_connect(data->exit_btn, "clicked", G_CALLBACK(on_exit_clicked), app);
    gtk_box_append(GTK_BOX(button_box), data->exit_btn);

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

    data->date_label = gtk_label_new(i18n_(data, I18N_DATE_PLACEHOLDER));
    gtk_widget_add_css_class(data->date_label, "clock-date");
    gtk_box_append(GTK_BOX(clock_box), data->date_label);

    gtk_box_append(GTK_BOX(main_box), clock_box);

    /* Weather section */
    GtkWidget *weather_section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(weather_section, "weather-section");
    gtk_widget_set_vexpand(weather_section, FALSE);

    data->weather_title_label = gtk_label_new(i18n_(data, I18N_WEATHER_FORECAST_TITLE));
    gtk_widget_add_css_class(data->weather_title_label, "weather-title");
    gtk_box_append(GTK_BOX(weather_section), data->weather_title_label);

    data->weather_scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(data->weather_scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(data->weather_scrolled),
                                                    FALSE);
    gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(data->weather_scrolled),
                                                     FALSE);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(data->weather_scrolled), 160);
    gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(data->weather_scrolled), 200);
    gtk_widget_set_hexpand(data->weather_scrolled, TRUE);
    gtk_widget_set_vexpand(data->weather_scrolled, FALSE);

    data->weather_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, WEATHER_BOX_SPACING);
    gtk_widget_add_css_class(data->weather_box, "weather-container");
    gtk_widget_set_halign(data->weather_box, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(data->weather_box, TRUE);
    gtk_box_set_homogeneous(GTK_BOX(data->weather_box), TRUE);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(data->weather_scrolled), data->weather_box);

    g_signal_connect(data->weather_scrolled, "notify::width",
                     G_CALLBACK(on_weather_scrolled_notify_width), data);

    gtk_box_append(GTK_BOX(weather_section), data->weather_scrolled);
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

    /* Hourly refresh at the next clock hour; that path uses WEATHER_HOURLY_REFRESH_DELAY_SECONDS. */
    guint seconds_until_hour = seconds_until_next_hour();
    data->weather_timer_id = g_timeout_add_seconds(seconds_until_hour, update_weather_callback, data);

    fetch_weather(data);

    g_signal_connect(data->window, "realize", G_CALLBACK(on_window_realize_fullscreen), data);

    gtk_widget_set_visible(data->window, TRUE);
    g_idle_add(weather_layout_update_idle, data);
}
