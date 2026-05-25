#include "clock.h"
#include "weather.h"

GDateTime *clock_get_location_datetime(AppData *data) {
    if (!data) {
        return NULL;
    }

    time_t now = time(NULL);
    if (now == (time_t)-1) {
        return NULL;
    }

    if (data->utc_offset_seconds != 0) {
        return g_date_time_new_from_unix_utc((gint64)(now + data->utc_offset_seconds));
    }
    if (data->tz) {
        GDateTime *utc = g_date_time_new_from_unix_utc(now);
        if (!utc) {
            return NULL;
        }
        GDateTime *local = g_date_time_to_timezone(utc, data->tz);
        g_date_time_unref(utc);
        return local;
    }
    return g_date_time_new_from_unix_local(now);
}

void update_clock(AppData *data) {
    if (!data || !data->clock_label || !data->date_label) {
        return;
    }

    if (!GTK_IS_WIDGET(data->clock_label) || !GTK_IS_WIDGET(data->date_label)) {
        return;
    }
    if (!GTK_IS_LABEL(data->clock_label) || !GTK_IS_LABEL(data->date_label)) {
        return;
    }

    if (!gtk_widget_get_parent(data->clock_label) || !gtk_widget_get_parent(data->date_label)) {
        return;
    }

    GDateTime *dt = clock_get_location_datetime(data);
    if (!dt) {
        return;
    }

    gchar *time_str = g_date_time_format(dt, "%H:%M:%S");

    if (time_str) {
        gtk_label_set_text(GTK_LABEL(data->clock_label), time_str);
        g_free(time_str);
    }

    weather_refresh_date_label(data, dt);

    g_date_time_unref(dt);
}

gboolean update_clock_callback(gpointer user_data) {
    AppData *data = (AppData *)user_data;
    if (!data) {
        return G_SOURCE_REMOVE;
    }

    if (!data->session) {
        return G_SOURCE_REMOVE;
    }

    if (data->clock_label && data->date_label &&
        GTK_IS_WIDGET(data->clock_label) && GTK_IS_WIDGET(data->date_label) &&
        gtk_widget_get_parent(data->clock_label) && gtk_widget_get_parent(data->date_label)) {
        update_clock(data);
        gtk_widget_set_visible(data->clock_label, TRUE);
        gtk_widget_set_visible(data->date_label, TRUE);
        gtk_widget_queue_draw(data->clock_label);
        gtk_widget_queue_draw(data->date_label);
    }
    return G_SOURCE_CONTINUE;
}

guint seconds_until_next_hour(void) {
    time_t now = time(NULL);
    if (now == (time_t)-1) {
        return UPDATE_INTERVAL_SECONDS;
    }

    struct tm *tm_info = localtime(&now);
    if (!tm_info) {
        return UPDATE_INTERVAL_SECONDS;
    }

    int current_minute = tm_info->tm_min;
    int current_second = tm_info->tm_sec;
    int seconds_remaining_in_hour = (60 - current_minute) * 60 - current_second;

    return (guint)seconds_remaining_in_hour;
}
