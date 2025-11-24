#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include <json-glib/json-gobject.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <libsoup/soup.h>
#include <libsoup/soup-message-body.h>

#define WEATHER_API_URL "https://api.open-meteo.com/v1/forecast?latitude=52.52&longitude=13.41&hourly=temperature_2m,weathercode&forecast_days=1"
#define UPDATE_INTERVAL_SECONDS 3600  // 1 hour in seconds
#define CONFIG_FILE_NAME "weatherclock.conf"

typedef struct {
    GtkWidget *window;
    GtkWidget *clock_label;
    GtkWidget *date_label;
    GtkWidget *weather_box;
    GtkWidget *lat_entry;
    GtkWidget *lon_entry;
    SoupSession *session;
    gchar *location_lat;
    gchar *location_lon;
} AppData;

// Weather code to description mapping
static const char* get_weather_description(int code) {
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

// Weather code to emoji/icon
static const char* get_weather_icon(int code) {
    if (code == 0) return "☀️";
    if (code <= 3) return "⛅";
    if (code <= 49) return "🌫️";
    if (code <= 59) return "🌦️";
    if (code <= 69) return "🌧️";
    if (code <= 79) return "❄️";
    if (code <= 84) return "🌦️";
    if (code <= 86) return "❄️";
    if (code <= 99) return "⛈️";
    return "❓";
}

static void update_clock(AppData *data) {
    if (!data || !data->clock_label || !data->date_label) {
        return;
    }
    
    time_t now = time(NULL);
    if (now == (time_t)-1) {
        return; // time() failed
    }
    
    struct tm *tm_info = localtime(&now);
    if (!tm_info) {
        return; // localtime() failed
    }
    
    char time_str[64];
    char date_str[64];
    
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
    strftime(date_str, sizeof(date_str), "%A, %B %d, %Y", tm_info);
    
    gtk_label_set_text(GTK_LABEL(data->clock_label), time_str);
    gtk_label_set_text(GTK_LABEL(data->date_label), date_str);
}

static void parse_weather_json(const char *json_data_str, AppData *data) {
    if (!data || !data->weather_box) {
        return;
    }
    
    // Clear existing weather widgets
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(data->weather_box))) {
        gtk_box_remove(GTK_BOX(data->weather_box), child);
    }
    
    if (!json_data_str || strlen(json_data_str) == 0) {
        GtkWidget *error_label = gtk_label_new("Empty weather data received");
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);
        return;
    }
    
    // Debug: show first part of response
    g_debug("JSON response length: %zu", strlen(json_data_str));
    if (strlen(json_data_str) < 500) {
        g_debug("Full JSON: %s", json_data_str);
    } else {
        gchar *preview = g_strndup(json_data_str, 500);
        g_debug("JSON preview: %s...", preview);
        g_free(preview);
    }
    
    JsonParser *parser = json_parser_new();
    GError *error = NULL;
    
    if (!json_parser_load_from_data(parser, json_data_str, -1, &error)) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Parse error: %s", error ? error->message : "Unknown");
        GtkWidget *error_label = gtk_label_new(error_msg);
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);
        if (error) g_error_free(error);
        g_object_unref(parser);
        return;
    }
    
    JsonNode *root = json_parser_get_root(parser);
    if (!root) {
        GtkWidget *error_label = gtk_label_new("Invalid JSON: no root node");
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);
        g_object_unref(parser);
        return;
    }
    
    JsonObject *root_obj = json_node_get_object(root);
    if (!root_obj) {
        GtkWidget *error_label = gtk_label_new("Invalid weather data format");
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);
        g_object_unref(parser);
        return;
    }
    
    // Check for API errors first - Open-Meteo returns "error" as boolean or "reason" as string
    if (json_object_has_member(root_obj, "error")) {
        JsonNode *error_node = json_object_get_member(root_obj, "error");
        const gchar *error_msg = NULL;
        
        // Show all available keys for debugging first
        GList *members_list = json_object_get_members(root_obj);
        GString *keys_str = g_string_new("");
        GList *iter;
        for (iter = members_list; iter != NULL; iter = iter->next) {
            if (keys_str->len > 0) {
                g_string_append(keys_str, ", ");
            }
            g_string_append(keys_str, (const gchar *)iter->data);
        }
        
        // Check if error is true/false boolean
        GType error_type = json_node_get_value_type(error_node);
        if (error_type == G_TYPE_BOOLEAN) {
            gboolean error_bool = json_node_get_boolean(error_node);
            if (error_bool) {
                // Error is true, check for reason field
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
            // Error is a string
            error_msg = json_node_get_string(error_node);
        } else {
            // Unknown error type
            char type_str[64];
            snprintf(type_str, sizeof(type_str), "Error type: %s", g_type_name(error_type));
            error_msg = type_str;
        }
        
        char error_text[512];
        if (error_msg) {
            snprintf(error_text, sizeof(error_text), "API Error: %s (Keys: %s)", error_msg, keys_str->str);
        } else {
            snprintf(error_text, sizeof(error_text), "API Error: Unknown (Keys: %s)", keys_str->str);
        }
        
        GtkWidget *error_label = gtk_label_new(error_text);
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);
        
        g_string_free(keys_str, TRUE);
        g_list_free(members_list);
        g_object_unref(parser);
        return;
    }
    
    if (!json_object_has_member(root_obj, "hourly")) {
        // Debug: print all keys in root object
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
        snprintf(error_msg, sizeof(error_msg), "No hourly data. Keys: %s", members_str->str);
        GtkWidget *error_label = gtk_label_new(error_msg);
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);
        
        g_string_free(members_str, TRUE);
        g_list_free(members_list);
        g_object_unref(parser);
        return;
    }
    
    JsonObject *hourly = json_object_get_object_member(root_obj, "hourly");
    if (!hourly) {
        GtkWidget *error_label = gtk_label_new("No hourly data available");
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);
        g_object_unref(parser);
        return;
    }
    
    JsonArray *time_array = json_object_get_array_member(hourly, "time");
    JsonArray *temp_array = json_object_get_array_member(hourly, "temperature_2m");
    JsonArray *code_array = json_object_get_array_member(hourly, "weathercode");
    
    if (!time_array || !temp_array || !code_array) {
        GtkWidget *error_label = gtk_label_new("Incomplete weather data");
        gtk_widget_add_css_class(error_label, "error-text");
        gtk_box_append(GTK_BOX(data->weather_box), error_label);
        g_object_unref(parser);
        return;
    }
    
    guint array_length = json_array_get_length(time_array);
    guint hours_to_show = (array_length > 6) ? 6 : array_length;
    
    // Get current time to show only future hours
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    int current_hour = tm_info->tm_hour;
    int start_index = 0;
    
    // Find the first future hour
    for (guint i = 0; i < array_length; i++) {
        JsonNode *time_node = json_array_get_element(time_array, i);
        if (!time_node) {
            continue;
        }
        
        const gchar *time_str = json_node_get_string(time_node);
        
        if (time_str && strlen(time_str) >= 16) {
            // Ensure we have enough characters: "YYYY-MM-DDTHH:MM" format
            int hour = atoi(time_str + 11);
            int minute = 0;
            if (strlen(time_str) >= 16) {
                minute = atoi(time_str + 14);
            }
            if (hour > current_hour || (hour == current_hour && minute >= tm_info->tm_min)) {
                start_index = i;
                break;
            }
        }
    }
    
    // Create weather display widgets for next 6 hours
    for (guint i = 0; i < hours_to_show && (start_index + i) < array_length; i++) {
        guint idx = start_index + i;
        
        // Bounds check (defensive programming)
        if (idx >= array_length) {
            break;
        }
        
        JsonNode *time_node = json_array_get_element(time_array, idx);
        JsonNode *temp_node = json_array_get_element(temp_array, idx);
        JsonNode *code_node = json_array_get_element(code_array, idx);
        
        if (!time_node || !temp_node || !code_node) continue;
        
        const gchar *time_str = json_node_get_string(time_node);
        if (!time_str) {
            continue; // Skip invalid time
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
        gtk_widget_set_hexpand(hour_box, TRUE);
        gtk_widget_set_halign(hour_box, GTK_ALIGN_FILL);
        
        // Time label (extract hour from ISO time string)
        char hour_str[8];
        if (time_str && strlen(time_str) >= 13) {
            snprintf(hour_str, sizeof(hour_str), "%.2s:00", time_str + 11);
            hour_str[7] = '\0'; // Ensure null termination
        } else {
            strncpy(hour_str, "N/A", sizeof(hour_str) - 1);
            hour_str[sizeof(hour_str) - 1] = '\0';
        }
        
        GtkWidget *time_label = gtk_label_new(hour_str);
        gtk_widget_add_css_class(time_label, "weather-time");
        gtk_box_append(GTK_BOX(hour_box), time_label);
        
        // Weather icon
        GtkWidget *icon_label = gtk_label_new(get_weather_icon((int)code));
        gtk_widget_add_css_class(icon_label, "weather-icon");
        gtk_label_set_xalign(GTK_LABEL(icon_label), 0.5); // Center horizontally
        gtk_box_append(GTK_BOX(hour_box), icon_label);
        
        // Temperature
        char temp_str[32];
        int temp_len = snprintf(temp_str, sizeof(temp_str), "%.1f°C", temp);
        if (temp_len < 0 || temp_len >= (int)sizeof(temp_str)) {
            strncpy(temp_str, "N/A", sizeof(temp_str) - 1);
            temp_str[sizeof(temp_str) - 1] = '\0';
        }
        GtkWidget *temp_label = gtk_label_new(temp_str);
        gtk_widget_add_css_class(temp_label, "weather-temp");
        gtk_box_append(GTK_BOX(hour_box), temp_label);
        
        // Description
        GtkWidget *desc_label = gtk_label_new(get_weather_description((int)code));
        gtk_widget_add_css_class(desc_label, "weather-desc");
        gtk_box_append(GTK_BOX(hour_box), desc_label);
        
        gtk_box_append(GTK_BOX(data->weather_box), hour_box);
    }
    
    g_object_unref(parser);
}

