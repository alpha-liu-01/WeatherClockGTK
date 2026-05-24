#include "clock.h"

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

static gchar *format_date_string(GDateTime *dt, AppLanguage lang) {
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

    time_t now = time(NULL);
    if (now == (time_t)-1) {
        return;
    }

    GDateTime *dt = NULL;

    /* PRIORITY 1: Use UTC offset (most reliable on Windows deployments).
       The utc_offset_seconds comes directly from the weather API and doesn't
       depend on having timezone database files installed. */
    if (data->utc_offset_seconds != 0) {
        time_t adjusted_time = now + data->utc_offset_seconds;
        dt = g_date_time_new_from_unix_utc(adjusted_time);
    }
    /* PRIORITY 2: Try GTimeZone object (may not work on Windows without tz database) */
    else if (data->tz) {
        dt = g_date_time_new_from_unix_utc(now);
        if (dt) {
            GDateTime *dt_tz = g_date_time_to_timezone(dt, data->tz);
            g_date_time_unref(dt);
            dt = dt_tz;
        }
    }
    /* PRIORITY 3: Fallback to system local time */
    else {
        dt = g_date_time_new_from_unix_local(now);
    }

    if (!dt) {
        return;
    }

    gchar *time_str = g_date_time_format(dt, "%H:%M:%S");
    gchar *date_str = format_date_string(dt, data->language);

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
