#include "weather.h"
#include "clock.h"
#include "config.h"
#include "i18n.h"
#include "ui.h"

#define FETCH_KIND_FORECAST "forecast"
#define FETCH_KIND_AQI "aqi"
#define FETCH_BUNDLE_KEY "fetch-bundle"

struct WeatherFetchBundle {
    AppData *data;
    gchar *forecast_json;
    gchar *aqi_json;
    gboolean forecast_done;
    gboolean aqi_done;
    gboolean forecast_ok;
    gboolean aqi_ok;
};

typedef struct {
    AppData *data;
    gchar *forecast_json;
    gchar *aqi_json;
    gboolean aqi_available;
} WeatherParseBundleData;

static const char *weekdays_zh[] = {
    NULL,
    "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xb8\x80",
    "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xba\x8c",
    "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xb8\x89",
    "\xe6\x98\x9f\xe6\x9c\x9f\xe5\x9b\x9b",
    "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xba\x94",
    "\xe6\x98\x9f\xe6\x9c\x9f\xe5\x85\xad",
    "\xe6\x98\x9f\xe6\x9c\x9f\xe6\x97\xa5",
};

static gboolean retry_fetch_weather(gpointer user_data);

static void setup_weather_label(GtkWidget *label, gboolean wrap_text) {
    gtk_widget_set_hexpand(label, TRUE);
    gtk_widget_set_halign(label, GTK_ALIGN_FILL);
    gtk_label_set_xalign(GTK_LABEL(label), 0.5);
    if (wrap_text) {
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
        gtk_label_set_lines(GTK_LABEL(label), 2);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    } else {
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    }
}

static gchar *format_location_date(GDateTime *dt, AppLanguage lang) {
    if (lang == APP_LANG_ZH_CN) {
        gint year = g_date_time_get_year(dt);
        gint month = g_date_time_get_month(dt);
        gint day = g_date_time_get_day_of_month(dt);
        gint dow = g_date_time_get_day_of_week(dt);
        const char *wd = (dow >= 1 && dow <= 7) ? weekdays_zh[dow] : "";
        return g_strdup_printf("%d\xe5\xb9\xb4%d\xe6\x9c\x88%d\xe6\x97\xa5 %s",
                               year, month, day, wd);
    }
    return g_date_time_format(dt, "%A, %B %d, %Y");
}

static gboolean parse_iso_hour_timestamp(const gchar *time_str,
                                         int *year, int *month, int *day, int *hour) {
    if (!time_str || strlen(time_str) < 13) {
        return FALSE;
    }
    *year = atoi(time_str);
    *month = atoi(time_str + 5);
    *day = atoi(time_str + 8);
    *hour = atoi(time_str + 11);
    return TRUE;
}

static int weather_hourly_index_for_time(JsonArray *time_array,
                                         int year, int month, int day, int hour) {
    if (!time_array) {
        return -1;
    }

    guint array_length = json_array_get_length(time_array);
    for (guint i = 0; i < array_length; i++) {
        JsonNode *time_node = json_array_get_element(time_array, i);
        if (!time_node) {
            continue;
        }

        const gchar *time_str = json_node_get_string(time_node);
        int y, m, d, h;
        if (parse_iso_hour_timestamp(time_str, &y, &m, &d, &h) &&
            y == year && m == month && d == day && h == hour) {
            return (int)i;
        }
    }
    return -1;
}

static int weather_hourly_index_at_or_after(JsonArray *time_array,
                                            int year, int month, int day, int hour) {
    if (!time_array) {
        return -1;
    }

    guint array_length = json_array_get_length(time_array);
    for (guint i = 0; i < array_length; i++) {
        JsonNode *time_node = json_array_get_element(time_array, i);
        if (!time_node) {
            continue;
        }

        const gchar *time_str = json_node_get_string(time_node);
        int y, m, d, h;
        if (!parse_iso_hour_timestamp(time_str, &y, &m, &d, &h)) {
            continue;
        }

        if (y > year) {
            return (int)i;
        }
        if (y == year && m > month) {
            return (int)i;
        }
        if (y == year && m == month && d > day) {
            return (int)i;
        }
        if (y == year && m == month && d == day && h >= hour) {
            return (int)i;
        }
    }
    return -1;
}

