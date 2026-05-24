#ifndef UI_H
#define UI_H

#include "app.h"

void activate(GtkApplication *app, gpointer user_data);
void ui_refresh_translations(AppData *data);
void weather_layout_update(AppData *data);
void weather_layout_update_later(AppData *data);

#endif
