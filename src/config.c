#include "config.h"

gchar *get_config_file_path(void) {
    const gchar *home_dir = g_get_home_dir();
    if (home_dir) {
        return g_build_filename(home_dir, CONFIG_FILE_NAME, NULL);
    }
    return g_strdup(CONFIG_FILE_NAME);
}

static void load_keyfile_from_disk(GKeyFile *key_file, const gchar *config_path) {
    if (!g_file_test(config_path, G_FILE_TEST_EXISTS)) {
        return;
    }

    GError *load_error = NULL;
    if (!g_key_file_load_from_file(key_file, config_path, G_KEY_FILE_NONE, &load_error)) {
        g_warning("Failed to load config: %s", load_error ? load_error->message : "Unknown error");
        g_clear_error(&load_error);
    }
}

static void write_app_data_to_keyfile(GKeyFile *key_file, AppData *data) {
    if (data->location_lat && data->location_lon) {
        g_key_file_set_string(key_file, "Location", "latitude", data->location_lat);
        g_key_file_set_string(key_file, "Location", "longitude", data->location_lon);
        if (data->timezone) {
            g_key_file_set_string(key_file, "Location", "timezone", data->timezone);
        }
        g_key_file_set_integer(key_file, "Location", "utc_offset_seconds", data->utc_offset_seconds);
    }

    g_key_file_set_string(key_file, "General", "language", app_language_to_string(data->language));
    g_key_file_set_string(key_file, "General", "forecast_mode",
                          forecast_mode_to_string(data->forecast_mode));

    g_key_file_set_string(key_file, "Theme", "mode", theme_mode_to_string(data->theme_mode));
    g_key_file_set_boolean(key_file, "Theme", "schedule_enabled", data->theme_schedule_enabled);
    gchar *light_time = g_strdup_printf("%02d:%02d", data->theme_light_hour, data->theme_light_minute);
    g_key_file_set_string(key_file, "Theme", "light_time", light_time);
    g_free(light_time);
    gchar *dark_time = g_strdup_printf("%02d:%02d", data->theme_dark_hour, data->theme_dark_minute);
    g_key_file_set_string(key_file, "Theme", "dark_time", dark_time);
    g_free(dark_time);
}

static void save_app_config(AppData *data) {
    if (!data) {
        return;
    }

    gchar *config_path = get_config_file_path();
    if (!config_path) {
        return;
    }

    GKeyFile *key_file = g_key_file_new();
    load_keyfile_from_disk(key_file, config_path);
    write_app_data_to_keyfile(key_file, data);

    GError *error = NULL;
    if (!g_key_file_save_to_file(key_file, config_path, &error)) {
        g_warning("Failed to save config: %s", error ? error->message : "Unknown error");
        g_clear_error(&error);
    }

    g_key_file_unref(key_file);
    g_free(config_path);
}

void save_location_to_config(AppData *data) {
    if (!data || !data->location_lat || !data->location_lon) {
        return;
    }

    save_app_config(data);
}

void save_language_to_config(AppData *data) {
    if (!data) {
        return;
    }

    save_app_config(data);
}

void load_location_from_config(AppData *data) {
    if (!data) {
        return;
    }

    gchar *config_path = get_config_file_path();
    if (!config_path) {
        return;
    }

    if (!g_file_test(config_path, G_FILE_TEST_EXISTS)) {
        g_free(config_path);
        return;
    }

    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;

    if (!g_key_file_load_from_file(key_file, config_path, G_KEY_FILE_NONE, &error)) {
        g_warning("Failed to load config: %s", error ? error->message : "Unknown error");
        g_clear_error(&error);
        g_key_file_unref(key_file);
        g_free(config_path);
        return;
    }

    gchar *lat = g_key_file_get_string(key_file, "Location", "latitude", &error);
    if (lat && strlen(lat) > 0) {
        g_free(data->location_lat);
        data->location_lat = lat;
    } else {
        g_free(lat);
    }

    g_clear_error(&error);

    gchar *lon = g_key_file_get_string(key_file, "Location", "longitude", &error);
    if (lon && strlen(lon) > 0) {
        g_free(data->location_lon);
        data->location_lon = lon;
    } else {
        g_free(lon);
    }

    g_clear_error(&error);

    gchar *tz = g_key_file_get_string(key_file, "Location", "timezone", &error);
    if (tz && strlen(tz) > 0) {
        if (data->tz) {
            g_time_zone_unref(data->tz);
        }
        g_free(data->timezone);
        data->timezone = tz;
        data->tz = g_time_zone_new_identifier(tz);
        if (!data->tz) {
            G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            data->tz = g_time_zone_new(tz);
            G_GNUC_END_IGNORE_DEPRECATIONS
            if (!data->tz) {
                g_debug("Failed to create timezone from config: %s", tz);
            }
        }
    } else {
        g_free(tz);
    }

    g_clear_error(&error);

    gint utc_offset = g_key_file_get_integer(key_file, "Location", "utc_offset_seconds", &error);
    if (!error) {
        data->utc_offset_seconds = utc_offset;
        g_debug("Loaded UTC offset from config: %d seconds", utc_offset);
    }
    g_clear_error(&error);

    gchar *lang = g_key_file_get_string(key_file, "General", "language", &error);
    if (lang) {
        data->language = app_language_from_string(lang);
        g_free(lang);
    }
    g_clear_error(&error);

    g_key_file_unref(key_file);
    g_free(config_path);
}