void weather_refresh_date_label(AppData *data, GDateTime *location_dt) {
    if (!data || !data->date_label || !location_dt) {
        return;
    }
    if (!GTK_IS_LABEL(data->date_label)) {
        return;
    }

    gchar *date_part = format_location_date(location_dt, data->language);
    if (!date_part) {
        return;
    }

    if (!data->current_hour.valid) {
        gtk_label_set_text(GTK_LABEL(data->date_label), date_part);
        g_free(date_part);
        return;
    }

    const char *cond = get_weather_description(data->current_hour.weather_code, data->language);
    gchar *full = NULL;
    if (data->current_hour.us_aqi >= 0) {
        full = g_strdup_printf(i18n_(data, I18N_DATE_WITH_WEATHER),
                               date_part, cond,
                               data->current_hour.temperature,
                               data->current_hour.us_aqi);
    } else {
        full = g_strdup_printf(i18n_(data, I18N_DATE_WITH_WEATHER_NO_AQI),
                               date_part, cond,
                               data->current_hour.temperature);
    }

    if (full) {
        gtk_label_set_text(GTK_LABEL(data->date_label), full);
        g_free(full);
    } else {
        gtk_label_set_text(GTK_LABEL(data->date_label), date_part);
    }
    g_free(date_part);
}

const char *get_weather_description(int code, AppLanguage lang) {
    I18nId id = I18N_WEATHER_UNKNOWN;
    if (code == 0) {
        id = I18N_WEATHER_CLEAR;
    } else if (code <= 3) {
        id = I18N_WEATHER_CLOUDY;
    } else if (code <= 49) {
        id = I18N_WEATHER_FOGGY;
    } else if (code <= 59) {
        id = I18N_WEATHER_DRIZZLE;
    } else if (code <= 69) {
        id = I18N_WEATHER_RAIN;
    } else if (code <= 79) {
        id = I18N_WEATHER_SNOW;
    } else if (code <= 84) {
        id = I18N_WEATHER_RAIN_SHOWER;
    } else if (code <= 86) {
        id = I18N_WEATHER_SNOW_SHOWER;
    } else if (code <= 99) {
        id = I18N_WEATHER_THUNDERSTORM;
    }
    return i18n_get(lang, id);
}

const char *get_weather_icon(int code) {
    if (code == 0) return "\xe2\x98\x80\xef\xb8\x8f";
    if (code <= 3) return "\xe2\x9b\x85";
    if (code <= 49) return "\xf0\x9f\x8c\xab\xef\xb8\x8f";
    if (code <= 59) return "\xf0\x9f\x8c\xa6\xef\xb8\x8f";
    if (code <= 69) return "\xf0\x9f\x8c\xa7\xef\xb8\x8f";
    if (code <= 79) return "\xe2\x9d\x84\xef\xb8\x8f";
    if (code <= 84) return "\xf0\x9f\x8c\xa6\xef\xb8\x8f";
    if (code <= 86) return "\xe2\x9d\x84\xef\xb8\x8f";
    if (code <= 99) return "\xe2\x9b\x88\xef\xb8\x8f";
    return "\xe2\x9d\x93";
}

static void clear_weather_box(AppData *data) {
    if (!data || !data->weather_box || !GTK_IS_BOX(data->weather_box)) {
        return;
    }

    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(data->weather_box))) {
        gtk_box_remove(GTK_BOX(data->weather_box), child);
    }
}

static void append_weather_error(AppData *data, const gchar *text) {
    GtkWidget *error_label = gtk_label_new(text ? text : "");
    gtk_widget_add_css_class(error_label, "error-text");
    gtk_box_append(GTK_BOX(data->weather_box), error_label);
}

static gint read_json_number(JsonNode *node) {
    if (!node) {
        return 0;
    }
    if (json_node_get_value_type(node) == G_TYPE_INT64) {
        return (gint)json_node_get_int(node);
    }
    if (json_node_get_value_type(node) == G_TYPE_DOUBLE) {
        return (gint)json_node_get_double(node);
    }
    return 0;
}

static gdouble read_json_double(JsonNode *node) {
    if (!node) {
        return 0.0;
    }
    if (json_node_get_value_type(node) == G_TYPE_DOUBLE) {
        return json_node_get_double(node);
    }
    if (json_node_get_value_type(node) == G_TYPE_INT64) {
        return (gdouble)json_node_get_int(node);
    }
    return 0.0;
}

static gint read_us_aqi_at(JsonArray *aqi_array, guint idx) {
    if (!aqi_array || idx >= json_array_get_length(aqi_array)) {
        return -1;
    }

    JsonNode *node = json_array_get_element(aqi_array, idx);
    if (!node) {
        return -1;
    }

    if (json_node_get_node_type(node) == JSON_NODE_NULL) {
        return -1;
    }

    GType t = json_node_get_value_type(node);
    if (t == G_TYPE_INT64) {
        return (gint)json_node_get_int(node);
    }
    if (t == G_TYPE_DOUBLE) {
        return (gint)json_node_get_double(node);
    }
    return -1;
}

