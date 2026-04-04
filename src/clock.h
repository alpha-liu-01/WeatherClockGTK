#ifndef CLOCK_H
#define CLOCK_H

#include "app.h"

void update_clock(AppData *data);
gboolean update_clock_callback(gpointer user_data);
guint seconds_until_next_hour(void);

#endif
