/*************************************************************************
	> File Name: main.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-08-14 14:21
	> Description: 
 ************************************************************************/

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

using namespace std;

int size = 0;

void sleep_a_while(double s) {
#ifndef NDEBUG
    struct timespec ts;
    long sec = s;
    ts.tv_sec = sec;
    s -= sec;
    s *= 1e9;
    sec = s;
    if (sec > 1000000000) {
        sec = 999999999;
    }
    ts.tv_nsec = sec;
    nanosleep(&ts, NULL);
#endif
}

const char *cmd_u() {
    sleep_a_while(0.05);
    return "u";
}

const char *cmd_r() {
    sleep_a_while(0.05);
    return "r";
}

const char *cmd_l() {
    sleep_a_while(0.05);
    return "l";
}

const char *cmd_s() {
    return "s";
}

const char *cmd_D() {
    static char buf[32];
    if (size == 0) {
        return NULL;
    }
    int off = rand() % size--;
    sprintf(buf, "D %d", off);
    return buf;
}

const char *cmd_I() {
    static char buf[32];
    int off = rand() % ++size;
    int data = rand() % 27;
    if (data == 26) {
        sprintf(buf, "I %d \\n", off);
    }
    else {
        sprintf(buf, "I %d %c", off, data + 'A');
    }
    return buf;
}

typedef const char *(cmd_func)(void);

cmd_func *funcs[] = {
    cmd_I, cmd_I, cmd_I, cmd_I, cmd_I,
    cmd_D, cmd_D, 
    cmd_u, cmd_r, cmd_l
};

#define NR_FUNC (sizeof(funcs) / sizeof(funcs[0]))

const char *generator() {
    const char *ret;
    do {
        int i = rand() % NR_FUNC;
        ret = funcs[i]();
    } while (ret == NULL);
    return ret;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " [n times]" << endl;
        return 1;
    }
    int t = atoi(argv[1]);
    if (t <= 0) {
        cerr << argv[0] << ": Invalid argument: " << argv[1] << endl;
        return 1;
    }
    srand(time(NULL));
    for (int i = 0; i < t; ++i) {
        const char *p = generator();
        cout << p << endl;
        if (p[0] == 'D' || p[0] == 'I')
        cerr << p << endl;
    }
    cout << cmd_l() << endl;
    return 0;
}
