/** \file config_env.c
 * \brief Utilities to get config values from the environment
 */
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "config_env.h"

#define LOG_TAG "config_env"

static char* configvar_raw(char* varname)
{
    char* val = getenv(varname);
    if (val == NULL)
    {
        LOGE("No envvar %s", varname);
        exit(1);
    }
    if (strlen(val) == 0)
    {
        LOGE("Envvar %s is empty", varname);
        exit(1);
    }
    return val;
}

char* configvar_string(char* varname)
{
    char* val = configvar_raw(varname);
    LOGD("%s: %s", varname, val);
    return val;
}

int configvar_int(char* varname)
{
    int ret;
    char* val = configvar_raw(varname);
    ret = atoi(val);
    LOGD("%s: %d", varname, ret);
    return ret;
}

int configvar_bool(char* varname)
{
    int ret = 0;
    char* val = configvar_raw(varname);
    char yn = val[0];
    if (yn == 'y' || yn == 'Y' || yn == '1')
    {
        ret = 1;
    }
    else if (yn != 'n' && yn != 'N' && yn != '0')
    {
        LOGE("%s: value must start with (y|n|0|1), was %s", varname, val);
        exit(1);
    }
    LOGD("%s: %d", varname, ret);
    return ret;
}
