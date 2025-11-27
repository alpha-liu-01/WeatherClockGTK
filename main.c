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
#define MAX_RETRY_ATTEMPTS 5          // Maximum number of retry attempts
#define INITIAL_RETRY_DELAY 30        // Initial retry delay in seconds (30s)
#define MAX_RETRY_DELAY 600           // Maximum retry delay in seconds (10 minutes)

typedef struct {
    GtkWidget *window;
    GtkWidget *settings_window;     // Settings/preferences window
    GtkWidget *clock_label;
    GtkWidget *date_label;
    GtkWidget *weather_box;
    GtkWidget *lat_entry;
    GtkWidget *lon_entry;
    SoupSession *session;
    SoupMessage *pending_message;  // Track pending HTTP request to cancel on exit
    GtkCssProvider *css_provider;   // Track CSS provider for cleanup
    guint clock_timer_id;           // Track clock update timer
    guint weather_timer_id;         // Track weather update timer
    guint retry_timer_id;           // Track retry timer
    gchar *location_lat;
    gchar *location_lon;
    gchar *timezone;  // IANA timezone (e.g., "America/Toronto")
    GTimeZone *tz;    // GTimeZone object for time conversion
    gint utc_offset_seconds;  // UTC offset in seconds (fallback if timezone creation fails)
    gint retry_count;          // Current retry attempt count
    gint retry_delay;          // Current retry delay in seconds
    gboolean is_retrying;      // Flag to indicate if we're in retry mode
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
    
    // Verify widgets are valid GTK objects and actually labels before casting
    if (!GTK_IS_WIDGET(data->clock_label) || !GTK_IS_WIDGET(data->date_label)) {
        return;
    }
    if (!GTK_IS_LABEL(data->clock_label) || !GTK_IS_LABEL(data->date_label)) {
        return;
    }
    
    // Ensure widgets are part of widget tree (have parents) before updating
    if (!gtk_widget_get_parent(data->clock_label) || !gtk_widget_get_parent(data->date_label)) {
        return;
    }
    
    time_t now = time(NULL);
    if (now == (time_t)-1) {
        return; // time() failed
    }
    
    GDateTime *dt = NULL;
    
    // PRIORITY 1: Use UTC offset (most reliable on Windows deployments)
    // The utc_offset_seconds comes directly from the weather API and doesn't
    // depend on having timezone database files installed
    if (data->utc_offset_seconds != 0) {
        // Convert UTC time to local by adding the offset
        // For GMT-5: offset = -18000 seconds, so we subtract 5 hours from UTC
        time_t adjusted_time = now + data->utc_offset_seconds;
        dt = g_date_time_new_from_unix_utc(adjusted_time);
    }
    // PRIORITY 2: Try GTimeZone object (may not work on Windows without tz database)
    else if (data->tz) {
        dt = g_date_time_new_from_unix_utc(now);
        if (dt) {
            GDateTime *dt_tz = g_date_time_to_timezone(dt, data->tz);
            g_date_time_unref(dt);
            dt = dt_tz;
        }
    }
    // PRIORITY 3: Fallback to system local time
    else {
        dt = g_date_time_new_from_unix_local(now);
    }
    
    if (!dt) {
        return;
    }
    
    gchar *time_str = g_date_time_format(dt, "%H:%M:%S");
    gchar *date_str = g_date_time_format(dt, "%A, %B %d, %Y");
    
    if (time_str) {
        gtk_label_set_text(GTK_LABEL(data->clock_label), time_str);
        g_free(time_str);
    }
    if (date_str) {
        gtk_label_set_text(GTK_LABEL(data->date_label), date_str);
        g_free(date_str);
    }
    
    g_date_time_unref(dt);
}

// Forward declarations
static void save_location_to_config(AppData *data);

