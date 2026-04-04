#include "weather.h"
#include "config.h"

typedef struct {
    AppData *data;
    gchar *json_data;
} WeatherParseData;

static gboolean retry_fetch_weather(gpointer user_data);

const char *get_weather_description(int code) {
    if (code == 0) return "Clear";
    if (code <= 3) return "Cloudy";
    if (code <= 49) return "Foggy";
    if (code <= 59) return "Drizzle";
    if (code <= 69) return "Rain";
    if (code <= 79) return "Snow";
    if (code <= 84) return "Rain Shower";
    if (code <= 86) return "Snow Shower";
    if (code <= 99) return "Thunderstorm";
    return "Unknown";
}

const char *get_weather_icon(int code) {
    if (code == 0) return "\xe2\x98\x80\xef\xb8\x8f";       /* sun */
    if (code <= 3) return "\xe2\x9b\x85";                     /* sun behind cloud */
    if (code <= 49) return "\xf0\x9f\x8c\xab\xef\xb8\x8f";   /* fog */
    if (code <= 59) return "\xf0\x9f\x8c\xa6\xef\xb8\x8f";   /* sun behind rain cloud */
    if (code <= 69) return "\xf0\x9f\x8c\xa7\xef\xb8\x8f";   /* cloud with rain */
    if (code <= 79) return "\xe2\x9d\x84\xef\xb8\x8f";       /* snowflake */
    if (code <= 84) return "\xf0\x9f\x8c\xa6\xef\xb8\x8f";   /* sun behind rain cloud */
    if (code <= 86) return "\xe2\x9d\x84\xef\xb8\x8f";       /* snowflake */
    if (code <= 99) return "\xe2\x9b\x88\xef\xb8\x8f";       /* thunderstorm */
    return "\xe2\x9d\x93";                                     /* question mark */
}

