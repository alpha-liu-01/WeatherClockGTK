#ifndef CONFIG_H
#define CONFIG_H

#include "app.h"

gchar *get_config_file_path(void);
void save_location_to_config(AppData *data);
void load_location_from_config(AppData *data);
void update_location_from_entries(AppData *data);
void save_language_to_config(AppData *data);
void load_language_from_config(AppData *data);

#endif