typedef struct {
    AppData *data;
    gchar *json_data;
} WeatherParseData;

static gboolean parse_weather_idle(gpointer user_data) {
    WeatherParseData *parse_data = (WeatherParseData *)user_data;
    if (!parse_data) {
        return G_SOURCE_REMOVE;
    }
    
    parse_weather_json(parse_data->json_data, parse_data->data);
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
    if (!data || !data->weather_box) {
        g_free(error_data);
        return G_SOURCE_REMOVE;
    }
    
    // Clear existing widgets
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(data->weather_box))) {
        gtk_box_remove(GTK_BOX(data->weather_box), child);
    }
    
    GtkWidget *error_label = gtk_label_new("Failed to fetch weather");
    gtk_widget_add_css_class(error_label, "error-text");
    gtk_box_append(GTK_BOX(data->weather_box), error_label);
    
    g_free(error_data);
    return G_SOURCE_REMOVE;
}

static void on_weather_response(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    SoupMessage *msg = SOUP_MESSAGE(user_data);
    AppData *data = (AppData *)g_object_get_data(G_OBJECT(msg), "app-data");
    
    if (!data) {
        g_warning("AppData not found in message user data");
        g_object_unref(msg);
        return;
    }
    
    GError *error = NULL;
    GBytes *body_bytes = soup_session_send_and_read_finish(SOUP_SESSION(source_object), res, &error);
    
    if (error) {
        // Error occurred
        g_warning("Weather fetch error: %s", error->message);
        WeatherParseData *error_data = g_new0(WeatherParseData, 1);
        error_data->data = data;
        error_data->json_data = NULL; // Signal error
        g_idle_add(show_weather_error, error_data);
        g_error_free(error);
        g_object_unref(msg);
        return;
    }
    
    if (!body_bytes) {
        g_warning("No body bytes received");
        WeatherParseData *error_data = g_new0(WeatherParseData, 1);
        error_data->data = data;
        error_data->json_data = NULL;
        g_idle_add(show_weather_error, error_data);
        g_object_unref(msg);
        return;
    }
    
    // Success - get the data
    gsize length;
    const gchar *response_body = (const gchar *)g_bytes_get_data(body_bytes, &length);
    if (response_body && length > 0) {
        // Debug: print first 500 chars of response
        gchar *preview = g_strndup(response_body, length > 500 ? 500 : length);
        g_debug("Weather API response (first 500 chars): %s", preview);
        g_free(preview);
        
        WeatherParseData *parse_data = g_new0(WeatherParseData, 1);
        parse_data->data = data;
        parse_data->json_data = g_strndup(response_body, length);
        g_idle_add(parse_weather_idle, parse_data);
    } else {
        g_warning("Empty response body");
        WeatherParseData *error_data = g_new0(WeatherParseData, 1);
        error_data->data = data;
        error_data->json_data = NULL;
        g_idle_add(show_weather_error, error_data);
    }
    
    g_bytes_unref(body_bytes);
    g_object_unref(msg);
}

