#include "theme.h"
#include "clock.h"
#include "style.h"

#define THEME_CHECK_INTERVAL_SECONDS 30

static gboolean theme_timer_callback(gpointer user_data) {
    theme_evaluate((AppData *)user_data);
    return G_SOURCE_CONTINUE;
}

ThemeMode theme_resolve_effective(AppData *data) {
    if (!data) {
        return THEME_MODE_DARK;
    }

    if (!data->theme_schedule_enabled) {
        return data->theme_mode;
    }

    GDateTime *dt = clock_get_location_datetime(data);
    if (!dt) {
        return data->current_theme;
    }

    gint now_minutes = g_date_time_get_hour(dt) * 60 + g_date_time_get_minute(dt);
    g_date_time_unref(dt);

    gint light_minutes = data->theme_light_hour * 60 + data->theme_light_minute;
    gint dark_minutes = data->theme_dark_hour * 60 + data->theme_dark_minute;

    if (light_minutes == dark_minutes) {
        /* Degenerate schedule (same time for both): keep whatever is showing. */
        return data->current_theme;
    }

    if (light_minutes < dark_minutes) {
        /* Same-day window, e.g. 07:00 -> 19:00. */
        return (now_minutes >= light_minutes && now_minutes < dark_minutes)
                   ? THEME_MODE_LIGHT
                   : THEME_MODE_DARK;
    }

    /* Window wraps past midnight, e.g. 19:00 -> 07:00. */
    return (now_minutes >= light_minutes || now_minutes < dark_minutes)
               ? THEME_MODE_LIGHT
               : THEME_MODE_DARK;
}

void theme_init(AppData *data) {
    if (!data || !data->css_provider) {
        return;
    }
    data->current_theme = theme_resolve_effective(data);
    gtk_css_provider_load_from_string(data->css_provider,
                                      data->current_theme == THEME_MODE_LIGHT ? APP_CSS_LIGHT : APP_CSS);
}

void theme_apply(AppData *data, ThemeMode mode) {
    if (!data || !data->css_provider) {
        return;
    }
    if (data->current_theme == mode) {
        return;
    }

    data->current_theme = mode;
    gtk_css_provider_load_from_string(data->css_provider,
                                      mode == THEME_MODE_LIGHT ? APP_CSS_LIGHT : APP_CSS);
}

void theme_evaluate(AppData *data) {
    if (!data) {
        return;
    }
    theme_apply(data, theme_resolve_effective(data));
}

void theme_start_timer(AppData *data) {
    if (!data || data->theme_timer_id != 0) {
        return;
    }
    data->theme_timer_id = g_timeout_add_seconds(THEME_CHECK_INTERVAL_SECONDS,
                                                 theme_timer_callback, data);
}

void theme_stop_timer(AppData *data) {
    if (!data || data->theme_timer_id == 0) {
        return;
    }
    g_source_remove(data->theme_timer_id);
    data->theme_timer_id = 0;
}
