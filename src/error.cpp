/*************************************************************************
	> File Name: error.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-25 21:44
	> Description: 
 ************************************************************************/

#include "error.h"

/* program name for error prompt */
extern char *program_invocation_name;

/* prompt an error with prefix 'PROG:FILE:LINE: '
 * if term is 'true', terminate with status EXIT_FAILURE
 * term and lineno are encapsulated by macro, see header
 */
void _err(prompt_t &errmsg, bool term, int lineno, const char *file) {
#ifndef NDEBUG
    cerr << program_invocation_name << ":" << file << ":"
         << lineno << ":" << errmsg << endl;
#else
    cerr << program_invocation_name << ":" << errmsg << endl;
#endif
    if (term) {
        exit(EXIT_FAILURE);
    }
}

