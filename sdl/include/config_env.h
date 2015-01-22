/**
 * \file config_env.h
 * \brief Utilities to get config values from the environment.
 */
#ifndef __CONFIG_ENV_H_
#define __CONFIG_ENV_H_

/** \brief Get the value of a config variable from the env
 * \param varname Name of the env variable
 * \returns A buffer containing the variable
 */
char* configvar_string(char* varname);

/** \brief Get the value of a integer config variable from the env
 * \param varname Name of the env variable
 * \returns The value of the variable
 */
int configvar_int(char* varname);
/** \brief Get the value of a boolean config variable from the env
 * \param varname Name of the env variable
 * \returns 1 or 0 depending on the truth value
 *
 * A "true" value can be Y/y/1 and a "false" value can be N/n/0.
 */
int configvar_bool(char* varname);

#endif