static gboolean parse_weather_json(const char *json_data_str, AppData *data) {
    if (!data || !data->weather_box) {
        return FALSE;
    }

    if (!GTK_IS_BOX(data->weather_box)) {
        return FALSE;
    }

    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(data->weather_box))) {
        gtk_box_remove(GTK_BOX(data->weather_box), child);
    }

    if (!json_data_str || strlen(json_data_str) == 0) {
        GtkWidget *error_label = gtk_label_new("Empty weather data received");
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);
        return FALSE;
    }

    if (json_data_str[0] == '<') {
        g_warning("API returned HTML instead of JSON (likely server error)");
        GtkWidget *error_label = gtk_label_new("Server returned error page - retrying...");
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);
        return FALSE;
    }

    g_debug("JSON response length: %zu", strlen(json_data_str));
    if (strlen(json_data_str) < 500) {
        g_debug("Full JSON: %s", json_data_str);
    } else {
        gchar *preview = g_strndup(json_data_str, 500);
        if (preview) {
            g_debug("JSON preview: %s...", preview);
            g_free(preview);
        }
    }

    JsonParser *parser = json_parser_new();
    GError *error = NULL;

    if (!json_parser_load_from_data(parser, json_data_str, -1, &error)) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Parse error: %s - retrying...", error ? error->message : "Unknown");
        g_warning("JSON parse error: %s", error ? error->message : "Unknown");
        GtkWidget *error_label = gtk_label_new(error_msg);
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);
        if (error) g_error_free(error);
        g_object_unref(parser);
        return FALSE;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (!root) {
        GtkWidget *error_label = gtk_label_new("Invalid JSON: no root node - retrying...");
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);
        g_object_unref(parser);
        return FALSE;
    }

    JsonObject *root_obj = json_node_get_object(root);
    if (!root_obj) {
        GtkWidget *error_label = gtk_label_new("Invalid weather data format - retrying...");
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);
        g_object_unref(parser);
        return FALSE;
    }

    /* Extract timezone from API response */
    if (json_object_has_member(root_obj, "timezone")) {
        JsonNode *tz_node = json_object_get_member(root_obj, "timezone");
        const gchar *tz_str = json_node_get_string(tz_node);
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
                    g_debug("Timezone '%s' not available (normal on Windows), using UTC offset", tz_str);
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

    /* Extract UTC offset as fallback (in seconds) */
    if (json_object_has_member(root_obj, "utc_offset_seconds")) {
        JsonNode *offset_node = json_object_get_member(root_obj, "utc_offset_seconds");
        GType offset_type = json_node_get_value_type(offset_node);

        gint new_offset = 0;
        if (offset_type == G_TYPE_INT64) {
            gint64 offset = json_node_get_int(offset_node);
            new_offset = (gint)offset;
        } else if (offset_type == G_TYPE_DOUBLE) {
            gdouble offset = json_node_get_double(offset_node);
            new_offset = (gint)offset;
        } else if (JSON_NODE_HOLDS_VALUE(offset_node)) {
            new_offset = (gint)json_node_get_int(offset_node);
        }

        if (new_offset != data->utc_offset_seconds) {
            gint old_offset = data->utc_offset_seconds;
            data->utc_offset_seconds = new_offset;

            if (old_offset == 0) {
                g_info("UTC offset: %+.1f hours (timezone database not available, using offset)",
                       new_offset / 3600.0);
            } else {
                g_info("UTC offset changed: %+.1f -> %+.1f hours (DST transition?)",
                       old_offset / 3600.0, new_offset / 3600.0);
            }

            save_location_to_config(data);
        }
    }

    /* Check for API errors */
    if (json_object_has_member(root_obj, "error")) {
        JsonNode *error_node = json_object_get_member(root_obj, "error");
        const gchar *error_msg = NULL;

        GList *members_list = json_object_get_members(root_obj);
        GString *keys_str = g_string_new("");
        GList *iter;
        for (iter = members_list; iter != NULL; iter = iter->next) {
            if (keys_str->len > 0) {
                g_string_append(keys_str, ", ");
            }
            g_string_append(keys_str, (const gchar *)iter->data);
        }

        GType error_type = json_node_get_value_type(error_node);
        if (error_type == G_TYPE_BOOLEAN) {
            gboolean error_bool = json_node_get_boolean(error_node);
            if (error_bool) {
                if (json_object_has_member(root_obj, "reason")) {
                    JsonNode *reason_node = json_object_get_member(root_obj, "reason");
                    error_msg = json_node_get_string(reason_node);
                } else {
                    error_msg = "API returned error=true but no reason field";
                }
            } else {
                error_msg = "API returned error=false (should not happen)";
            }
        } else if (error_type == G_TYPE_STRING) {
            error_msg = json_node_get_string(error_node);
        } else {
            error_msg = NULL;
        }

        char error_text[512];
        if (error_msg) {
            snprintf(error_text, sizeof(error_text), "API Error: %s (Keys: %s)", error_msg, keys_str->str);
        } else if (error_type != G_TYPE_BOOLEAN && error_type != G_TYPE_STRING) {
            snprintf(error_text, sizeof(error_text), "API Error: Error type: %s (Keys: %s)",
                     g_type_name(error_type), keys_str->str);
        } else {
            snprintf(error_text, sizeof(error_text), "API Error: Unknown (Keys: %s)", keys_str->str);
        }

        GtkWidget *error_label = gtk_label_new(error_text);
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);

        g_string_free(keys_str, TRUE);
        g_list_free(members_list);
        g_object_unref(parser);
        return TRUE;
    }

    if (!json_object_has_member(root_obj, "hourly")) {
        GList *members_list = json_object_get_members(root_obj);
        GString *members_str = g_string_new("");
        GList *iter;
        for (iter = members_list; iter != NULL; iter = iter->next) {
            if (members_str->len > 0) {
                g_string_append(members_str, ", ");
            }
            g_string_append(members_str, (const gchar *)iter->data);
        }
        g_warning("No 'hourly' key found. Available keys: %s", members_str->str);

        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "No hourly data - retrying... Keys: %s", members_str->str);
        GtkWidget *error_label = gtk_label_new(error_msg);
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);

        g_string_free(members_str, TRUE);
        g_list_free(members_list);
        g_object_unref(parser);
        return FALSE;
    }

    JsonObject *hourly = json_object_get_object_member(root_obj, "hourly");
    if (!hourly) {
        GtkWidget *error_label = gtk_label_new("No hourly data available - retrying...");
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);
        g_object_unref(parser);
        return FALSE;
    }

    JsonArray *time_array = json_object_get_array_member(hourly, "time");
    JsonArray *temp_array = json_object_get_array_member(hourly, "temperature_2m");
    JsonArray *code_array = json_object_get_array_member(hourly, "weathercode");

    if (!time_array || !temp_array || !code_array) {
        GtkWidget *error_label = gtk_label_new("Incomplete weather data - retrying...");
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);
        g_object_unref(parser);
        return FALSE;
    }

    guint array_length = json_array_get_length(time_array);
    const guint hours_to_show = 6;

    int start_index = 0;
    time_t now = time(NULL);
    if (now != (time_t)-1) {
        struct tm *tm_info = localtime(&now);
        if (tm_info) {
            int current_year = tm_info->tm_year + 1900;
            int current_month = tm_info->tm_mon + 1;
            int current_day = tm_info->tm_mday;
            int current_hour = tm_info->tm_hour;

            for (guint i = 0; i < array_length; i++) {
                JsonNode *time_node = json_array_get_element(time_array, i);
                if (!time_node) {
                    continue;
                }

                const gchar *time_str = json_node_get_string(time_node);

                if (time_str && strlen(time_str) >= 16) {
                    int year = atoi(time_str);
                    int month = atoi(time_str + 5);
                    int day = atoi(time_str + 8);
                    int hour = atoi(time_str + 11);

                    gboolean is_current_or_future = FALSE;

                    if (year > current_year) {
                        is_current_or_future = TRUE;
                    } else if (year == current_year && month > current_month) {
                        is_current_or_future = TRUE;
                    } else if (year == current_year && month == current_month && day > current_day) {
                        is_current_or_future = TRUE;
                    } else if (year == current_year && month == current_month && day == current_day) {
                        if (hour >= current_hour) {
                            is_current_or_future = TRUE;
                        }
                    }

                    if (is_current_or_future) {
                        start_index = i;
                        break;
                    }
                }
            }
        }
    }

    for (guint i = 0; i < hours_to_show && (start_index + i) < array_length; i++) {
        guint idx = start_index + i;

        if (idx >= array_length) {
            break;
        }

        JsonNode *time_node = json_array_get_element(time_array, idx);
        JsonNode *temp_node = json_array_get_element(temp_array, idx);
        JsonNode *code_node = json_array_get_element(code_array, idx);

        if (!time_node || !temp_node || !code_node) continue;

        const gchar *time_str = json_node_get_string(time_node);
        if (!time_str) {
            continue;
        }

        gdouble temp = 0.0;
        if (json_node_get_value_type(temp_node) == G_TYPE_DOUBLE) {
            temp = json_node_get_double(temp_node);
        } else if (json_node_get_value_type(temp_node) == G_TYPE_INT64) {
            temp = (gdouble)json_node_get_int(temp_node);
        }

        gint64 code = 0;
        if (json_node_get_value_type(code_node) == G_TYPE_INT64) {
            code = json_node_get_int(code_node);
        } else if (json_node_get_value_type(code_node) == G_TYPE_DOUBLE) {
            code = (gint64)json_node_get_double(code_node);
        }

        GtkWidget *hour_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_widget_add_css_class(hour_box, "weather-hour");

        char hour_str[8];
        if (time_str && strlen(time_str) >= 13) {
            snprintf(hour_str, sizeof(hour_str), "%.2s:00", time_str + 11);
            hour_str[7] = '\0';
        } else {
            strncpy(hour_str, "N/A", sizeof(hour_str) - 1);
            hour_str[sizeof(hour_str) - 1] = '\0';
        }

        GtkWidget *time_label = gtk_label_new(hour_str);
        gtk_widget_add_css_class(time_label, "weather-time");
        gtk_box_append(GTK_BOX(hour_box), time_label);

        GtkWidget *icon_label = gtk_label_new(get_weather_icon((int)code));
        gtk_widget_add_css_class(icon_label, "weather-icon");
        gtk_label_set_xalign(GTK_LABEL(icon_label), 0.5);
        gtk_box_append(GTK_BOX(hour_box), icon_label);

        char temp_str[32];
        int temp_len = snprintf(temp_str, sizeof(temp_str), "%.1f\xc2\xb0""C", temp);
        if (temp_len < 0 || temp_len >= (int)sizeof(temp_str)) {
            strncpy(temp_str, "N/A", sizeof(temp_str) - 1);
            temp_str[sizeof(temp_str) - 1] = '\0';
        }
        GtkWidget *temp_label = gtk_label_new(temp_str);
        gtk_widget_add_css_class(temp_label, "weather-temp");
        gtk_box_append(GTK_BOX(hour_box), temp_label);

        GtkWidget *desc_label = gtk_label_new(get_weather_description((int)code));
        gtk_widget_add_css_class(desc_label, "weather-desc");
        gtk_box_append(GTK_BOX(hour_box), desc_label);

        gtk_box_append(GTK_BOX(data->weather_box), hour_box);
    }

    g_object_unref(parser);
    return TRUE;
}

