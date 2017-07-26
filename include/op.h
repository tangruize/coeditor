/*************************************************************************
	> File Name: op.h
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-25 22:07
	> Description: 
 ************************************************************************/

#ifndef _OP_H
#define _OP_H

/* (line, offset), both should be greater than 0 */
struct pos_t {
    int lineno;
    int offset;
};

/* operation struct */
struct op_t {
    int operation;
    int data;
    pos_t pos;
};

/* enum some operations */
enum {DELETE = 'd', INSERT = 'i', PRINT = 'p', SAVE = 's'};

#endif
