#include "i18n.h"
#include <glib.h>

static const char *strings_en[I18N_COUNT] = {
    [I18N_MAIN_TITLE] = "Weather Clock",
    [I18N_SETTINGS_TITLE] = "Settings - Weather Clock",
    [I18N_LOCATION_SETTINGS] = "Location Settings",
    [I18N_LANGUAGE] = "Language",
    [I18N_LATITUDE] = "Latitude:",
    [I18N_LONGITUDE] = "Longitude:",
    [I18N_UPDATE_LOCATION] = "Update Location",
    [I18N_CLOSE] = "Close",
    [I18N_SETTINGS_BTN] = "Settings",
    [I18N_EXIT] = "Exit",
    [I18N_WEATHER_FORECAST_TITLE] = "Hourly Weather Forecast",
    [I18N_DAILY_FORECAST_TITLE] = "6-Day Forecast",
    [I18N_TODAY] = "Today",
    [I18N_DAILY_TEMP_RANGE] = "%.0f\xc2\xb0 / %.0f\xc2\xb0""C",
    [I18N_FORECAST_MODE_HOURLY] = "Hourly forecast",
    [I18N_FORECAST_MODE_DAILY] = "Daily forecast",
    [I18N_LANG_OPTION_EN] = "English",
    [I18N_LANG_OPTION_ZH] = "\xe4\xb8\xad\xe6\x96\x87\xef\xbc\x88\xe7\xae\x80\xe4\xbd\x93\xef\xbc\x89",
    [I18N_DATE_PLACEHOLDER] = "Monday, January 1, 2024",
    [I18N_DATE_WITH_WEATHER] = "%s \xc2\xb7 %s \xc2\xb7 %.1f\xc2\xb0""C \xc2\xb7 AQI %d",
    [I18N_DATE_WITH_WEATHER_NO_AQI] = "%s \xc2\xb7 %s \xc2\xb7 %.1f\xc2\xb0""C",
    [I18N_WEATHER_CLEAR] = "Clear",
    [I18N_WEATHER_CLOUDY] = "Cloudy",
    [I18N_WEATHER_FOGGY] = "Foggy",
    [I18N_WEATHER_DRIZZLE] = "Drizzle",
    [I18N_WEATHER_RAIN] = "Rain",
    [I18N_WEATHER_SNOW] = "Snow",
    [I18N_WEATHER_RAIN_SHOWER] = "Rain Shower",
    [I18N_WEATHER_SNOW_SHOWER] = "Snow Shower",
    [I18N_WEATHER_THUNDERSTORM] = "Thunderstorm",
    [I18N_WEATHER_UNKNOWN] = "Unknown",
    [I18N_NA] = "N/A",
    [I18N_ERR_EMPTY_WEATHER] = "Empty weather data received",
    [I18N_ERR_SERVER_HTML] = "Server returned error page - retrying...",
    [I18N_ERR_PARSE_FMT] = "Parse error: %s - retrying...",
    [I18N_ERR_NO_ROOT] = "Invalid JSON: no root node - retrying...",
    [I18N_ERR_INVALID_FORMAT] = "Invalid weather data format - retrying...",
    [I18N_ERR_API_FMT] = "API Error: %s (Keys: %s)",
    [I18N_ERR_API_TYPE_FMT] = "API Error: Error type: %s (Keys: %s)",
    [I18N_ERR_API_UNKNOWN_FMT] = "API Error: Unknown (Keys: %s)",
    [I18N_ERR_NO_HOURLY_FMT] = "No hourly data - retrying... Keys: %s",
    [I18N_ERR_NO_HOURLY_DATA] = "No hourly data available - retrying...",
    [I18N_ERR_INCOMPLETE_DATA] = "Incomplete weather data - retrying...",
    [I18N_ERR_CONNECTION_FMT] = "Connection issue - retrying in %d seconds... (attempt %d/%d)",
    [I18N_ERR_FETCH_FAILED] = "Failed to fetch weather - will retry at next scheduled update",
    [I18N_ERR_UNKNOWN_DETAIL] = "Unknown",
};