static void apply_forecast_metadata(JsonObject *root_obj, AppData *data) {
    if (!root_obj || !data) {
        return;
    }

    if (json_object_has_member(root_obj, "timezone")) {
        JsonNode *tz_node = json_object_get_member(root_obj, "timezone");
        const gchar *tz_str = NULL;
        if (JSON_NODE_HOLDS_VALUE(tz_node) &&
            json_node_get_value_type(tz_node) == G_TYPE_STRING) {
            tz_str = json_node_get_string(tz_node);
        }
        if (tz_str && strlen(tz_str) > 0) {
            if (data->tz) {
                g_time_zone_unref(data->tz);
                data->tz = NULL;
            }
            g_free(data->timezone);
            data->timezone = g_strdup(tz_str);
            data->tz = g_time_zone_new_identifier(tz_str);
            if (!data->tz) {
                G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                data->tz = g_time_zone_new(tz_str);
                G_GNUC_END_IGNORE_DEPRECATIONS
                if (!data->tz) {
                    g_debug("Timezone '%s' not available, using UTC offset", tz_str);
                } else {
                    g_info("Using timezone: %s", tz_str);
                    save_location_to_config(data);
                }
            } else {
                g_info("Using timezone: %s", tz_str);
                save_location_to_config(data);
            }
        }
    }

    if (json_object_has_member(root_obj, "utc_offset_seconds")) {
        JsonNode *offset_node = json_object_get_member(root_obj, "utc_offset_seconds");
        GType offset_type = json_node_get_value_type(offset_node);

        gint new_offset = 0;
        if (offset_type == G_TYPE_INT64) {
            new_offset = (gint)json_node_get_int(offset_node);
        } else if (offset_type == G_TYPE_DOUBLE) {
            new_offset = (gint)json_node_get_double(offset_node);
        } else if (JSON_NODE_HOLDS_VALUE(offset_node)) {
            new_offset = (gint)json_node_get_int(offset_node);
        }

        if (new_offset != data->utc_offset_seconds) {
            gint old_offset = data->utc_offset_seconds;
            data->utc_offset_seconds = new_offset;

            if (old_offset == 0) {
                g_info("UTC offset: %+.1f hours", new_offset / 3600.0);
            } else {
                g_info("UTC offset changed: %+.1f -> %+.1f hours",
                       old_offset / 3600.0, new_offset / 3600.0);
            }
            save_location_to_config(data);
        }
    }
}

static gboolean handle_forecast_api_error(JsonObject *root_obj, AppData *data) {
    if (!json_object_has_member(root_obj, "error")) {
        return FALSE;
    }

    JsonNode *error_node = json_object_get_member(root_obj, "error");
    const gchar *error_msg = NULL;

    GList *members_list = json_object_get_members(root_obj);
    GString *keys_str = g_string_new("");
    for (GList *iter = members_list; iter != NULL; iter = iter->next) {
        if (keys_str->len > 0) {
            g_string_append(keys_str, ", ");
        }
        g_string_append(keys_str, (const gchar *)iter->data);
    }

    GType error_type = json_node_get_value_type(error_node);
    if (error_type == G_TYPE_BOOLEAN) {
        if (json_node_get_boolean(error_node)) {
            if (json_object_has_member(root_obj, "reason")) {
                error_msg = json_node_get_string(json_object_get_member(root_obj, "reason"));
            } else {
                error_msg = "API returned error=true but no reason field";
            }
        }
    } else if (error_type == G_TYPE_STRING) {
        error_msg = json_node_get_string(error_node);
    }

    gchar *error_text = NULL;
    if (error_msg) {
        error_text = g_strdup_printf(i18n_(data, I18N_ERR_API_FMT), error_msg, keys_str->str);
    } else if (error_type != G_TYPE_BOOLEAN && error_type != G_TYPE_STRING) {
        error_text = g_strdup_printf(i18n_(data, I18N_ERR_API_TYPE_FMT),
                                     g_type_name(error_type), keys_str->str);
    } else {
        error_text = g_strdup_printf(i18n_(data, I18N_ERR_API_UNKNOWN_FMT), keys_str->str);
    }

    append_weather_error(data, error_text);
    g_free(error_text);
    g_string_free(keys_str, TRUE);
    g_list_free(members_list);
    return TRUE;
}

