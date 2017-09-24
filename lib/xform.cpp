/************************************************************************
	> File Name: xform.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-08-06 22:47
	> Description: 
 ************************************************************************/

#include <syslog.h>
#include <iostream>
#include <sstream>
#include "op.h"
#include "xform.h"

using namespace std;

static int _xform(op_t &op, op_t &outop, string &error) {
    if (op.operation == 0) {
        return 1;
    }
    else if (outop.operation == 0) {
        return 0;
    }
    if (op.operation == CH_DELETE && outop.operation == CH_DELETE) {
        if (op.char_offset > outop.char_offset) {
            --op.char_offset;
        }
        else if (op.char_offset < outop.char_offset) {
            --outop.char_offset;
        }
        else /* if (op.char_offset == outop.char_offset) */ {
            /* delete same char, do nothing */
            op.operation = NOOP;
            outop.operation = NOOP;
            return 1;
        }
    }
    else if (op.operation == CH_DELETE && outop.operation == CH_INSERT)
    {
        if (op.char_offset >= outop.char_offset) {
            ++op.char_offset;
        }
        else /* if (op.char_offset < outop.char_offset) */ {
            --outop.char_offset;
        }
    }
    else if (op.operation == CH_INSERT && outop.operation == CH_DELETE)
    {
        if (op.char_offset > outop.char_offset) {
            --op.char_offset;
        }
        else /* if (op.char_offset <= outop.char_offset) */ {
            ++outop.char_offset;
        }
        
    }
    else if (op.operation == CH_INSERT && outop.operation
             == CH_INSERT)
    {
        if (op.char_offset > outop.char_offset) {
            ++op.char_offset;
        }
        else if (op.char_offset < outop.char_offset) {
            ++outop.char_offset;
        }
        else if (op.data != outop.data
                 /* && op.char_offset == outop.char_offset */ )
        {
            /* Insert different chars at different offset */
            if (op.id > outop.id) {
                ++op.char_offset;
            }
            else if (op.id < outop.id) {
                ++outop.char_offset;
            }
            else {
                /* should never happen! */
                error = "WARNING: IDs should not be same";
            }
        }
        else /* if (op.data == outop.data
                 && op.char_offset == outop.char_offset) */
        {
            /* insert same char, do nothing */
            op.operation = NOOP;
            outop.operation = NOOP;
            return 1;
        }
    }
    else {
        /* unknown operations */
        stringstream ss;
        ss << "WARNING: unsupported operations: ";
        if (isalpha(op.operation)) {
            ss << (char)op.operation;
        }
        else {
            ss << (unsigned)op.operation;
        }
        ss << ", ";
        if (isalpha(outop.operation)) {
            ss << (char)outop.operation;
        }
        else {
            ss << (unsigned)outop.operation;
        }
        getline(ss, error);
    }
    return 0;
}

int xform(op_t &op, op_t &outop) {
    string s;
    int ret = _xform(op, outop, s);
    // if (s.size()) {
    //     cerr << s << endl;
    // }
    return ret;
}

/*
int xformServer(op_t &op, op_t &outop) {
    string s;
    int ret = _xform(op, outop, s);
    if (s.size()) {
        syslog(LOG_WARNING, "%s", s.c_str());
    }
    return ret;
}
*/