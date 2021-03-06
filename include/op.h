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
#include <sys/types.h>

/* (line, offset), both should be greater than 0 */
struct pos_t {
    int lineno;
    int offset;
};

typedef short user_id_t;

/* operation struct */
struct op_t {
    char operation; /* exclude PRINT and SAVE */
    char data;/* data to insert or date deleted */
    user_id_t id; /* client id */
    union {
        pos_t pos; /* lowercase operations use pos */
        uint64_t char_offset; /* UPPERCASE operations use offset */
    };
}__attribute__((packed));

/* enum some operations */
enum {
    NOOP = 0, SYN = 0, SEND_SYN = 1, RECV_SYN = 2,
    DELETE = 'd', INSERT = 'i', PRINT = 'p', SAVE = 's',
    CH_DELETE = 'D', CH_INSERT = 'I'
};

struct state_t {
    int local;
    int global;
};

struct trans_t {
    op_t op;
    union {
        int64_t state;
        state_t states;
    };
}__attribute__((packed));

/* to authenticate while connecting server */
#define MD5SUM_SIZE 32
struct auth_t {
    user_id_t id;
    char md5sum[MD5SUM_SIZE + 1];
}__attribute__((packed));

#endif
