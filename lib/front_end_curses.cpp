/*************************************************************************
	> File Name: front_end_curses.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-26 15:15
	> Description: 
 ************************************************************************/

#include "common.h"
#include "front_end.h"
#include "op.h"

#include <ncurses.h>

const char *front_end_version = "CURSES 0.1";

/* i have no idea, but it is not my fault */
const char *front_end_author = "Cooooooooooooolest TRZ";

/* init curses */
void initCurses() {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
}

/* finalization on unloading */
void __attribute__ ((destructor)) endCurses() {
    endwin();
}

/* the front-end function using curses */
extern "C" void frontEnd(textOp &file) {
    initCurses();
    printw("Hello, world!");
    getch();
}
