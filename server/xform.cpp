/*************************************************************************
	> File Name: xform.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-08-06 22:47
	> Description: 
 ************************************************************************/

#include <syslog.h>
#include "op.h"

int xform(op_t &op, op_t &outop) {
    if (op.operation == 0) {
        return 1;
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
                syslog(LOG_WARNING, "WARNING: IDs should not be same");
            }
        }
        else /* if (op.data == outop.data
                 && op.char_offset == outop.char_offset) */
        {
            /* insert same char, do nothing */
            op.operation = NOOP;
            return 1;
        }
    }
    else {
        /* unknown operations */
        syslog(LOG_WARNING, "WARNING: unsupported operations: %c, %c",  
              (char)op.operation, (char)outop.operation);
    }
    return 0;
}
