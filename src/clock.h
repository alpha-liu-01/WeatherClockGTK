#ifndef CLOCK_H
#define CLOCK_H

#include "app.h"

GDateTime *clock_get_location_datetime(AppData *data);
void update_clock(AppData *data);
gboolean update_clock_callback(gpointer user_data);
guint seconds_until_next_hour(void);
guint seconds_until_next_local_midnight(AppData *data);

#endif