static const char *strings_zh[I18N_COUNT] = {
    [I18N_MAIN_TITLE] = "\xe5\xa4\xa9\xe6\xb0\x94\xe6\x97\xb6\xe9\x92\x9f",
    [I18N_SETTINGS_TITLE] = "\xe8\xae\xbe\xe7\xbd\xae - \xe5\xa4\xa9\xe6\xb0\x94\xe6\x97\xb6\xe9\x92\x9f",
    [I18N_LOCATION_SETTINGS] = "\xe4\xbd\x8d\xe7\xbd\xae\xe8\xae\xbe\xe7\xbd\xae",
    [I18N_LANGUAGE] = "\xe8\xaf\xad\xe8\xa8\x80",
    [I18N_LATITUDE] = "\xe7\xba\xac\xe5\xba\xa6\xef\xbc\x9a",
    [I18N_LONGITUDE] = "\xe7\xbb\x8f\xe5\xba\xa6\xef\xbc\x9a",
    [I18N_UPDATE_LOCATION] = "\xe6\x9b\xb4\xe6\x96\xb0\xe4\xbd\x8d\xe7\xbd\xae",
    [I18N_CLOSE] = "\xe5\x85\xb3\xe9\x97\xad",
    [I18N_SETTINGS_BTN] = "\xe8\xae\xbe\xe7\xbd\xae",
    [I18N_EXIT] = "\xe9\x80\x80\xe5\x87\xba",
    [I18N_WEATHER_FORECAST_TITLE] = "\xe9\x80\x90\xe5\xb0\x8f\xe6\x97\xb6\xe5\xa4\xa9\xe6\xb0\x94\xe9\xa2\x84\xe6\x8a\xa5",
    [I18N_DAILY_FORECAST_TITLE] = "6\xe6\x97\xa5\xe5\xa4\xa9\xe6\xb0\x94\xe9\xa2\x84\xe6\x8a\xa5",
    [I18N_TODAY] = "\xe4\xbb\x8a\xe5\xa4\xa9",
    [I18N_DAILY_TEMP_RANGE] = "%.0f\xc2\xb0 / %.0f\xc2\xb0""C",
    [I18N_FORECAST_MODE_HOURLY] = "\xe9\x80\x90\xe5\xb0\x8f\xe6\x97\xb6\xe9\xa2\x84\xe6\x8a\xa5",
    [I18N_FORECAST_MODE_DAILY] = "\xe9\x80\x90\xe6\x97\xa5\xe9\xa2\x84\xe6\x8a\xa5",
    [I18N_LANG_OPTION_EN] = "English",
    [I18N_LANG_OPTION_ZH] = "\xe4\xb8\xad\xe6\x96\x87\xef\xbc\x88\xe7\xae\x80\xe4\xbd\x93\xef\xbc\x89",
    [I18N_DATE_PLACEHOLDER] = "2024\xe5\xb9\xb4\x31\xe6\x9c\x88\x31\xe6\x97\xa5 \xe6\x98\x9f\xe6\x9c\x9f\xe4\xb8\x80",
    [I18N_DATE_WITH_WEATHER] = "%s\xef\xbc\x8c%s\xef\xbc\x8c%.1f\xc2\xb0""C\xef\xbc\x8c""AQI %d",
    [I18N_DATE_WITH_WEATHER_NO_AQI] = "%s\xef\xbc\x8c%s\xef\xbc\x8c%.1f\xc2\xb0""C",
    [I18N_WEATHER_CLEAR] = "\xe6\x99\xb4",
    [I18N_WEATHER_CLOUDY] = "\xe5\xa4\x9a\xe4\xba\x91",
    [I18N_WEATHER_FOGGY] = "\xe9\x9b\xbe",
    [I18N_WEATHER_DRIZZLE] = "\xe6\xaf\x9b\xe6\xaf\x9b\xe9\x9b\xa8",
    [I18N_WEATHER_RAIN] = "\xe9\x9b\xa8",
    [I18N_WEATHER_SNOW] = "\xe9\x9b\xaa",
    [I18N_WEATHER_RAIN_SHOWER] = "\xe9\x98\xb5\xe9\x9b\xa8",
    [I18N_WEATHER_SNOW_SHOWER] = "\xe9\x98\xb5\xe9\x9b\xaa",
    [I18N_WEATHER_THUNDERSTORM] = "\xe9\x9b\xb7\xe6\x9a\xb4",
    [I18N_WEATHER_UNKNOWN] = "\xe6\x9c\xaa\xe7\x9f\xa5",
    [I18N_NA] = "\xe6\x97\xa0",
    [I18N_ERR_EMPTY_WEATHER] = "\xe6\x94\xb6\xe5\x88\xb0\xe7\xa9\xba\xe7\x9a\x84\xe5\xa4\xa9\xe6\xb0\x94\xe6\x95\xb0\xe6\x8d\xae",
    [I18N_ERR_SERVER_HTML] = "\xe6\x9c\x8d\xe5\x8a\xa1\xe5\x99\xa8\xe8\xbf\x94\xe5\x9b\x9e\xe9\x94\x99\xe8\xaf\xaf\xe9\xa1\xb5\xe9\x9d\xa2\xef\xbc\x8c\xe6\xad\xa3\xe5\x9c\xa8\xe9\x87\x8d\xe8\xaf\x95...",
    [I18N_ERR_PARSE_FMT] = "\xe8\xa7\xa3\xe6\x9e\x90\xe9\x94\x99\xe8\xaf\xaf\xef\xbc\x9a%s\xef\xbc\x8c\xe6\xad\xa3\xe5\x9c\xa8\xe9\x87\x8d\xe8\xaf\x95...",
    [I18N_ERR_NO_ROOT] = "\xe6\x97\xa0\xe6\x95\x88 JSON\xef\xbc\x9a\xe6\x97\xa0\xe6\xa0\xb9\xe8\x8a\x82\xe7\x82\xb9\xef\xbc\x8c\xe6\xad\xa3\xe5\x9c\xa8\xe9\x87\x8d\xe8\xaf\x95...",
    [I18N_ERR_INVALID_FORMAT] = "\xe5\xa4\xa9\xe6\xb0\x94\xe6\x95\xb0\xe6\x8d\xae\xe6\xa0\xbc\xe5\xbc\x8f\xe6\x97\xa0\xe6\x95\x88\xef\xbc\x8c\xe6\xad\xa3\xe5\x9c\xa8\xe9\x87\x8d\xe8\xaf\x95...",
    [I18N_ERR_API_FMT] = "API \xe9\x94\x99\xe8\xaf\xaf\xef\xbc\x9a%s\xef\xbc\x88\xe9\x94\xae\xef\xbc\x9a%s\xef\xbc\x89",
    [I18N_ERR_API_TYPE_FMT] = "API \xe9\x94\x99\xe8\xaf\xaf\xef\xbc\x9a\xe9\x94\x99\xe8\xaf\xaf\xe7\xb1\xbb\xe5\x9e\x8b %s\xef\xbc\x88\xe9\x94\xae\xef\xbc\x9a%s\xef\xbc\x89",
    [I18N_ERR_API_UNKNOWN_FMT] = "API \xe9\x94\x99\xe8\xaf\xaf\xef\xbc\x9a\xe6\x9c\xaa\xe7\x9f\xa5\xef\xbc\x88\xe9\x94\xae\xef\xbc\x9a%s\xef\xbc\x89",
    [I18N_ERR_NO_HOURLY_FMT] = "\xe6\x97\xa0\xe9\x80\x90\xe5\xb0\x8f\xe6\x97\xb6\xe6\x95\xb0\xe6\x8d\xae\xef\xbc\x8c\xe6\xad\xa3\xe5\x9c\xa8\xe9\x87\x8d\xe8\xaf\x95... \xe9\x94\xae\xef\xbc\x9a%s",
    [I18N_ERR_NO_HOURLY_DATA] = "\xe6\x97\xa0\xe9\x80\x90\xe5\xb0\x8f\xe6\x97\xb6\xe6\x95\xb0\xe6\x8d\xae\xef\xbc\x8c\xe6\xad\xa3\xe5\x9c\xa8\xe9\x87\x8d\xe8\xaf\x95...",
    [I18N_ERR_INCOMPLETE_DATA] = "\xe5\xa4\xa9\xe6\xb0\x94\xe6\x95\xb0\xe6\x8d\xae\xe4\xb8\x8d\xe5\xae\x8c\xe6\x95\xb4\xef\xbc\x8c\xe6\xad\xa3\xe5\x9c\xa8\xe9\x87\x8d\xe8\xaf\x95...",
    [I18N_ERR_CONNECTION_FMT] = "\xe8\xbf\x9e\xe6\x8e\xa5\xe9\x97\xae\xe9\xa2\x98\xef\xbc\x8c%d \xe7\xa7\x92\xe5\x90\x8e\xe9\x87\x8d\xe8\xaf\x95... (\xe7\xac\xac %d/%d \xe6\xac\xa1)",
    [I18N_ERR_FETCH_FAILED] = "\xe8\x8e\xb7\xe5\x8f\x96\xe5\xa4\xa9\xe6\xb0\x94\xe5\xa4\xb1\xe8\xb4\xa5\xef\xbc\x8c\xe5\xb0\x86\xe5\x9c\xa8\xe4\xb8\x8b\xe6\xac\xa1\xe5\xae\x9a\xe6\x97\xb6\xe6\x9b\xb4\xe6\x96\xb0\xe6\x97\xb6\xe9\x87\x8d\xe8\xaf\x95",
    [I18N_ERR_UNKNOWN_DETAIL] = "\xe6\x9c\xaa\xe7\x9f\xa5",
};

const char *i18n_get(AppLanguage lang, I18nId id) {
    if ((unsigned)id >= (unsigned)I18N_COUNT) {
        return "";
    }
    if (lang == APP_LANG_ZH_CN) {
        return strings_zh[id] ? strings_zh[id] : "";
    }
    return strings_en[id] ? strings_en[id] : "";
}

AppLanguage app_language_from_string(const char *s) {
    if (!s) {
        return APP_LANG_EN;
    }
    if (g_strcmp0(s, "zh_CN") == 0) {
        return APP_LANG_ZH_CN;
    }
    return APP_LANG_EN;
}

const char *app_language_to_string(AppLanguage lang) {
    if (lang == APP_LANG_ZH_CN) {
        return "zh_CN";
    }
    return "en";
}

