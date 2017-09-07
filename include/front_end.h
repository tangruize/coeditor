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

extern "C" void frontEnd(textOp &file);
extern "C" const char *front_end_version;
extern "C" const char *front_end_author;
extern "C" volatile int buf_changed;
extern "C" volatile int ot_status;
extern "C" int write_op_pos;
extern "C" int write_op;
extern "C" int front_end_number_version;
extern "C" int no_cli;

#endif
