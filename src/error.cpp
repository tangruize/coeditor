/*************************************************************************
	> File Name: error.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-25 21:44
	> Description: 
 ************************************************************************/

#include "error.h"

/* program name for error prompt */
const char *program_name = "";

/* prompt an error with prefix 'PROG:FILE:LINE: '
 * if term is 'true', terminate with status EXIT_FAILURE
 * term and lineno are encapsulated by macro, see header
 */
void _err(prompt_t &errmsg, bool term, int lineno) {
    if (strcmp(program_name, "") != 0) {
        cerr << program_name << ":";
    }
    cerr << __FILE__ << ":" << lineno << ":" << errmsg << endl;
    if (term) {
        exit(EXIT_FAILURE);
    }
}