// Forward declaration
static void fetch_weather(AppData *data);

static gchar* get_config_file_path(void) {
    // Get config directory (user's home directory or current directory)
    const gchar *home_dir = g_get_home_dir();
    if (home_dir) {
        return g_build_filename(home_dir, CONFIG_FILE_NAME, NULL);
    }
    return g_strdup(CONFIG_FILE_NAME);
}

static void save_location_to_config(AppData *data) {
    if (!data || !data->location_lat || !data->location_lon) {
        return;
    }
    
    gchar *config_path = get_config_file_path();
    if (!config_path) {
        return;
    }
    
    GKeyFile *key_file = g_key_file_new();
    g_key_file_set_string(key_file, "Location", "latitude", data->location_lat);
    g_key_file_set_string(key_file, "Location", "longitude", data->location_lon);
    
    GError *error = NULL;
    if (!g_key_file_save_to_file(key_file, config_path, &error)) {
        g_warning("Failed to save config: %s", error ? error->message : "Unknown error");
        if (error) {
            g_error_free(error);
        }
    }
    
    g_key_file_unref(key_file);
    g_free(config_path);
}

static void load_location_from_config(AppData *data) {
    if (!data) {
        return;
    }
    
    gchar *config_path = get_config_file_path();
    if (!config_path) {
        return;
    }
    
    // Check if config file exists
    if (!g_file_test(config_path, G_FILE_TEST_EXISTS)) {
        g_free(config_path);
        return; // No config file, use defaults
    }
    
    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;
    
    if (!g_key_file_load_from_file(key_file, config_path, G_KEY_FILE_NONE, &error)) {
        g_warning("Failed to load config: %s", error ? error->message : "Unknown error");
        if (error) {
            g_error_free(error);
        }
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
    
    if (error) {
        g_error_free(error);
        error = NULL;
    }
    
    gchar *lon = g_key_file_get_string(key_file, "Location", "longitude", &error);
    if (lon && strlen(lon) > 0) {
        g_free(data->location_lon);
        data->location_lon = lon;
    } else {
        g_free(lon);
    }
    
    if (error) {
        g_error_free(error);
    }
    
    g_key_file_unref(key_file);
    g_free(config_path);
}

static void update_location_from_entries(AppData *data) {
    if (!data) {
        return;
    }
    
    if (!data->lat_entry || !data->lon_entry) {
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
    
    // Save to config file
    save_location_to_config(data);
}

static void on_location_update(GtkWidget *widget, gpointer user_data) {
    (void)widget; // Suppress unused parameter warning
    AppData *data = (AppData *)user_data;
    update_location_from_entries(data);
    fetch_weather(data);
}

static void on_exit_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget; // Suppress unused parameter warning
    GtkApplication *app = (GtkApplication *)user_data;
    g_application_quit(G_APPLICATION(app));
}

static void fetch_weather(AppData *data) {
    if (!data || !data->session) {
        return;
    }
    
    // Get location from entries if available, otherwise use stored values
    const gchar *lat = data->location_lat ? data->location_lat : "52.52";
    const gchar *lon = data->location_lon ? data->location_lon : "13.41";
    
    if (data->lat_entry) {
        const gchar *lat_text = gtk_editable_get_text(GTK_EDITABLE(data->lat_entry));
        if (lat_text && strlen(lat_text) > 0) {
            lat = lat_text;
        }
    }
    if (data->lon_entry) {
        const gchar *lon_text = gtk_editable_get_text(GTK_EDITABLE(data->lon_entry));
        if (lon_text && strlen(lon_text) > 0) {
            lon = lon_text;
        }
    }
    
    // Validate lat/lon are reasonable (prevent buffer overflow in URL)
    if (strlen(lat) > 20 || strlen(lon) > 20) {
        g_warning("Latitude or longitude too long, using defaults");
        lat = "52.52";
        lon = "13.41";
    }
    
    char url[512];
    int url_len = snprintf(url, sizeof(url), 
             "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&hourly=temperature_2m,weathercode&forecast_days=1",
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
    
    // Store data pointer in message user_data for callback
    g_object_set_data(G_OBJECT(msg), "app-data", data);
    soup_session_send_and_read_async(data->session, msg, G_PRIORITY_DEFAULT, NULL, on_weather_response, msg);
}

static gboolean update_clock_callback(gpointer user_data) {
    AppData *data = (AppData *)user_data;
    update_clock(data);
    return G_SOURCE_CONTINUE;
}

static gboolean update_weather_callback(gpointer user_data) {
    AppData *data = (AppData *)user_data;
    fetch_weather(data);
    return G_SOURCE_CONTINUE;
}

static void activate(GtkApplication *app, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    
    // Create main window
    data->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(data->window), "Weather Clock");
    gtk_window_set_default_size(GTK_WINDOW(data->window), 800, 600);
    gtk_window_fullscreen(GTK_WINDOW(data->window));
    
    // Set window background to black
    gtk_widget_set_name(data->window, "main-window");
    
    // Main container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_top(main_box, 40);
    gtk_widget_set_margin_bottom(main_box, 40);
    gtk_widget_set_margin_start(main_box, 40);
    gtk_widget_set_margin_end(main_box, 40);
    gtk_window_set_child(GTK_WINDOW(data->window), main_box);
    
    // Exit button at the top
    GtkWidget *exit_btn = gtk_button_new_with_label("Exit");
    gtk_widget_set_halign(exit_btn, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(exit_btn, 10);
    gtk_widget_add_css_class(exit_btn, "exit-button");
    g_signal_connect(exit_btn, "clicked", G_CALLBACK(on_exit_clicked), app);
    gtk_box_append(GTK_BOX(main_box), exit_btn);
    
    // Clock section
    GtkWidget *clock_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
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
    
    // Location input section
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
    
    // Weather section
    GtkWidget *weather_section = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_widget_add_css_class(weather_section, "weather-section");
    
    GtkWidget *weather_title = gtk_label_new("Hourly Weather Forecast");
    gtk_widget_add_css_class(weather_title, "weather-title");
    gtk_box_append(GTK_BOX(weather_section), weather_title);
    
    // Scrollable weather container
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    
    data->weather_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_add_css_class(data->weather_box, "weather-container");
    gtk_widget_set_halign(data->weather_box, GTK_ALIGN_CENTER);
    gtk_box_set_homogeneous(GTK_BOX(data->weather_box), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), data->weather_box);
    
    gtk_box_append(GTK_BOX(weather_section), scrolled);
    gtk_box_append(GTK_BOX(main_box), weather_section);
    
    // CSS styling
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const char *css = 
        "#main-window {"
        "  background-color: #000000;"
        "}"
        "window {"
        "  background-color: #000000;"
        "}"
        ".clock-time {"
        "  font-size: 400px;"
        "  font-weight: bold;"
        "  color: #ffffff;"
        "}"
        ".clock-date {"
        "  font-size: 100px;"
        "  color: #ffffff;"
        "}"
        ".weather-section {"
        "  background-color: rgba(20, 20, 20, 0.9);"
        "  border-radius: 15px;"
        "  padding: 20px;"
        "}"
        ".weather-title {"
        "  font-size: 28px;"
        "  font-weight: bold;"
        "  color: #ffffff;"
        "  margin-bottom: 10px;"
        "}"
        ".weather-container {"
        "  padding: 10px;"
        "}"
        ".weather-hour {"
        "  background-color: rgba(40, 40, 40, 0.9);"
        "  border-radius: 10px;"
        "  padding: 15px;"
        "  margin: 5px;"
        "}"
        ".weather-time {"
        "  font-size: 36px;"
        "  font-weight: bold;"
        "  color: #ffffff;"
        "}"
        ".weather-icon {"
        "  font-size: 48px;"
        "}"
        ".weather-temp {"
        "  font-size: 72px;"
        "  font-weight: bold;"
        "  color: #ffffff;"
        "}"
        ".weather-desc {"
        "  font-size: 14px;"
        "  color: #cccccc;"
        "}"
        ".error-text {"
        "  color: #ff6b6b;"
        "  font-size: 18px;"
        "}"
        ".location-box {"
        "  padding: 10px;"
        "  margin-bottom: 10px;"
        "}"
        ".location-box label {"
        "  margin: 0 5px;"
        "  color: #ffffff;"
        "}"
        ".location-box entry {"
        "  min-width: 100px;"
        "  margin: 0 10px;"
        "  background-color: #1a1a1a;"
        "  color: #ffffff;"
        "}"
        ".exit-button {"
        "  padding: 10px 20px;"
        "  font-size: 16px;"
        "  background-color: #bf616a;"
        "  color: #000000;"
        "  border-radius: 5px;"
        "}"
        ".exit-button:hover {"
        "  background-color: #a04850;"
        "}";
    
    gtk_css_provider_load_from_string(css_provider, css);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(css_provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    // Initialize clock
    update_clock(data);
    
    // Set up timers
    g_timeout_add_seconds(1, update_clock_callback, data);
    g_timeout_add_seconds(UPDATE_INTERVAL_SECONDS, update_weather_callback, data);
    
    // Fetch initial weather
    fetch_weather(data);
    
    gtk_widget_set_visible(data->window, TRUE);
}

int main(int argc, char *argv[]) {
    AppData *data = g_new0(AppData, 1);
    if (!data) {
        g_error("Failed to allocate AppData");
        return 1;
    }
    
    // Initialize location (default: Toronto)
    // Can be overridden with command line arguments or config file
    data->location_lat = g_strdup("43.640");
    data->location_lon = g_strdup("-79.565");
    
    if (!data->location_lat || !data->location_lon) {
        g_error("Failed to allocate location strings");
        g_free(data->location_lat);
        g_free(data->location_lon);
        g_free(data);
        return 1;
    }
    
    // Load location from config file (if exists)
    load_location_from_config(data);
    
    // Command line arguments override config file
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
        
        // Save command line arguments to config
        save_location_to_config(data);
    }
    
    // Create Soup session for HTTP requests
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
    
    g_object_unref(app);
    if (data->session) {
        g_object_unref(data->session);
    }
    g_free(data->location_lat);
    g_free(data->location_lon);
    g_free(data);
    
    return status;
}

