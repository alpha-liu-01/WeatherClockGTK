#ifndef THEME_H
#define THEME_H

#include "app.h"

/* Resolves the effective theme from schedule/manual settings without applying it. */
ThemeMode theme_resolve_effective(AppData *data);
/* Loads the initial CSS into data->css_provider and applies the effective theme. */
void theme_init(AppData *data);
/* Swaps the display-wide CSS to the given theme (no-op if already applied). */
void theme_apply(AppData *data, ThemeMode mode);
/* Resolves the effective theme and applies it. */
void theme_evaluate(AppData *data);
/* Starts/stops the periodic re-evaluation timer. */
void theme_start_timer(AppData *data);
void theme_stop_timer(AppData *data);

#endif
