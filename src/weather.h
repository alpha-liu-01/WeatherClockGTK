#ifndef WEATHER_H
#define WEATHER_H

#include "app.h"

const char *get_weather_description(int code, AppLanguage lang);
const char *get_weather_icon(int code);
void fetch_weather(AppData *data);
void schedule_delayed_weather_fetch(AppData *data);
gboolean update_weather_callback(gpointer user_data);

#endif
