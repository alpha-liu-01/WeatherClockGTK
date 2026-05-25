#ifndef APP_H
#define APP_H

#include "i18n.h"
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
#define UPDATE_INTERVAL_SECONDS 3600
/* After the hourly refresh timer fires (top of the hour), wait this long before calling
 * Open-Meteo so traffic lands ~:00:10 instead of :00:00. Startup and manual location
 * updates fetch immediately. Retries use INITIAL_RETRY_DELAY unchanged. */
#define WEATHER_HOURLY_REFRESH_DELAY_SECONDS 10
#define CONFIG_FILE_NAME "weatherclock.conf"
#define MAX_RETRY_ATTEMPTS 5
#define INITIAL_RETRY_DELAY 30
#define MAX_RETRY_DELAY 600
#define WEATHER_HOUR_COUNT 6
#define WEATHER_DAY_COUNT 6

typedef enum {
    FORECAST_MODE_HOURLY,
    FORECAST_MODE_DAILY
} ForecastMode;

typedef struct {
    gboolean valid;
    gdouble temperature;
    gint weather_code;
    gint us_aqi; /* -1 if missing */
} CurrentHourWeather;

typedef struct {
    gchar hour_label[8];
    gdouble temperature;
    gint weather_code;
} HourlyForecastSlot;

typedef struct {
    gboolean valid;
    gchar *date_iso;
    gchar *day_label;
    gint weather_code;
    gdouble temp_max;
    gdouble temp_min;
} DailyForecastDay;

typedef struct WeatherFetchBundle WeatherFetchBundle;
typedef struct DailyFetchBundle DailyFetchBundle;

typedef struct {
    GtkWidget *window;
    GtkWidget *settings_window;
    GtkWidget *clock_label;
    GtkWidget *date_label;
    GtkWidget *weather_box;
    GtkWidget *weather_scrolled;
    GtkWidget *lat_entry;
    GtkWidget *lon_entry;
    GtkWidget *language_dropdown;
    GtkWidget *location_title_label;
    GtkWidget *lat_label;
    GtkWidget *lon_label;
    GtkWidget *lang_label;
    GtkWidget *update_location_btn;
    GtkWidget *settings_close_btn;
    GtkWidget *settings_btn;
    GtkWidget *exit_btn;
    GtkWidget *weather_title_label;
    GtkWidget *weather_header;
    GtkWidget *forecast_mode_switch;
    ForecastMode forecast_mode;
    HourlyForecastSlot hourly_slots[WEATHER_HOUR_COUNT];
    gboolean hourly_slots_valid;
    gchar *hourly_panel_error;
    DailyForecastDay daily_days[WEATHER_DAY_COUNT];
    gboolean daily_valid;
    gchar *daily_panel_error;
    AppLanguage language;
    SoupSession *session;
    SoupMessage *pending_forecast_message;
    SoupMessage *pending_aqi_message;
    WeatherFetchBundle *fetch_bundle;
    SoupMessage *pending_daily_message;
    DailyFetchBundle *daily_fetch_bundle;
    CurrentHourWeather current_hour;
    GtkCssProvider *css_provider;
    guint clock_timer_id;
    guint weather_timer_id;
    guint daily_timer_id;
    guint scheduled_fetch_timer_id;
    guint scheduled_daily_fetch_timer_id;
    guint retry_timer_id;
    guint daily_retry_timer_id;
    gint daily_retry_count;
    gint daily_retry_delay;
    gboolean daily_is_retrying;
    gchar *location_lat;
    gchar *location_lon;
    gchar *timezone;
    GTimeZone *tz;
    gint utc_offset_seconds;
    gint retry_count;
    gint retry_delay;
    gboolean is_retrying;
} AppData;

#endif
