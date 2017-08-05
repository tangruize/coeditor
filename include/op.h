/*************************************************************************
	> File Name: op.h
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-25 22:07
	> Description: 
 ************************************************************************/

#ifndef _OP_H
#define _OP_H

#include <stdint.h>

/* (line, offset), both should be greater than 0 */
struct pos_t {
    int lineno;
    int offset;
};

/* operation struct */
struct op_t {
    char operation;
    char data;
    short id;
    union {
        pos_t pos;
        uint64_t char_offset;
    };
}__attribute__((packed));

/* enum some operations */
enum {
    DELETE = 'd', INSERT = 'i', PRINT = 'p', SAVE = 's',
    CH_DELETE = 'D', CH_INSERT = 'I'
};

struct state_t {
    int client;
    int server;
};

struct trans_t {
    op_t op;
    state_t state;
}__attribute__((packed));

#endif
