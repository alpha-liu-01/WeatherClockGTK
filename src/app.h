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
#define CONFIG_FILE_NAME "weatherclock.conf"
#define MAX_RETRY_ATTEMPTS 5
#define INITIAL_RETRY_DELAY 30
#define MAX_RETRY_DELAY 600
#define WEATHER_HOUR_COUNT 6

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
    AppLanguage language;
    SoupSession *session;
    SoupMessage *pending_message;
    GtkCssProvider *css_provider;
    guint clock_timer_id;
    guint weather_timer_id;
    guint retry_timer_id;
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
