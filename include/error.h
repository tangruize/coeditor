/*************************************************************************
	> File Name: error.h
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-25 21:45
	> Description: 
 ************************************************************************/

#ifndef _ERROR_H
#define _ERROR_H

#include "common.h"

/* make assert() effective, comment it to disable */
#define DEBUG
#ifdef DEBUG
#ifdef NDEBUG
#undef NDEBUG
#endif /* NDEBUG */
#else
#define NDEBUG
#endif /* DEBUG */

#include <cassert>
#include <errno.h>

/* return value for some functions, to inform an error */
typedef const string prompt_t;

/* null string stands for no error */
#define NOERR ""

/* program name for error prompt */
extern const char *program_name;

/* prompt an error with prefix 'PROG:FILE:LINE: '
 * if term is 'true', terminate with status EXIT_FAILURE
 * term and lineno are encapsulated by macro, see below
 */
void _err(prompt_t &errmsg, bool term, int lineno, const char *file);

#define errExit(msg) _err(msg, true, __LINE__, __FILE__)
#define errMsg(msg) _err(msg, false, __LINE__, __FILE__)

#define PROMPT_ERROR(msg) do { \
        if (msg != string(NOERR)) { \
            errMsg(msg); \
        } \
    } while(0)

#define EXIT_ERROR(msg) do { \
        if (msg != string(NOERR)) { \
            errExit(msg); \
        } \
    } while(0)

#define PROMPT_ERROR_EN(msg) do { \
        int saved_errno = errno; \
        std::string msg_en = string(msg) + string(": ") \
                             + strerror(saved_errno); \
        if (msg_en != NOERR) { \
            errMsg(msg_en); \
        } \
    } while(0)

#endif
