#ifndef WEATHER_H
#define WEATHER_H

#include "app.h"

const char *get_weather_description(int code);
const char *get_weather_icon(int code);
void fetch_weather(AppData *data);
gboolean update_weather_callback(gpointer user_data);

#endif
