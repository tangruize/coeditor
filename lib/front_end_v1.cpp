/*************************************************************************
	> File Name: front_end_v1.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-31 15:11
	> Description: 
 ************************************************************************/

#include <iostream>
#include <istream>
#include "text.h"

extern "C" {

#include <unistd.h>
#include <string.h>

#ifdef __x86_64__
#define INTERP "/lib64/ld-linux-x86-64.so.2"
#else
#define INTERP "/lib/ld-linux.so.2"
#endif

static const char interp[] __attribute__((section(".interp"))) = INTERP;
volatile int buf_changed = 0;
int write_op = 1;
int write_op_pos = 0;

const char *front_end_version = "CLI 0.1";
const char *front_end_author = "Ruize Tang";

void frontEndCli(textOp &file, istream &in);

void frontEnd(textOp &file) {
    frontEndCli(file, std::cin);
}

/* invoke the shared library will enter this function */
void __attribute__ ((noreturn))
frontEndMain(void) {
    static const char *banner[] = {
        "Welcome to use Co-editor! ",
        "The version you are now using is ", front_end_version, 
        ", by ", front_end_author, ". Have fun!\n",
    };
    for (int i = 0; i  < sizeof(banner) / sizeof(banner[0]); ++i) {
        write(STDOUT_FILENO, banner[i], strlen(banner[i]));
    }
    _exit(0);
}

}