void load_language_from_config(AppData *data) {
    /* Language is loaded together with location in load_location_from_config. */
    (void)data;
}

void save_forecast_mode_to_config(AppData *data) {
    if (!data) {
        return;
    }
    save_app_config(data);
}

ForecastMode forecast_mode_from_string(const char *s) {
    if (s && g_strcmp0(s, "daily") == 0) {
        return FORECAST_MODE_DAILY;
    }
    return FORECAST_MODE_HOURLY;
}

const char *forecast_mode_to_string(ForecastMode mode) {
    if (mode == FORECAST_MODE_DAILY) {
        return "daily";
    }
    return "hourly";
}

void save_theme_to_config(AppData *data) {
    if (!data) {
        return;
    }
    save_app_config(data);
}

void load_theme_from_config(AppData *data) {
    if (!data) {
        return;
    }

    /* Defaults: dark mode, schedule off, 07:00 light -> 19:00 dark. */
    data->theme_mode = THEME_MODE_DARK;
    data->theme_schedule_enabled = FALSE;
    data->theme_light_hour = 7;
    data->theme_light_minute = 0;
    data->theme_dark_hour = 19;
    data->theme_dark_minute = 0;

    gchar *config_path = get_config_file_path();
    if (!config_path) {
        return;
    }

    if (!g_file_test(config_path, G_FILE_TEST_EXISTS)) {
        g_free(config_path);
        return;
    }

    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;

    if (!g_key_file_load_from_file(key_file, config_path, G_KEY_FILE_NONE, &error)) {
        g_warning("Failed to load config: %s", error ? error->message : "Unknown error");
        g_clear_error(&error);
        g_key_file_unref(key_file);
        g_free(config_path);
        return;
    }

    gchar *mode = g_key_file_get_string(key_file, "Theme", "mode", NULL);
    if (mode) {
        data->theme_mode = theme_mode_from_string(mode);
        g_free(mode);
    }

    data->theme_schedule_enabled =
        g_key_file_get_boolean(key_file, "Theme", "schedule_enabled", NULL);
    g_clear_error(&error);

    gchar *light_time = g_key_file_get_string(key_file, "Theme", "light_time", NULL);
    if (light_time) {
        gint h = -1, m = -1;
        if (sscanf(light_time, "%d:%d", &h, &m) == 2 && h >= 0 && h <= 23 && m >= 0 && m <= 59) {
            data->theme_light_hour = h;
            data->theme_light_minute = m;
        }
        g_free(light_time);
    }

    gchar *dark_time = g_key_file_get_string(key_file, "Theme", "dark_time", NULL);
    if (dark_time) {
        gint h = -1, m = -1;
        if (sscanf(dark_time, "%d:%d", &h, &m) == 2 && h >= 0 && h <= 23 && m >= 0 && m <= 59) {
            data->theme_dark_hour = h;
            data->theme_dark_minute = m;
        }
        g_free(dark_time);
    }

    g_key_file_unref(key_file);
    g_free(config_path);
}

ThemeMode theme_mode_from_string(const char *s) {
    if (s && g_strcmp0(s, "light") == 0) {
        return THEME_MODE_LIGHT;
    }
    return THEME_MODE_DARK;
}

const char *theme_mode_to_string(ThemeMode mode) {
    if (mode == THEME_MODE_LIGHT) {
        return "light";
    }
    return "dark";
}

void update_location_from_entries(AppData *data) {
    if (!data) {
        return;
    }

    if (!data->lat_entry || !data->lon_entry) {
        return;
    }

    if (!GTK_IS_EDITABLE(data->lat_entry) || !GTK_IS_EDITABLE(data->lon_entry)) {
        return;
    }

    const gchar *lat_text = gtk_editable_get_text(GTK_EDITABLE(data->lat_entry));
    const gchar *lon_text = gtk_editable_get_text(GTK_EDITABLE(data->lon_entry));

    if (lat_text && strlen(lat_text) > 0) {
        gchar *new_lat = g_strdup(lat_text);
        if (new_lat) {
            g_free(data->location_lat);
            data->location_lat = new_lat;
        }
    }
    if (lon_text && strlen(lon_text) > 0) {
        gchar *new_lon = g_strdup(lon_text);
        if (new_lon) {
            g_free(data->location_lon);
            data->location_lon = new_lon;
        }
    }

    save_location_to_config(data);
}
