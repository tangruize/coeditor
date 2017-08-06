/*************************************************************************
	> File Name: front_end_cli.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-25 22:30
	> Description: 
 ************************************************************************/

#include "common.h"
#include "front_end.h"
#include "op.h"
#include "error.h"
#include "init.h"
#include <unistd.h>

/* using dynamic linking, C linkage is preferred */
extern "C" {

extern volatile int buf_changed;
extern int write_op;

#define DEFAULT_PS ">> "
static string ps = DEFAULT_PS;

static inline void shellPrompt() {
    if (isatty(STDERR_FILENO)) {
        write(STDERR_FILENO, ps.c_str(), ps.size());
    }
}

static void enqueuePos(op_t &p) {
    if (front_end_number_version > 1) {
        buf_changed = 1;
        queuedOp(1, &p);
    } 
}

/* the front-end function */
void frontEndCli(textOp &file, istream &in) {
    string line, cmd, data;
    op_t op, saved_op;
    pos_t tmp_pos;
    char delete_char;
    for (shellPrompt(); getline(in, line); shellPrompt()) {
        if (line.size() == 0) {
            continue;
        }
        stringstream ss(line);
        cmd.clear();
        data.clear();
        memset(&op, -1, sizeof(op));
        uint64_t offset = (uint64_t)-1;
        ss >> cmd;
        if (cmd.size() && isupper(cmd[0])) {
            ss >> offset >> data;
        }
        else {
            /* attribute packed cannot bind to reference type */
            //ss >> op.pos.lineno >> op.pos.offset >> data;
            memset(&tmp_pos, -1, sizeof(tmp_pos));
            ss >> tmp_pos.lineno >> tmp_pos.offset >> data;
            op.pos = tmp_pos;
        }
        if (cmd.size() != 1 || ((data.size() > 2
            || (op.pos.lineno == -1 && offset == (uint64_t)-1))
            && cmd[0] != SAVE && cmd[0] != PRINT))
        {
            errMsg("frontEndCli: syntax error: " + line);
            continue;
        }
        op.operation = cmd[0];
        string msg;
        saved_op = op;
        switch (op.operation) {
            case CH_DELETE:
                msg = file.deleteCharAt(offset, &delete_char, &saved_op.pos);
                PROMPT_ERROR(msg);
                if (msg == NOERR) {
                    op.data = delete_char;
                    op.char_offset = offset;
                    saved_op.data = delete_char;
                    saved_op.operation = DELETE;
                    enqueuePos(saved_op);
                    if (write_op) {
                        writeOpFifo(op);
                    }
                }
                break;
            case DELETE: {
                msg = file.deleteChar(op.pos, &delete_char);
                PROMPT_ERROR(msg);
                if (msg == NOERR) {
                    op.data = delete_char;
                    enqueuePos(op);
                    if (write_op) {
                        writeOpFifo(op);
                    }
                }
                break;
            }
            case CH_INSERT:
                op.pos = file.translateOffset(offset);
                saved_op.char_offset = offset;
                if (op.pos.lineno < 1) {
                    PROMPT_ERROR("translateOffset: offset out of range "
                                 + to_string(offset));
                    break;
                }
            case INSERT: {
                if (!data.size() 
                    || (data[0] != '\\' && data.size() != 1)) 
                {
                    errMsg("frontEndCli: syntax error: " + line);
                    continue;
                }
                if (data[0] == '\\' && data.size() == 2) {
                    const static char *esc = "abfnrtv";
                    const static char *raw = "\a\b\f\n\r\t\v";
                    const char *pos = strchr(esc, data[1]);
                    if (pos == NULL) {
                        errMsg("frontEndCli: No such escaped character: "
                               + data);
                        continue;
                    }
                    data[0] = *((unsigned long)pos
                                - (unsigned long)esc + raw);
                }
                op.data = data[0];
                saved_op.data = data[0];
                msg = file.insertChar(op.pos, (char)op.data);
                PROMPT_ERROR(msg);
                if (msg == NOERR) {
                    op.operation = INSERT;
                    enqueuePos(op);
                    if (write_op) {
                        writeOpFifo(saved_op);
                    }
                }
                break;
            }
            case PRINT: {
                if (op.pos.lineno < 1) {
                    op.pos.lineno = 1;
                }
                file.printLines(op.pos.lineno, op.pos.offset);
                break;
            }
            case SAVE: {
                stringstream ss2(line);
                ss2 >> cmd >> data;
                msg = file.saveFile(data);
                PROMPT_ERROR(msg);
                buf_changed = 1;
                break;
            }
            default: {
                errMsg("frontEndCli: no such operation '" + cmd + "'");
            }
        }
    }
}

}