static gboolean parse_weather_idle(gpointer user_data) {
    WeatherParseData *parse_data = (WeatherParseData *)user_data;
    if (!parse_data) {
        return G_SOURCE_REMOVE;
    }

    AppData *data = parse_data->data;

    if (data && data->session) {
        gboolean success = parse_weather_json(parse_data->json_data, data);

        if (!success && data->retry_count < MAX_RETRY_ATTEMPTS) {
            g_warning("JSON parsing failed, scheduling retry (attempt %d/%d)",
                      data->retry_count + 1, MAX_RETRY_ATTEMPTS);

            data->retry_delay = INITIAL_RETRY_DELAY * (1 << data->retry_count);
            if (data->retry_delay > MAX_RETRY_DELAY) {
                data->retry_delay = MAX_RETRY_DELAY;
            }

            data->is_retrying = TRUE;

            if (data->retry_timer_id != 0) {
                g_source_remove(data->retry_timer_id);
            }
            data->retry_timer_id = g_timeout_add_seconds(data->retry_delay, retry_fetch_weather, data);
            data->retry_count++;
        } else if (!success) {
            g_warning("JSON parsing failed after max retries");
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

    g_free(parse_data->json_data);
    g_free(parse_data);
    return G_SOURCE_REMOVE;
}

static gboolean show_weather_error(gpointer user_data) {
    WeatherParseData *error_data = (WeatherParseData *)user_data;
    if (!error_data) {
        return G_SOURCE_REMOVE;
    }

    AppData *data = error_data->data;
    if (!data || !data->weather_box || !data->session) {
        g_free(error_data);
        return G_SOURCE_REMOVE;
    }

    if (!GTK_IS_BOX(data->weather_box)) {
        g_free(error_data);
        return G_SOURCE_REMOVE;
    }

    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(data->weather_box))) {
        gtk_box_remove(GTK_BOX(data->weather_box), child);
    }

    gchar *error_msg;
    if (data->is_retrying) {
        error_msg = g_strdup_printf("Connection issue - retrying in %d seconds... (attempt %d/%d)",
                                    data->retry_delay, data->retry_count + 1, MAX_RETRY_ATTEMPTS);
    } else {
        error_msg = g_strdup("Failed to fetch weather - will retry at next scheduled update");
    }

    GtkWidget *error_label = gtk_label_new(error_msg);
    gtk_widget_add_css_class(error_label, "error-text");
    gtk_box_append(GTK_BOX(data->weather_box), error_label);

    g_free(error_msg);
    g_free(error_data);
    return G_SOURCE_REMOVE;
}

