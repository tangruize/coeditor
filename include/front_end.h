/*************************************************************************
	> File Name: front_end.h
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-25 22:33
	> Description: 
 ************************************************************************/

#ifndef _FRONT_END_H
#define _FRONT_END_H

#include "text_mutex.h"

void frontEnd(textOp &file);
void frontEndCli(textOp &file, istream &in);
void frontEndCurses(textOp &file);
extern volatile int buf_changed;
extern volatile int ot_status;
extern int write_op_pos;
extern int write_op;
extern int front_end_number_version;
extern int no_cli;

#endif