static void parse_weather_json(const char *json_data_str, AppData *data) {
    if (!data || !data->weather_box) {
        return;
    }
    
    // Clear existing weather widgets
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(data->weather_box))) {
        gtk_box_remove(GTK_BOX(data->weather_box), child);
        // Widget is automatically unparented and will be destroyed when last reference drops
        // GTK widgets use reference counting, so explicit unref is usually not needed
        // but we can add it for extra safety if needed
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
        if (preview) {
            g_debug("JSON preview: %s...", preview);
            g_free(preview);
        }
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
    
    // Extract timezone from API response
    if (json_object_has_member(root_obj, "timezone")) {
        JsonNode *tz_node = json_object_get_member(root_obj, "timezone");
        const gchar *tz_str = json_node_get_string(tz_node);
        if (tz_str && strlen(tz_str) > 0) {
            // Update timezone
            if (data->tz) {
                g_time_zone_unref(data->tz);
                data->tz = NULL;
            }
            g_free(data->timezone);
            data->timezone = g_strdup(tz_str);
            data->tz = g_time_zone_new_identifier(tz_str);
            if (!data->tz) {
                // Fallback: try the deprecated function (might work on some systems)
                // Suppress deprecation warning since this is an intentional fallback
                G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                data->tz = g_time_zone_new(tz_str);
                G_GNUC_END_IGNORE_DEPRECATIONS
                if (!data->tz) {
                    g_debug("Failed to create timezone for: %s, will use UTC offset if available", tz_str);
                } else {
                    g_debug("Set timezone to: %s (using deprecated API)", tz_str);
                    save_location_to_config(data);
                }
            } else {
                g_debug("Set timezone to: %s", tz_str);
                // Save timezone to config
                save_location_to_config(data);
            }
        }
    }
    
    // Extract UTC offset as fallback (in seconds)
    if (json_object_has_member(root_obj, "utc_offset_seconds")) {
        JsonNode *offset_node = json_object_get_member(root_obj, "utc_offset_seconds");
        GType offset_type = json_node_get_value_type(offset_node);
        
        // Handle different numeric types the API might return
        if (offset_type == G_TYPE_INT64) {
            gint64 offset = json_node_get_int(offset_node);
            data->utc_offset_seconds = (gint)offset;
        } else if (offset_type == G_TYPE_DOUBLE) {
            gdouble offset = json_node_get_double(offset_node);
            data->utc_offset_seconds = (gint)offset;
        } else if (JSON_NODE_HOLDS_VALUE(offset_node)) {
            // Try to get as int anyway
            data->utc_offset_seconds = (gint)json_node_get_int(offset_node);
        }
        
        g_print("UTC offset set: %d seconds (%+.1f hours)\n", 
                data->utc_offset_seconds, data->utc_offset_seconds / 3600.0);
        
        // Save config immediately after getting UTC offset
        save_location_to_config(data);
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
            // Unknown error type - set to NULL, we'll format it directly below
            error_msg = NULL;
        }
        
        char error_text[512];
        if (error_msg) {
            snprintf(error_text, sizeof(error_text), "API Error: %s (Keys: %s)", error_msg, keys_str->str);
        } else if (error_type != G_TYPE_BOOLEAN && error_type != G_TYPE_STRING) {
            // Format unknown error type directly into error_text
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
    const guint hours_to_show = 6;  // Always show 6 hours
    
    // Get current time to find the starting hour
    int start_index = 0;
    time_t now = time(NULL);
    if (now != (time_t)-1) {
        struct tm *tm_info = localtime(&now);
        if (tm_info) {
            int current_year = tm_info->tm_year + 1900;
            int current_month = tm_info->tm_mon + 1;
            int current_day = tm_info->tm_mday;
            int current_hour = tm_info->tm_hour;
            
            // Find the first hour >= current hour (same day or next day)
            for (guint i = 0; i < array_length; i++) {
                JsonNode *time_node = json_array_get_element(time_array, i);
                if (!time_node) {
                    continue;
                }
                
                const gchar *time_str = json_node_get_string(time_node);
                
                if (time_str && strlen(time_str) >= 16) {
                    // Parse date and time: "YYYY-MM-DDTHH:MM" format
                    int year = atoi(time_str);
                    int month = atoi(time_str + 5);
                    int day = atoi(time_str + 8);
                    int hour = atoi(time_str + 11);
                    
                    // Find the current hour (or next hour if we've passed it)
                    // We want to start from the current hour and show 6 hours total
                    gboolean is_current_or_future = FALSE;
                    
                    if (year > current_year) {
                        is_current_or_future = TRUE;
                    } else if (year == current_year && month > current_month) {
                        is_current_or_future = TRUE;
                    } else if (year == current_year && month == current_month && day > current_day) {
                        is_current_or_future = TRUE;
                    } else if (year == current_year && month == current_month && day == current_day) {
                        // Same day: start from current hour (hour >= current_hour)
                        // This ensures we show the current hour even if it's partially passed
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
    
    // Create weather display widgets for next 6 hours
    // Always show exactly 6 hours, even if we need to go into the next day
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
    
    // Safety check: ensure data and session are still valid
    if (parse_data->data && parse_data->data->session) {
        parse_weather_json(parse_data->json_data, parse_data->data);
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
    
    // Additional safety: verify weather_box is still a valid widget
    if (!GTK_IS_WIDGET(data->weather_box)) {
        g_free(error_data);
        return G_SOURCE_REMOVE;
    }
    
    // Clear existing widgets
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(data->weather_box))) {
        gtk_box_remove(GTK_BOX(data->weather_box), child);
        // Widget is automatically unparented and will be destroyed when last reference drops
    }
    
    // Show different message based on retry state
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

// Forward declarations
static void fetch_weather(AppData *data);
static gboolean retry_fetch_weather(gpointer user_data);

static void on_weather_response(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    SoupMessage *msg = SOUP_MESSAGE(user_data);
    if (!msg) {
        g_warning("Invalid message in weather response callback");
        return;
    }
    
    AppData *data = (AppData *)g_object_get_data(G_OBJECT(msg), "app-data");
    
    // Clear pending message reference
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
        // Error occurred - determine if it's retryable
        g_warning("Weather fetch error: %s (attempt %d/%d)", error->message, data->retry_count + 1, MAX_RETRY_ATTEMPTS);
        
        // Check if this is a transient error that should be retried
        gboolean should_retry = (data->retry_count < MAX_RETRY_ATTEMPTS);
        
        if (should_retry) {
            // Calculate exponential backoff delay: 30s, 60s, 120s, 240s, 480s
            data->retry_delay = INITIAL_RETRY_DELAY * (1 << data->retry_count);  // 30 * 2^retry_count
            if (data->retry_delay > MAX_RETRY_DELAY) {
                data->retry_delay = MAX_RETRY_DELAY;
            }
            
            data->is_retrying = TRUE;
            
            // Show error message with retry info
            WeatherParseData *error_data = g_new0(WeatherParseData, 1);
            error_data->data = data;
            error_data->json_data = NULL;
            g_idle_add(show_weather_error, error_data);
            
            // Schedule retry
            g_info("Scheduling retry in %d seconds...", data->retry_delay);
            if (data->retry_timer_id != 0) {
                g_source_remove(data->retry_timer_id);
            }
            data->retry_timer_id = g_timeout_add_seconds(data->retry_delay, retry_fetch_weather, data);
            
            data->retry_count++;
        } else {
            // Max retries exceeded
            g_warning("Max retry attempts (%d) exceeded. Will retry at next scheduled update.", MAX_RETRY_ATTEMPTS);
            data->is_retrying = FALSE;
            data->retry_count = 0;
            data->retry_delay = 0;
            
            WeatherParseData *error_data = g_new0(WeatherParseData, 1);
            error_data->data = data;
            error_data->json_data = NULL;
            g_idle_add(show_weather_error, error_data);
        }
        
        g_error_free(error);
        g_object_unref(msg);
        return;
    }
    
    if (!body_bytes) {
        g_warning("No body bytes received (attempt %d/%d)", data->retry_count + 1, MAX_RETRY_ATTEMPTS);
        
        // Retry logic for empty response
        if (data->retry_count < MAX_RETRY_ATTEMPTS) {
            data->retry_delay = INITIAL_RETRY_DELAY * (1 << data->retry_count);
            if (data->retry_delay > MAX_RETRY_DELAY) {
                data->retry_delay = MAX_RETRY_DELAY;
            }
            
            data->is_retrying = TRUE;
            WeatherParseData *error_data = g_new0(WeatherParseData, 1);
            error_data->data = data;
            error_data->json_data = NULL;
            g_idle_add(show_weather_error, error_data);
            
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
            error_data->data = data;
            error_data->json_data = NULL;
            g_idle_add(show_weather_error, error_data);
        }
        
        g_object_unref(msg);
        return;
    }
    
    // Success - get the data
    gsize length;
    const gchar *response_body = (const gchar *)g_bytes_get_data(body_bytes, &length);
    if (response_body && length > 0) {
        // Success! Reset retry counters
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
        
        // Debug: print first 500 chars of response
        gchar *preview = g_strndup(response_body, length > 500 ? 500 : length);
        if (preview) {
            g_debug("Weather API response (first 500 chars): %s", preview);
            g_free(preview);
        }
        
        WeatherParseData *parse_data = g_new0(WeatherParseData, 1);
        parse_data->data = data;
        parse_data->json_data = g_strndup(response_body, length);
        
        // Check if allocation succeeded
        if (!parse_data->json_data) {
            g_warning("Failed to allocate memory for JSON data");
            g_free(parse_data);
        } else {
            g_idle_add(parse_weather_idle, parse_data);
        }
    } else {
        g_warning("Empty response body (attempt %d/%d)", data->retry_count + 1, MAX_RETRY_ATTEMPTS);
        
        // Retry logic for empty body
        if (data->retry_count < MAX_RETRY_ATTEMPTS) {
            data->retry_delay = INITIAL_RETRY_DELAY * (1 << data->retry_count);
            if (data->retry_delay > MAX_RETRY_DELAY) {
                data->retry_delay = MAX_RETRY_DELAY;
            }
            
            data->is_retrying = TRUE;
            WeatherParseData *error_data = g_new0(WeatherParseData, 1);
            error_data->data = data;
            error_data->json_data = NULL;
            g_idle_add(show_weather_error, error_data);
            
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
            error_data->data = data;
            error_data->json_data = NULL;
            g_idle_add(show_weather_error, error_data);
        }
    }
    
    g_bytes_unref(body_bytes);
    g_object_unref(msg);
}

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
    if (data->timezone) {
        g_key_file_set_string(key_file, "Location", "timezone", data->timezone);
    }
    
    // Save UTC offset as fallback (important for deployments without timezone database)
    g_key_file_set_integer(key_file, "Location", "utc_offset_seconds", data->utc_offset_seconds);
    
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
        error = NULL;
    }
    
    // Load timezone if available
    gchar *tz = g_key_file_get_string(key_file, "Location", "timezone", &error);
    if (tz && strlen(tz) > 0) {
        if (data->tz) {
            g_time_zone_unref(data->tz);
        }
        g_free(data->timezone);
        data->timezone = tz;
        data->tz = g_time_zone_new_identifier(tz);
        if (!data->tz) {
            // Fallback: try the deprecated function
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
    
    if (error) {
        g_error_free(error);
        error = NULL;
    }
    
    // Load UTC offset as fallback (critical for deployments without timezone database)
    gint utc_offset = g_key_file_get_integer(key_file, "Location", "utc_offset_seconds", &error);
    if (!error) {
        data->utc_offset_seconds = utc_offset;
        g_debug("Loaded UTC offset from config: %d seconds", utc_offset);
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
    
    // Reset retry counters when manually updating location
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
    (void)widget; // Suppress unused parameter warning
    GtkApplication *app = (GtkApplication *)user_data;
    g_application_quit(G_APPLICATION(app));
}

// Handle close request from window's close button (X)
// Hide the window instead of destroying it
static gboolean on_settings_window_close_request(GtkWindow *window, gpointer user_data) {
    (void)window;
    AppData *data = (AppData *)user_data;
    if (data && data->settings_window) {
        gtk_widget_set_visible(data->settings_window, FALSE);
    }
    return TRUE; // Prevent default close behavior (destruction)
}

// Toggle settings window visibility
static void on_settings_toggle(GtkWidget *widget, gpointer user_data) {
    (void)widget; // Suppress unused parameter warning
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

// Create settings window with location inputs
static void create_settings_window(AppData *data) {
    if (!data) {
        return;
    }
    
    // Create settings window
    data->settings_window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(data->settings_window), "Settings - Weather Clock");
    gtk_window_set_default_size(GTK_WINDOW(data->settings_window), 500, 200);
    gtk_window_set_resizable(GTK_WINDOW(data->settings_window), TRUE);
    gtk_window_set_modal(GTK_WINDOW(data->settings_window), FALSE);
    gtk_widget_set_name(data->settings_window, "settings-window");
    
    // Connect to close-request signal to hide instead of destroy
    g_signal_connect(data->settings_window, "close-request", 
                     G_CALLBACK(on_settings_window_close_request), data);
    
    // Main container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    gtk_widget_set_margin_start(main_box, 20);
    gtk_widget_set_margin_end(main_box, 20);
    gtk_window_set_child(GTK_WINDOW(data->settings_window), main_box);
    
    // Title
    GtkWidget *title_label = gtk_label_new("Location Settings");
    gtk_widget_add_css_class(title_label, "settings-title");
    gtk_widget_set_halign(title_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(main_box), title_label);
    
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
    
    // Close button
    GtkWidget *close_btn = gtk_button_new_with_label("Close");
    gtk_widget_set_halign(close_btn, GTK_ALIGN_END);
    gtk_widget_add_css_class(close_btn, "exit-button");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_settings_toggle), data);
    gtk_box_append(GTK_BOX(main_box), close_btn);
    
    // Initially hide the window
    gtk_widget_set_visible(data->settings_window, FALSE);
}

// Retry callback for failed weather fetches
static gboolean retry_fetch_weather(gpointer user_data) {
    AppData *data = (AppData *)user_data;
    if (!data) {
        return G_SOURCE_REMOVE;
    }
    
    // Safety check: ensure session is still valid (indicates app is still running)
    if (!data->session) {
        return G_SOURCE_REMOVE;
    }
    
    g_info("Retrying weather fetch (attempt %d/%d)...", data->retry_count, MAX_RETRY_ATTEMPTS);
    fetch_weather(data);
    
    // Timer is one-shot, remove it
    data->retry_timer_id = 0;
    return G_SOURCE_REMOVE;
}

static void fetch_weather(AppData *data) {
    if (!data || !data->session) {
        return;
    }
    
    // Cancel any pending request before starting a new one
    // In libsoup-3.0, unref'ing a message automatically cancels it
    if (data->pending_message) {
        g_object_unref(data->pending_message);
        data->pending_message = NULL;
    }
    
    // Cancel any pending retry timer to avoid duplicate fetches
    if (data->retry_timer_id != 0) {
        g_source_remove(data->retry_timer_id);
        data->retry_timer_id = 0;
    }
    
    // Reset retry state when starting a fresh fetch
    // This prevents stale retry state from interfering
    data->retry_count = 0;
    data->retry_delay = 0;
    data->is_retrying = FALSE;
    
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
    // Request 2 days to ensure we always have enough data for 6 hours
    // This is especially important when it's late in the day (e.g., 19:00-23:00 + next day 00:00)
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
    
    // Store data pointer in message user_data for callback
    g_object_set_data(G_OBJECT(msg), "app-data", data);
    data->pending_message = msg;  // Track pending message
    g_object_ref(msg);  // Keep a reference until callback completes
    soup_session_send_and_read_async(data->session, msg, G_PRIORITY_DEFAULT, NULL, on_weather_response, msg);
}

static gboolean update_clock_callback(gpointer user_data) {
    AppData *data = (AppData *)user_data;
    if (!data) {
        return G_SOURCE_REMOVE;
    }
    
    // Safety check: ensure session is still valid (indicates app is still running)
    if (!data->session) {
        return G_SOURCE_REMOVE;
    }
    
    // Only update if widgets are valid and part of widget tree
    if (data->clock_label && data->date_label && 
        GTK_IS_WIDGET(data->clock_label) && GTK_IS_WIDGET(data->date_label) &&
        gtk_widget_get_parent(data->clock_label) && gtk_widget_get_parent(data->date_label)) {
        update_clock(data);
    }
    return G_SOURCE_CONTINUE;
}

static gboolean update_weather_callback(gpointer user_data) {
    AppData *data = (AppData *)user_data;
    if (!data) {
        return G_SOURCE_REMOVE;
    }
    
    // Safety check: ensure session is still valid (indicates app is still running)
    if (!data->session) {
        return G_SOURCE_REMOVE;
    }
    
    fetch_weather(data);
    
    // After first refresh, reschedule for every hour
    // Remove the old timer and create a new one that runs every hour
    if (data->weather_timer_id != 0) {
        g_source_remove(data->weather_timer_id);
        data->weather_timer_id = 0;
    }
    
    // Schedule next refresh for exactly 1 hour from now
    data->weather_timer_id = g_timeout_add_seconds(UPDATE_INTERVAL_SECONDS, update_weather_callback, data);
    
    return G_SOURCE_REMOVE; // Remove the one-time timer
}

// Calculate seconds until the next top of the hour
static guint seconds_until_next_hour(void) {
    time_t now = time(NULL);
    if (now == (time_t)-1) {
        return UPDATE_INTERVAL_SECONDS; // Fallback to 1 hour
    }
    
    struct tm *tm_info = localtime(&now);
    if (!tm_info) {
        return UPDATE_INTERVAL_SECONDS; // Fallback to 1 hour
    }
    
    // Calculate seconds until next hour
    int current_minute = tm_info->tm_min;
    int current_second = tm_info->tm_sec;
    int seconds_remaining_in_hour = (60 - current_minute) * 60 - current_second;
    
    return (guint)seconds_remaining_in_hour;
}

// Direct callback for realize signal
static void on_window_realize_fullscreen_direct(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    AppData *data = (AppData *)user_data;
    if (data && data->window) {
        gtk_window_fullscreen(GTK_WINDOW(data->window));
    }
}

// Idle callback to go fullscreen after window is realized
// This ensures proper scaling on both GNOME and Plasma Wayland
static gboolean on_window_realize_fullscreen(gpointer user_data) {
    AppData *data = (AppData *)user_data;
    if (data && data->window && gtk_widget_get_realized(data->window)) {
        gtk_window_fullscreen(GTK_WINDOW(data->window));
        return G_SOURCE_REMOVE;
    }
    // If not realized yet, try again (max 10 attempts)
    static int attempts = 0;
    if (attempts++ < 10) {
        return G_SOURCE_CONTINUE;
    }
    attempts = 0;
    return G_SOURCE_REMOVE;
}

static void activate(GtkApplication *app, gpointer user_data) {
    AppData *data = (AppData *)user_data;
    
    // Create main window
    data->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(data->window), "Weather Clock");
    
    // Make window resizable
    gtk_window_set_resizable(GTK_WINDOW(data->window), TRUE);
    
    // Set window background to black
    gtk_widget_set_name(data->window, "main-window");
    
    // Main container
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_top(main_box, 40);
    gtk_widget_set_margin_bottom(main_box, 40);
    gtk_widget_set_margin_start(main_box, 40);
    gtk_widget_set_margin_end(main_box, 40);
    gtk_window_set_child(GTK_WINDOW(data->window), main_box);
    
    // Ensure the window content scales properly
    gtk_widget_set_hexpand(main_box, TRUE);
    gtk_widget_set_vexpand(main_box, TRUE);
    
    // Button box at the top (Settings and Exit)
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_END);
    gtk_widget_set_margin_bottom(button_box, 10);
    
    // Settings button
    GtkWidget *settings_btn = gtk_button_new_with_label("Settings");
    gtk_widget_add_css_class(settings_btn, "exit-button");
    g_signal_connect(settings_btn, "clicked", G_CALLBACK(on_settings_toggle), data);
    gtk_box_append(GTK_BOX(button_box), settings_btn);
    
    // Exit button
    GtkWidget *exit_btn = gtk_button_new_with_label("Exit");
    gtk_widget_add_css_class(exit_btn, "exit-button");
    g_signal_connect(exit_btn, "clicked", G_CALLBACK(on_exit_clicked), app);
    gtk_box_append(GTK_BOX(button_box), exit_btn);
    
    gtk_box_append(GTK_BOX(main_box), button_box);
    
    // Create settings window
    create_settings_window(data);
    
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
    data->css_provider = gtk_css_provider_new();
    const char *css = 
        "#main-window {"
        "  background-color: #000000;"
        "}"
        "window {"
        "  background-color: #000000;"
        "}"
        ".clock-time {"
        "  font-size: 340px;"
        "  font-weight: bold;"
        "  color: #ffffff;"
        "}"
        ".clock-date {"
        "  font-size: 75px;"
        "  color: #ffffff;"
        "}"
        ".weather-section {"
        "  background-color: rgba(20, 20, 20, 0.9);"
        "  border-radius: 15px;"
        "  padding: 10px;"
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
        "  font-size: 40px;"
        "}"
        ".weather-temp {"
        "  font-size: 64px;"
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
        "}"
        ".settings-title {"
        "  font-size: 24px;"
        "  font-weight: bold;"
        "  color: #ffffff;"
        "  margin-bottom: 15px;"
        "}";
    
    gtk_css_provider_load_from_string(data->css_provider, css);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(data->css_provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    // Initialize clock
    update_clock(data);
    
    // Set up timers and track their IDs
    data->clock_timer_id = g_timeout_add_seconds(1, update_clock_callback, data);
    
    // Calculate seconds until next top of the hour for weather refresh
    guint seconds_until_hour = seconds_until_next_hour();
    
    // Set up initial weather timer to trigger at the next top of the hour
    // After that, it will refresh every hour automatically
    data->weather_timer_id = g_timeout_add_seconds(seconds_until_hour, update_weather_callback, data);
    
    // Fetch initial weather
    fetch_weather(data);
    
    // Show window first
    gtk_widget_set_visible(data->window, TRUE);
    
    // Go fullscreen after window is realized
    // This ensures GTK4 properly calculates the window size with scaling on both GNOME and Plasma Wayland
    // Use idle callback to ensure window is fully realized before going fullscreen
    // Also connect to realize signal as backup
    g_signal_connect(data->window, "realize", G_CALLBACK(on_window_realize_fullscreen_direct), data);
    g_idle_add((GSourceFunc)on_window_realize_fullscreen, data);
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
    
    // Cleanup: cancel pending HTTP requests
    // In libsoup-3.0, unref'ing a message automatically cancels it
    if (data->pending_message) {
        g_object_unref(data->pending_message);
        data->pending_message = NULL;
    }
    
    // Cleanup: remove timer sources
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
    
    // Cleanup: unref CSS provider
    if (data->css_provider) {
        g_object_unref(data->css_provider);
        data->css_provider = NULL;
    }
    
    // Cleanup: destroy settings window if it exists
    // Windows are managed by GTK but we null out pointers for safety
    if (data->settings_window) {
        // Window should already be destroyed by GTK when application quits
        // but we explicitly null the pointer to prevent use-after-free
        data->settings_window = NULL;
    }
    if (data->window) {
        data->window = NULL;
    }
    
    // Cleanup: null out widget pointers to prevent use-after-free
    // These widgets are children of windows and are destroyed automatically
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

