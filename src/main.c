#include "app.h"
#include "config.h"
#include "ui.h"

int main(int argc, char *argv[]) {
    AppData *data = g_new0(AppData, 1);
    if (!data) {
        g_error("Failed to allocate AppData");
        return 1;
    }

    data->language = APP_LANG_EN;
    data->location_lat = g_strdup("43.640");
    data->location_lon = g_strdup("-79.565");

    if (!data->location_lat || !data->location_lon) {
        g_error("Failed to allocate location strings");
        g_free(data->location_lat);
        g_free(data->location_lon);
        g_free(data);
        return 1;
    }

    load_location_from_config(data);
    load_language_from_config(data);

    if (argc >= 3) {
        g_free(data->location_lat);
        g_free(data->location_lon);
        data->location_lat = g_strdup(argv[1]);
        data->location_lon = g_strdup(argv[2]);

        if (!data->location_lat || !data->location_lon) {
            g_error("Failed to allocate location strings from arguments");
            g_free(data->location_lat);
            g_free(data->location_lon);
            g_free(data);
            return 1;
        }

        save_location_to_config(data);
    }

    data->session = soup_session_new();
    if (!data->session) {
        g_error("Failed to create SoupSession");
        g_free(data->location_lat);
        g_free(data->location_lon);
        g_free(data);
        return 1;
    }

    GtkApplication *app = gtk_application_new("com.weatherclock.app", G_APPLICATION_DEFAULT_FLAGS);
    if (!app) {
        g_error("Failed to create GtkApplication");
        g_object_unref(data->session);
        g_free(data->location_lat);
        g_free(data->location_lon);
        g_free(data);
        return 1;
    }

    g_signal_connect(app, "activate", G_CALLBACK(activate), data);

    int status = g_application_run(G_APPLICATION(app), argc, argv);

    /* Cleanup */
    if (data->pending_message) {
        g_object_unref(data->pending_message);
        data->pending_message = NULL;
    }

    if (data->clock_timer_id != 0) {
        g_source_remove(data->clock_timer_id);
        data->clock_timer_id = 0;
    }
    if (data->weather_timer_id != 0) {
        g_source_remove(data->weather_timer_id);
        data->weather_timer_id = 0;
    }
    if (data->retry_timer_id != 0) {
        g_source_remove(data->retry_timer_id);
        data->retry_timer_id = 0;
    }

    if (data->css_provider) {
        GdkDisplay *display = gdk_display_get_default();
        if (display) {
            gtk_style_context_remove_provider_for_display(display,
                                                          GTK_STYLE_PROVIDER(data->css_provider));
        }
        g_object_unref(data->css_provider);
        data->css_provider = NULL;
    }

    if (data->settings_window && GTK_IS_WINDOW(data->settings_window)) {
        gtk_window_destroy(GTK_WINDOW(data->settings_window));
        data->settings_window = NULL;
    }
    data->window = NULL;

    data->clock_label = NULL;
    data->date_label = NULL;
    data->weather_box = NULL;
    data->lat_entry = NULL;
    data->lon_entry = NULL;

    g_object_unref(app);
    if (data->session) {
        g_object_unref(data->session);
        data->session = NULL;
    }
    if (data->tz) {
        g_time_zone_unref(data->tz);
        data->tz = NULL;
    }
    g_free(data->location_lat);
    data->location_lat = NULL;
    g_free(data->location_lon);
    data->location_lon = NULL;
    g_free(data->timezone);
    data->timezone = NULL;
    g_free(data);

    return status;
}
