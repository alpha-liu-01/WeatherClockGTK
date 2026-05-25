#ifndef WEATHER_H
#define WEATHER_H

#include "app.h"

const char *get_weather_description(int code, AppLanguage lang);
const char *get_weather_icon(int code);
void weather_cancel_pending_fetches(AppData *data);
void weather_cancel_pending_daily_fetches(AppData *data);
void weather_free_daily_cache(AppData *data);
void fetch_weather(AppData *data);
void fetch_daily_weather(AppData *data);
void schedule_delayed_weather_fetch(AppData *data);
void schedule_delayed_daily_weather_fetch(AppData *data);
gboolean update_weather_callback(gpointer user_data);
gboolean update_daily_weather_callback(gpointer user_data);
void weather_refresh_date_label(AppData *data, GDateTime *location_dt);
void weather_show_forecast_panel(AppData *data);
void weather_update_forecast_title(AppData *data);
void weather_refresh_daily_day_labels(AppData *data);

#endif