static void on_weather_response(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    SoupMessage *msg = SOUP_MESSAGE(user_data);
    if (!msg) {
        g_warning("Invalid message in weather response callback");
        return;
    }

    AppData *data = (AppData *)g_object_get_data(G_OBJECT(msg), "app-data");

    if (data && data->pending_message == msg) {
        data->pending_message = NULL;
    }

    if (!data || !data->session) {
        g_warning("AppData not found or invalid in message user data");
        g_object_unref(msg);
        return;
    }

    GError *error = NULL;
    GBytes *body_bytes = soup_session_send_and_read_finish(SOUP_SESSION(source_object), res, &error);

    if (error) {
        g_warning("Weather fetch error: %s (attempt %d/%d)", error->message, data->retry_count + 1, MAX_RETRY_ATTEMPTS);

        gboolean should_retry = (data->retry_count < MAX_RETRY_ATTEMPTS);

        if (should_retry) {
            data->retry_delay = INITIAL_RETRY_DELAY * (1 << data->retry_count);
            if (data->retry_delay > MAX_RETRY_DELAY) {
                data->retry_delay = MAX_RETRY_DELAY;
            }

            data->is_retrying = TRUE;

            WeatherParseData *error_data = g_new0(WeatherParseData, 1);
            if (error_data) {
                error_data->data = data;
                error_data->json_data = NULL;
                g_idle_add(show_weather_error, error_data);
            }

            g_info("Scheduling retry in %d seconds...", data->retry_delay);
            if (data->retry_timer_id != 0) {
                g_source_remove(data->retry_timer_id);
            }
            data->retry_timer_id = g_timeout_add_seconds(data->retry_delay, retry_fetch_weather, data);

            data->retry_count++;
        } else {
            g_warning("Max retry attempts (%d) exceeded. Will retry at next scheduled update.", MAX_RETRY_ATTEMPTS);
            data->is_retrying = FALSE;
            data->retry_count = 0;
            data->retry_delay = 0;

            WeatherParseData *error_data = g_new0(WeatherParseData, 1);
            if (error_data) {
                error_data->data = data;
                error_data->json_data = NULL;
                g_idle_add(show_weather_error, error_data);
            }
        }

        g_error_free(error);
        g_object_unref(msg);
        return;
    }

    if (!body_bytes) {
        g_warning("No body bytes received (attempt %d/%d)", data->retry_count + 1, MAX_RETRY_ATTEMPTS);

        if (data->retry_count < MAX_RETRY_ATTEMPTS) {
            data->retry_delay = INITIAL_RETRY_DELAY * (1 << data->retry_count);
            if (data->retry_delay > MAX_RETRY_DELAY) {
                data->retry_delay = MAX_RETRY_DELAY;
            }

            data->is_retrying = TRUE;
            WeatherParseData *error_data = g_new0(WeatherParseData, 1);
            if (error_data) {
                error_data->data = data;
                error_data->json_data = NULL;
                g_idle_add(show_weather_error, error_data);
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

            WeatherParseData *error_data = g_new0(WeatherParseData, 1);
            if (error_data) {
                error_data->data = data;
                error_data->json_data = NULL;
                g_idle_add(show_weather_error, error_data);
            }
        }

        g_object_unref(msg);
        return;
    }

    gsize length;
    const gchar *response_body = (const gchar *)g_bytes_get_data(body_bytes, &length);
    if (response_body && length > 0) {
        if (data->retry_count > 0) {
            g_info("Weather fetch succeeded after %d retry attempt(s)", data->retry_count);
        }
        data->retry_count = 0;
        data->retry_delay = 0;
        data->is_retrying = FALSE;
        if (data->retry_timer_id != 0) {
            g_source_remove(data->retry_timer_id);
            data->retry_timer_id = 0;
        }

        gchar *preview = g_strndup(response_body, length > 500 ? 500 : length);
        if (preview) {
            g_debug("Weather API response (first 500 chars): %s", preview);
            g_free(preview);
        }

        WeatherParseData *parse_data = g_new0(WeatherParseData, 1);
        parse_data->data = data;
        parse_data->json_data = g_strndup(response_body, length);

        if (!parse_data->json_data) {
            g_warning("Failed to allocate memory for JSON data");
            g_free(parse_data);
        } else {
            g_idle_add(parse_weather_idle, parse_data);
        }
    } else {
        g_warning("Empty response body (attempt %d/%d)", data->retry_count + 1, MAX_RETRY_ATTEMPTS);

        if (data->retry_count < MAX_RETRY_ATTEMPTS) {
            data->retry_delay = INITIAL_RETRY_DELAY * (1 << data->retry_count);
            if (data->retry_delay > MAX_RETRY_DELAY) {
                data->retry_delay = MAX_RETRY_DELAY;
            }

            data->is_retrying = TRUE;
            WeatherParseData *error_data = g_new0(WeatherParseData, 1);
            if (error_data) {
                error_data->data = data;
                error_data->json_data = NULL;
                g_idle_add(show_weather_error, error_data);
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

            WeatherParseData *error_data = g_new0(WeatherParseData, 1);
            if (error_data) {
                error_data->data = data;
                error_data->json_data = NULL;
                g_idle_add(show_weather_error, error_data);
            }
        }
    }

    g_bytes_unref(body_bytes);
    g_object_unref(msg);
}

static gboolean retry_fetch_weather(gpointer user_data) {
    AppData *data = (AppData *)user_data;
    if (!data) {
        return G_SOURCE_REMOVE;
    }

    if (!data->session) {
        return G_SOURCE_REMOVE;
    }

    data->retry_timer_id = 0;

    g_info("Retrying weather fetch (attempt %d/%d)...", data->retry_count + 1, MAX_RETRY_ATTEMPTS);
    fetch_weather(data);

    return G_SOURCE_REMOVE;
}

void fetch_weather(AppData *data) {
    if (!data || !data->session) {
        return;
    }

    if (data->pending_message) {
        g_object_unref(data->pending_message);
        data->pending_message = NULL;
    }

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

    char url[512];
    int url_len = snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&hourly=temperature_2m,weathercode&forecast_days=2&timezone=auto",
             lat, lon);

    if (url_len < 0 || url_len >= (int)sizeof(url)) {
        g_warning("URL construction failed or truncated");
        return;
    }

    g_debug("Fetching weather from: %s", url);

    SoupMessage *msg = soup_message_new("GET", url);
    if (!msg) {
        g_warning("Failed to create SoupMessage");
        return;
    }

    g_object_set_data(G_OBJECT(msg), "app-data", data);
    data->pending_message = msg;
    g_object_ref(msg);
    soup_session_send_and_read_async(data->session, msg, G_PRIORITY_DEFAULT, NULL, on_weather_response, msg);
}

gboolean update_weather_callback(gpointer user_data) {
    AppData *data = (AppData *)user_data;
    if (!data) {
        return G_SOURCE_REMOVE;
    }

    if (!data->session) {
        return G_SOURCE_REMOVE;
    }

    fetch_weather(data);

    if (data->weather_timer_id != 0) {
        g_source_remove(data->weather_timer_id);
        data->weather_timer_id = 0;
    }

    data->weather_timer_id = g_timeout_add_seconds(UPDATE_INTERVAL_SECONDS, update_weather_callback, data);

    return G_SOURCE_REMOVE;
}
