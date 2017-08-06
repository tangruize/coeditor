/*************************************************************************
	> File Name: front_end.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-26 08:33
	> Description: 
 ************************************************************************/

#include <iostream>
#include <istream>
#include <fstream>
#include <string>
#include <unistd.h>
#include <string.h>
#include "text_mutex.h"
#include "init.h"
#include <pthread.h>

using namespace std;

/* make shared library excutable, need C linkage, 
 * interpreter, and no return entry
 */
extern "C" {

#ifdef __x86_64__
#define INTERP "/lib64/ld-linux-x86-64.so.2"
#else
#define INTERP "/lib/ld-linux.so.2"
#endif

static const char interp[] __attribute__((section(".interp"))) = INTERP;

volatile int buf_changed = 0;
volatile int ot_status = -1;
int write_op = 0;
int write_op_pos = 0;
int no_cli = 1;

__asm__(".symver front_end_cli_version,front_end_version@VER_1");
__asm__(".symver front_end_curses_version,front_end_version@@VER_2");
__asm__(".symver front_end_cli_author,front_end_author@VER_1");
__asm__(".symver front_end_curses_author,front_end_author@@VER_2");
__asm__(".symver frontEnd_V1,frontEnd@VER_1");
__asm__(".symver frontEnd_V2,frontEnd@@VER_2");
__asm__(".symver frontEndMain_V1,frontEndMain@VER_1");
__asm__(".symver frontEndMain_V2,frontEndMain@@VER_2");

const char *front_end_cli_version = "CLI 0.1";
const char *front_end_cli_author = "Ruize Tang";
int front_end_number_version = 2;

const char *front_end_curses_version = "CURSES 1.1";

/* i have no idea, but it is not my fault */
const char *front_end_curses_author = "Cooooooooooooolest TRZ";

#define PAD_SIZE 32
static char version[PAD_SIZE] = {0};
static char author[PAD_SIZE] = {0};

const char *banner[] = {
    "Welcome to use Co-editor! ",
    "The version you are now using is ", version, 
    ", by ", author, ". Have fun!\n",
};

void printBanner() {
    for (int i = 0; i  < sizeof(banner) / sizeof(banner[0]); ++i) {
        write(STDOUT_FILENO, banner[i], strlen(banner[i]));
    }
}

/* invoke the shared library will enter this function */
void __attribute__ ((noreturn))
frontEndMain_V1(void) {
    strncpy(version, front_end_cli_version, PAD_SIZE - 1);
    strncpy(author, front_end_cli_author, PAD_SIZE - 1);
    printBanner();
    _exit(0);
}

void __attribute__ ((noreturn))
frontEndMain_V2(void) {
    strncpy(version, front_end_curses_version, PAD_SIZE - 1);
    strncpy(author, front_end_curses_author, PAD_SIZE - 1);
    printBanner();
    _exit(0);
}

void frontEndCli(textOp &file, istream &in);
void frontEndCurses(textOp &file);

void frontEnd_V1(textOp &file) {
    write_op = 1;
    frontEndCli(file, std::cin);
}

static void *fifoInputCli_thread(void *arg) {
    pthread_detach(pthread_self());
    gotoEditFileDir();
    ifstream in(getCliInputFifoName().c_str());
    restoreOldCwd();
    if (!in.is_open()) {
        return NULL;
    }
    frontEndCli(*(textOp*)arg, in);
    return NULL;
}

void frontEnd_V2(textOp &file) {
    redirectStderr();
#ifdef DEBUG
    if (!no_cli && creatCliInputFifo() == 0) {
        pthread_t t1;
        pthread_create(&t1, NULL, fifoInputCli_thread, &file);
    }
#endif
    frontEndCurses(file);
}

}