static void build_forecast_cards(AppData *data,
                                 JsonArray *time_array,
                                 JsonArray *temp_array,
                                 JsonArray *code_array,
                                 int forecast_start) {
    guint array_length = json_array_get_length(time_array);

    for (guint i = 0; i < WEATHER_HOUR_COUNT; i++) {
        guint idx = (guint)forecast_start + i;
        if (idx >= array_length) {
            break;
        }

        JsonNode *time_node = json_array_get_element(time_array, idx);
        JsonNode *temp_node = json_array_get_element(temp_array, idx);
        JsonNode *code_node = json_array_get_element(code_array, idx);

        if (!time_node || !temp_node || !code_node) {
            continue;
        }

        const gchar *time_str = json_node_get_string(time_node);
        if (!time_str) {
            continue;
        }

        gdouble temp = read_json_double(temp_node);
        gint code = read_json_number(code_node);

        GtkWidget *hour_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_widget_add_css_class(hour_box, "weather-hour");
        gtk_widget_set_hexpand(hour_box, TRUE);
        gtk_widget_set_halign(hour_box, GTK_ALIGN_FILL);

        char hour_str[8];
        if (strlen(time_str) >= 13) {
            snprintf(hour_str, sizeof(hour_str), "%.2s:00", time_str + 11);
            hour_str[7] = '\0';
        } else {
            strncpy(hour_str, i18n_(data, I18N_NA), sizeof(hour_str) - 1);
            hour_str[sizeof(hour_str) - 1] = '\0';
        }

        GtkWidget *time_label = gtk_label_new(hour_str);
        gtk_widget_add_css_class(time_label, "weather-time");
        setup_weather_label(time_label, FALSE);
        gtk_box_append(GTK_BOX(hour_box), time_label);

        GtkWidget *icon_label = gtk_label_new(get_weather_icon(code));
        gtk_widget_add_css_class(icon_label, "weather-icon");
        setup_weather_label(icon_label, FALSE);
        gtk_box_append(GTK_BOX(hour_box), icon_label);

        char temp_str[32];
        int temp_len = snprintf(temp_str, sizeof(temp_str), "%.1f\xc2\xb0""C", temp);
        if (temp_len < 0 || temp_len >= (int)sizeof(temp_str)) {
            strncpy(temp_str, i18n_(data, I18N_NA), sizeof(temp_str) - 1);
            temp_str[sizeof(temp_str) - 1] = '\0';
        }
        GtkWidget *temp_label = gtk_label_new(temp_str);
        gtk_widget_add_css_class(temp_label, "weather-temp");
        setup_weather_label(temp_label, FALSE);
        gtk_box_append(GTK_BOX(hour_box), temp_label);

        GtkWidget *desc_label = gtk_label_new(get_weather_description(code, data->language));
        gtk_widget_add_css_class(desc_label, "weather-desc");
        setup_weather_label(desc_label, TRUE);
        gtk_box_append(GTK_BOX(hour_box), desc_label);

        gtk_box_append(GTK_BOX(data->weather_box), hour_box);
    }
}

