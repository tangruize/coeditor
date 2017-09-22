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
#include <pthread.h>

#include "text_mutex.h"
#include "init.h"
#include "front_end.h"

using namespace std;

volatile int buf_changed = 0;
volatile int ot_status = -1;
int write_op = 0;
int write_op_pos = 0;
int no_cli = 1;
int front_end_number_version = 2;

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

void frontEnd(textOp &file) {
    redirectStderr();
    if (!no_cli && creatCliInputFifo() == 0) {
        pthread_t t1;
        pthread_create(&t1, NULL, fifoInputCli_thread, &file);
    }
    frontEndCurses(file);
}
