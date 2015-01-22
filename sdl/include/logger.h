/**
 * \file logger.h
 * \brief Logging macros
 */
#ifndef LOGGER_SDLPLAYER
#define LOGGER_SDLPLAYER
#include <glib.h>

/** Setup the log handler */
void init_logger();

void _log_wrap(GLogLevelFlags level, const char* fmt, ...);

/** \brief Log with user-specified GLogLevelFlags
 * \param level log levels
 * \param msg format string
 * \param ... format string args
 * */
#define LOG(level, msg, ...) _log_wrap(level, "[" LOG_TAG "] " msg, ##__VA_ARGS__)
/** \brief Log at DEBUG level */
#define LOGD(...) LOG(G_LOG_LEVEL_DEBUG, ##__VA_ARGS__)
/** \brief Log at INFO level */
#define LOGI(...) LOG(G_LOG_LEVEL_INFO, ##__VA_ARGS__)
/** \brief Log at MESSAGE level */
#define LOGM(...) LOG(G_LOG_LEVEL_MESSAGE, ##__VA_ARGS__)
/** \brief Log at WARNING level */
#define LOGW(...) LOG(G_LOG_LEVEL_WARNING, ##__VA_ARGS__)
/** \brief Log at CRITICAL level */
#define LOGC(...) LOG(G_LOG_LEVEL_CRITICAL, ##__VA_ARGS__)
/** \brief Log at ERROR level (makes the application abort) */
#define LOGE(...) LOG(G_LOG_LEVEL_ERROR, ##__VA_ARGS__)

#endif
