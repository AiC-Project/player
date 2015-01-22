/**
 * \file logger.c
 * \brief Logging
 */
#include <stdarg.h>  // for va_list
#include <stdio.h>   // for NULL
#include <string.h>  // for memset
#include <time.h>    // for localtime, time, time_t

#include "logger.h"

void _log_wrap(GLogLevelFlags level, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    g_logv("player", level, fmt, va);
    va_end(va);
}

static const gchar* log_level_to_string(GLogLevelFlags level)
{
    switch (level)
    {
    case G_LOG_LEVEL_ERROR:
        return "ERROR";
    case G_LOG_LEVEL_CRITICAL:
        return "CRITICAL";
    case G_LOG_LEVEL_WARNING:
        return "WARNING";
    case G_LOG_LEVEL_MESSAGE:
        return "MESSAGE";
    case G_LOG_LEVEL_INFO:
        return "INFO";
    case G_LOG_LEVEL_DEBUG:
        return "DEBUG";
    default:
        return "UNKNOWN";
    }
}

static void log_handler_cb(const gchar* log_domain, GLogLevelFlags log_level, const gchar* message,
                           gpointer user_data)
{
    (void) log_domain;
    (void) user_data;
    const gchar* log_level_str;

    int debug_enabled = 1;

    /* Ignore debug messages if disabled. */
    if (!debug_enabled && (log_level & G_LOG_LEVEL_DEBUG))
    {
        return;
    }

    log_level_str = log_level_to_string(log_level & G_LOG_LEVEL_MASK);

    time_t raw_time;
    time(&raw_time);
    struct tm info;
    localtime_r(&raw_time, &info);
    char time[30];
    memset(time, 0, 30);
    strftime(time, 30, "%H:%M:%S", &info);
    /* Use g_printerr() for warnings and g_print() otherwise. */
    if (log_level <= G_LOG_LEVEL_WARNING)
    {
        g_printerr("%s %8s %s\n", time, log_level_str, message);
    }
    else
    {
        g_print("%s %8s %s\n", time, log_level_str, message);
    }
}

void init_logger()
{
    g_log_set_handler("player", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
                      log_handler_cb, NULL);
}