static gboolean parse_weather_bundle(const char *forecast_json,
                                     const char *aqi_json,
                                     gboolean aqi_available,
                                     AppData *data) {
    if (!data || !data->weather_box) {
        return FALSE;
    }

    if (!GTK_IS_BOX(data->weather_box)) {
        return FALSE;
    }

    clear_weather_box(data);
    data->current_hour.valid = FALSE;

    if (!forecast_json || strlen(forecast_json) == 0) {
        append_weather_error(data, i18n_(data, I18N_ERR_EMPTY_WEATHER));
        return FALSE;
    }

    if (forecast_json[0] == '<') {
        append_weather_error(data, i18n_(data, I18N_ERR_SERVER_HTML));
        return FALSE;
    }

    JsonParser *parser = json_parser_new();
    GError *error = NULL;

    if (!json_parser_load_from_data(parser, forecast_json, -1, &error)) {
        const gchar *detail = (error && error->message)
            ? error->message
            : i18n_(data, I18N_ERR_UNKNOWN_DETAIL);
        gchar *error_msg = g_strdup_printf(i18n_(data, I18N_ERR_PARSE_FMT), detail);
        append_weather_error(data, error_msg);
        g_free(error_msg);
        if (error) {
            g_error_free(error);
        }
        g_object_unref(parser);
        return FALSE;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (!root) {
        append_weather_error(data, i18n_(data, I18N_ERR_NO_ROOT));
        g_object_unref(parser);
        return FALSE;
    }

    JsonObject *root_obj = json_node_get_object(root);
    if (!root_obj) {
        append_weather_error(data, i18n_(data, I18N_ERR_INVALID_FORMAT));
        g_object_unref(parser);
        return FALSE;
    }

    apply_forecast_metadata(root_obj, data);

    if (handle_forecast_api_error(root_obj, data)) {
        g_object_unref(parser);
        return TRUE;
    }

    if (!json_object_has_member(root_obj, "hourly")) {
        GList *members_list = json_object_get_members(root_obj);
        GString *members_str = g_string_new("");
        for (GList *iter = members_list; iter != NULL; iter = iter->next) {
            if (members_str->len > 0) {
                g_string_append(members_str, ", ");
            }
            g_string_append(members_str, (const gchar *)iter->data);
        }
        gchar *error_msg = g_strdup_printf(i18n_(data, I18N_ERR_NO_HOURLY_FMT), members_str->str);
        append_weather_error(data, error_msg);
        g_free(error_msg);
        g_string_free(members_str, TRUE);
        g_list_free(members_list);
        g_object_unref(parser);
        return FALSE;
    }

    JsonObject *hourly = json_object_get_object_member(root_obj, "hourly");
    if (!hourly) {
        append_weather_error(data, i18n_(data, I18N_ERR_NO_HOURLY_DATA));
        g_object_unref(parser);
        return FALSE;
    }

    JsonArray *time_array = json_object_get_array_member(hourly, "time");
    JsonArray *temp_array = json_object_get_array_member(hourly, "temperature_2m");
    JsonArray *code_array = json_object_get_array_member(hourly, "weathercode");

    if (!time_array || !temp_array || !code_array) {
        append_weather_error(data, i18n_(data, I18N_ERR_INCOMPLETE_DATA));
        g_object_unref(parser);
        return FALSE;
    }

    GDateTime *location_now = clock_get_location_datetime(data);
    if (!location_now) {
        append_weather_error(data, i18n_(data, I18N_ERR_INCOMPLETE_DATA));
        g_object_unref(parser);
        return FALSE;
    }

    int loc_year = g_date_time_get_year(location_now);
    int loc_month = g_date_time_get_month(location_now);
    int loc_day = g_date_time_get_day_of_month(location_now);
    int loc_hour = g_date_time_get_hour(location_now);

    int current_idx = weather_hourly_index_for_time(time_array, loc_year, loc_month, loc_day, loc_hour);
    if (current_idx < 0) {
        current_idx = weather_hourly_index_at_or_after(time_array, loc_year, loc_month, loc_day, loc_hour);
    }

    int forecast_start = current_idx + 1;
    guint array_length = json_array_get_length(time_array);

    if (current_idx < 0 ||
        (guint)(forecast_start + WEATHER_HOUR_COUNT) > array_length) {
        append_weather_error(data, i18n_(data, I18N_ERR_INCOMPLETE_DATA));
        g_date_time_unref(location_now);
        g_object_unref(parser);
        return FALSE;
    }

    JsonNode *cur_temp_node = json_array_get_element(temp_array, (guint)current_idx);
    JsonNode *cur_code_node = json_array_get_element(code_array, (guint)current_idx);
    if (!cur_temp_node || !cur_code_node) {
        append_weather_error(data, i18n_(data, I18N_ERR_INCOMPLETE_DATA));
        g_date_time_unref(location_now);
        g_object_unref(parser);
        return FALSE;
    }

    data->current_hour.valid = TRUE;
    data->current_hour.temperature = read_json_double(cur_temp_node);
    data->current_hour.weather_code = read_json_number(cur_code_node);
    data->current_hour.us_aqi = -1;

    if (aqi_available && aqi_json && strlen(aqi_json) > 0 && aqi_json[0] != '<') {
        JsonParser *aqi_parser = json_parser_new();
        GError *aqi_error = NULL;
        if (json_parser_load_from_data(aqi_parser, aqi_json, -1, &aqi_error)) {
            JsonNode *aqi_root = json_parser_get_root(aqi_parser);
            JsonObject *aqi_obj = aqi_root ? json_node_get_object(aqi_root) : NULL;
            if (aqi_obj && json_object_has_member(aqi_obj, "hourly")) {
                JsonObject *aqi_hourly = json_object_get_object_member(aqi_obj, "hourly");
                if (aqi_hourly && json_object_has_member(aqi_hourly, "us_aqi")) {
                    JsonArray *aqi_array = json_object_get_array_member(aqi_hourly, "us_aqi");
                    data->current_hour.us_aqi = read_us_aqi_at(aqi_array, (guint)current_idx);
                }
            }
        }
        if (aqi_error) {
            g_error_free(aqi_error);
        }
        g_object_unref(aqi_parser);
    }

    build_forecast_cards(data, time_array, temp_array, code_array, forecast_start);
    weather_refresh_date_label(data, location_now);
    weather_layout_update_later(data);

    g_date_time_unref(location_now);
    g_object_unref(parser);
    return TRUE;
}

static void free_fetch_bundle(WeatherFetchBundle *bundle) {
    if (!bundle) {
        return;
    }
    g_free(bundle->forecast_json);
    g_free(bundle->aqi_json);
    g_free(bundle);
}

void weather_cancel_pending_fetches(AppData *data) {
    if (!data) {
        return;
    }

    if (data->pending_forecast_message) {
        g_object_unref(data->pending_forecast_message);
        data->pending_forecast_message = NULL;
    }
    if (data->pending_aqi_message) {
        g_object_unref(data->pending_aqi_message);
        data->pending_aqi_message = NULL;
    }
    if (data->fetch_bundle) {
        free_fetch_bundle(data->fetch_bundle);
        data->fetch_bundle = NULL;
    }
}

static void schedule_weather_retry(AppData *data, gboolean show_error_ui) {
    if (!data || !data->session) {
        return;
    }

    if (data->retry_count < MAX_RETRY_ATTEMPTS) {
        data->retry_delay = INITIAL_RETRY_DELAY * (1 << data->retry_count);
        if (data->retry_delay > MAX_RETRY_DELAY) {
            data->retry_delay = MAX_RETRY_DELAY;
        }
        data->is_retrying = TRUE;

        if (show_error_ui && data->weather_box && GTK_IS_BOX(data->weather_box)) {
            clear_weather_box(data);
            gchar *error_msg = g_strdup_printf(i18n_(data, I18N_ERR_CONNECTION_FMT),
                                               data->retry_delay,
                                               data->retry_count + 1,
                                               MAX_RETRY_ATTEMPTS);
            append_weather_error(data, error_msg);
            g_free(error_msg);
        }

        if (data->retry_timer_id != 0) {
            g_source_remove(data->retry_timer_id);
        }
        data->retry_timer_id = g_timeout_add_seconds(data->retry_delay, retry_fetch_weather, data);
        data->retry_count++;
    } else {
        data->is_retrying = FALSE;
        data->retry_count = 0;
        data->retry_delay = 0;

        if (show_error_ui && data->weather_box && GTK_IS_BOX(data->weather_box)) {
            clear_weather_box(data);
            append_weather_error(data, i18n_(data, I18N_ERR_FETCH_FAILED));
        }
    }
}

static gboolean parse_weather_idle(gpointer user_data) {
    WeatherParseBundleData *parse_data = (WeatherParseBundleData *)user_data;
    if (!parse_data) {
        return G_SOURCE_REMOVE;
    }

    AppData *data = parse_data->data;
    gboolean success = FALSE;

    if (data && data->session) {
        success = parse_weather_bundle(parse_data->forecast_json,
                                       parse_data->aqi_json,
                                       parse_data->aqi_available,
                                       data);

        GDateTime *dt = clock_get_location_datetime(data);
        if (dt) {
            weather_refresh_date_label(data, dt);
            g_date_time_unref(dt);
        }

        if (!success && data->retry_count < MAX_RETRY_ATTEMPTS) {
            g_warning("Weather parse failed, scheduling retry (attempt %d/%d)",
                      data->retry_count + 1, MAX_RETRY_ATTEMPTS);
            data->is_retrying = TRUE;
            if (data->retry_timer_id != 0) {
                g_source_remove(data->retry_timer_id);
            }
            data->retry_delay = INITIAL_RETRY_DELAY * (1 << data->retry_count);
            if (data->retry_delay > MAX_RETRY_DELAY) {
                data->retry_delay = MAX_RETRY_DELAY;
            }
            data->retry_timer_id = g_timeout_add_seconds(data->retry_delay, retry_fetch_weather, data);
            data->retry_count++;
        } else if (!success) {
            data->is_retrying = FALSE;
            data->retry_count = 0;
            data->retry_delay = 0;
        } else {
            if (data->retry_count > 0) {
                g_info("Weather data parsed successfully after %d retry attempt(s)", data->retry_count);
            }
            data->is_retrying = FALSE;
            data->retry_count = 0;
            data->retry_delay = 0;
            if (data->retry_timer_id != 0) {
                g_source_remove(data->retry_timer_id);
                data->retry_timer_id = 0;
            }
        }
    }

    g_free(parse_data->forecast_json);
    g_free(parse_data->aqi_json);
    g_free(parse_data);
    return G_SOURCE_REMOVE;
}

static void try_finish_fetch_bundle(AppData *data) {
    WeatherFetchBundle *bundle = data->fetch_bundle;
    if (!bundle || !bundle->forecast_done || !bundle->aqi_done) {
        return;
    }

    data->fetch_bundle = NULL;

    if (!bundle->forecast_ok) {
        g_warning("Forecast fetch failed (attempt %d/%d)",
                  data->retry_count + 1, MAX_RETRY_ATTEMPTS);
        schedule_weather_retry(data, TRUE);
        free_fetch_bundle(bundle);
        return;
    }

    data->retry_count = 0;
    data->retry_delay = 0;
    data->is_retrying = FALSE;
    if (data->retry_timer_id != 0) {
        g_source_remove(data->retry_timer_id);
        data->retry_timer_id = 0;
    }

    WeatherParseBundleData *parse_data = g_new0(WeatherParseBundleData, 1);
    parse_data->data = data;
    parse_data->forecast_json = g_steal_pointer(&bundle->forecast_json);
    parse_data->aqi_json = bundle->aqi_ok ? g_steal_pointer(&bundle->aqi_json) : NULL;
    parse_data->aqi_available = bundle->aqi_ok;
    free_fetch_bundle(bundle);

    if (!parse_data->forecast_json) {
        g_free(parse_data);
        schedule_weather_retry(data, TRUE);
        return;
    }

    g_idle_add(parse_weather_idle, parse_data);
}

static void on_fetch_part_response(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    SoupMessage *msg = SOUP_MESSAGE(user_data);
    if (!msg) {
        return;
    }

    AppData *data = (AppData *)g_object_get_data(G_OBJECT(msg), "app-data");
    WeatherFetchBundle *msg_bundle = (WeatherFetchBundle *)g_object_get_data(G_OBJECT(msg),
                                                                             FETCH_BUNDLE_KEY);
    const char *kind = (const char *)g_object_get_data(G_OBJECT(msg), "fetch-kind");
    gboolean is_forecast = kind && g_strcmp0(kind, FETCH_KIND_FORECAST) == 0;
    gboolean is_aqi = kind && g_strcmp0(kind, FETCH_KIND_AQI) == 0;

    GError *error = NULL;
    GBytes *body_bytes = soup_session_send_and_read_finish(SOUP_SESSION(source_object), res, &error);

    if (data) {
        if (is_forecast && data->pending_forecast_message == msg) {
            data->pending_forecast_message = NULL;
        }
        if (is_aqi && data->pending_aqi_message == msg) {
            data->pending_aqi_message = NULL;
        }
    }

    if (!data || !data->session || !data->fetch_bundle ||
        msg_bundle != data->fetch_bundle) {
        if (error) {
            g_error_free(error);
        }
        if (body_bytes) {
            g_bytes_unref(body_bytes);
        }
        g_object_unref(msg);
        return;
    }

    WeatherFetchBundle *bundle = data->fetch_bundle;

    if (is_forecast) {
        if (error) {
            g_warning("Forecast fetch error: %s", error->message);
            bundle->forecast_ok = FALSE;
        } else if (!body_bytes) {
            g_warning("Forecast fetch: empty body");
            bundle->forecast_ok = FALSE;
        } else {
            gsize length;
            const gchar *body = (const gchar *)g_bytes_get_data(body_bytes, &length);
            if (body && length > 0) {
                bundle->forecast_json = g_strndup(body, length);
                bundle->forecast_ok = bundle->forecast_json != NULL;
            }
        }
        bundle->forecast_done = TRUE;
    } else if (is_aqi) {
        if (error) {
            g_warning("AQI fetch error: %s (continuing without AQI)", error->message);
            bundle->aqi_ok = FALSE;
        } else if (!body_bytes) {
            g_warning("AQI fetch: empty body (continuing without AQI)");
            bundle->aqi_ok = FALSE;
        } else {
            gsize length;
            const gchar *body = (const gchar *)g_bytes_get_data(body_bytes, &length);
            if (body && length > 0) {
                bundle->aqi_json = g_strndup(body, length);
                bundle->aqi_ok = bundle->aqi_json != NULL;
            }
        }
        bundle->aqi_done = TRUE;
    } else {
        g_warning("Weather fetch callback with unknown kind");
        if (body_bytes) {
            g_bytes_unref(body_bytes);
        }
        if (error) {
            g_error_free(error);
        }
        g_object_unref(msg);
        return;
    }

    if (body_bytes) {
        g_bytes_unref(body_bytes);
    }
    g_clear_error(&error);
    g_object_unref(msg);

    try_finish_fetch_bundle(data);
}

static gboolean retry_fetch_weather(gpointer user_data) {
    AppData *data = (AppData *)user_data;
    if (!data || !data->session) {
        return G_SOURCE_REMOVE;
    }

    data->retry_timer_id = 0;
    g_info("Retrying weather fetch (attempt %d/%d)...", data->retry_count + 1, MAX_RETRY_ATTEMPTS);
    fetch_weather(data);
    return G_SOURCE_REMOVE;
}

static gboolean delayed_scheduled_fetch(gpointer user_data) {
    AppData *data = (AppData *)user_data;
    if (!data) {
        return G_SOURCE_REMOVE;
    }

    data->scheduled_fetch_timer_id = 0;

    if (!data->session) {
        return G_SOURCE_REMOVE;
    }

    g_info("Hourly weather refresh (+%d s after hour boundary)", WEATHER_HOURLY_REFRESH_DELAY_SECONDS);
    fetch_weather(data);
    return G_SOURCE_REMOVE;
}

void schedule_delayed_weather_fetch(AppData *data) {
    if (!data || !data->session) {
        return;
    }

    if (data->scheduled_fetch_timer_id != 0) {
        g_source_remove(data->scheduled_fetch_timer_id);
        data->scheduled_fetch_timer_id = 0;
    }

    data->scheduled_fetch_timer_id = g_timeout_add_seconds(WEATHER_HOURLY_REFRESH_DELAY_SECONDS,
                                                           delayed_scheduled_fetch, data);
}

static gboolean start_fetch_message(AppData *data, const char *url, const char *kind,
                                    SoupMessage **pending_slot) {
    SoupMessage *msg = soup_message_new("GET", url);
    if (!msg) {
        g_warning("Failed to create SoupMessage for %s", kind);
        return FALSE;
    }

    g_object_set_data(G_OBJECT(msg), "app-data", data);
    g_object_set_data(G_OBJECT(msg), "fetch-kind", (gpointer)kind);
    if (data->fetch_bundle) {
        g_object_set_data(G_OBJECT(msg), FETCH_BUNDLE_KEY, data->fetch_bundle);
    }
    *pending_slot = msg;
    g_object_ref(msg);
    soup_session_send_and_read_async(data->session, msg, G_PRIORITY_DEFAULT, NULL,
                                     on_fetch_part_response, msg);
    return TRUE;
}

void fetch_weather(AppData *data) {
    if (!data || !data->session) {
        return;
    }

    weather_cancel_pending_fetches(data);

    if (!data->is_retrying) {
        if (data->retry_timer_id != 0) {
            g_source_remove(data->retry_timer_id);
            data->retry_timer_id = 0;
        }
        data->retry_count = 0;
        data->retry_delay = 0;
    }

    const gchar *lat = data->location_lat ? data->location_lat : "52.52";
    const gchar *lon = data->location_lon ? data->location_lon : "13.41";

    if (data->lat_entry && GTK_IS_EDITABLE(data->lat_entry)) {
        const gchar *lat_text = gtk_editable_get_text(GTK_EDITABLE(data->lat_entry));
        if (lat_text && strlen(lat_text) > 0) {
            lat = lat_text;
        }
    }
    if (data->lon_entry && GTK_IS_EDITABLE(data->lon_entry)) {
        const gchar *lon_text = gtk_editable_get_text(GTK_EDITABLE(data->lon_entry));
        if (lon_text && strlen(lon_text) > 0) {
            lon = lon_text;
        }
    }

    if (strlen(lat) > 20 || strlen(lon) > 20) {
        g_warning("Latitude or longitude too long, using defaults");
        lat = "52.52";
        lon = "13.41";
    }

    char forecast_url[512];
    int forecast_len = snprintf(forecast_url, sizeof(forecast_url),
                                "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
                                "&hourly=temperature_2m,weathercode&forecast_days=2&timezone=auto",
                                lat, lon);
    if (forecast_len < 0 || forecast_len >= (int)sizeof(forecast_url)) {
        g_warning("Forecast URL construction failed");
        return;
    }

    char aqi_url[512];
    int aqi_len = snprintf(aqi_url, sizeof(aqi_url),
                           "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%s&longitude=%s"
                           "&hourly=us_aqi&timezone=auto&forecast_days=2",
                           lat, lon);
    if (aqi_len < 0 || aqi_len >= (int)sizeof(aqi_url)) {
        g_warning("AQI URL construction failed");
        return;
    }

    WeatherFetchBundle *bundle = g_new0(WeatherFetchBundle, 1);
    if (!bundle) {
        return;
    }
    bundle->data = data;
    data->fetch_bundle = bundle;

    g_debug("Fetching forecast: %s", forecast_url);
    g_debug("Fetching AQI: %s", aqi_url);

    gboolean forecast_started = start_fetch_message(data, forecast_url, FETCH_KIND_FORECAST,
                                                  &data->pending_forecast_message);
    gboolean aqi_started = start_fetch_message(data, aqi_url, FETCH_KIND_AQI,
                                               &data->pending_aqi_message);

    if (!forecast_started) {
        bundle->forecast_done = TRUE;
        bundle->forecast_ok = FALSE;
    }
    if (!aqi_started) {
        bundle->aqi_done = TRUE;
        bundle->aqi_ok = FALSE;
    }
    try_finish_fetch_bundle(data);
}

gboolean update_weather_callback(gpointer user_data) {
    AppData *data = (AppData *)user_data;
    if (!data || !data->session) {
        return G_SOURCE_REMOVE;
    }

    schedule_delayed_weather_fetch(data);

    if (data->weather_timer_id != 0) {
        g_source_remove(data->weather_timer_id);
        data->weather_timer_id = 0;
    }

    data->weather_timer_id = g_timeout_add_seconds(UPDATE_INTERVAL_SECONDS, update_weather_callback, data);

    return G_SOURCE_REMOVE;
}
