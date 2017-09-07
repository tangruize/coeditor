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

int getInsertData(op_t &op, string &data, const string &line) {
    if (!data.size() 
        || (data[0] != '\\' && data.size() != 1)) 
    {
        errMsg("frontEndCli: syntax error: " + line);
        return -1;
    }
    if (data[0] == '\\' && data.size() == 2) {
        const static char *esc = "abfnrtv";
        const static char *raw = "\a\b\f\n\r\t\v";
        const char *pos = strchr(esc, data[1]);
        if (pos == NULL) {
            errMsg("frontEndCli: No such escaped character: "
                   + data);
            return -1;
        }
        data[0] = *((unsigned long)pos
                    - (unsigned long)esc + raw);
    }
    op.data = data[0];
    return 0;
}

/* the front-end function */
void frontEndCli(textOp &file, istream &in) {
    string line, cmd, data;
    op_t op, saved_op;
    pos_t tmp_pos;
    char delete_char;
    int flags = write_op ? (write_op_pos ? TM_WROP : TM_WROPOFF) : 0;
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
            && cmd[0] != SAVE && cmd[0] != PRINT && cmd[0] != 'l' 
            && cmd[0] != 'u' && cmd[0] != 'r'))
        {
            errMsg("frontEndCli: syntax error: " + line);
            continue;
        }
        op.operation = cmd[0];
        string msg;
        saved_op = op;
        switch (op.operation) {
            case 'l':
                memset(&saved_op, 0, sizeof(saved_op));
                saved_op.operation = SYN;
                writeOpFifo(saved_op);
                break;
            case 'u':
                memset(&saved_op, 0, sizeof(saved_op));
                saved_op.operation = SEND_SYN;
                writeOpFifo(saved_op);
                break;
            case 'r':
                memset(&saved_op, 0, sizeof(saved_op));
                saved_op.operation = RECV_SYN;
                writeOpFifo(saved_op);
                break;
            case CH_DELETE:
                msg = file.deleteCharAt(offset, &delete_char,
                                        &saved_op.pos, &flags);
                if (msg == NOERR) {
                    saved_op.data = delete_char;
                    saved_op.operation = DELETE;
                    enqueuePos(saved_op);
                }
                else {
                    PROMPT_ERROR(msg);
                }
                break;
            case DELETE:
                msg = file.deleteChar(op.pos, &delete_char,
                                      NULL, &flags);
                if (msg == NOERR) {
                    op.data = delete_char;
                    enqueuePos(op);
                }
                else {
                    PROMPT_ERROR(msg);
                }
                break;
            case CH_INSERT:
                if (getInsertData(op, data, line) != 0) {
                    continue;
                }
                msg = file.insertCharAt(offset, op.data,
                                        &saved_op.pos, &flags);
                if (msg == NOERR) {
                    saved_op.data = op.data;
                    saved_op.operation = INSERT;
                    enqueuePos(saved_op);
                }
                else {
                    PROMPT_ERROR(msg);
                }
                break;
            case INSERT:
                if (getInsertData(op, data, line) != 0) {
                    continue;
                }
                msg = file.insertChar(op.pos, op.data,
                                      NULL, &flags);
                if (msg == NOERR) {
                    enqueuePos(op);
                }
                else {
                    PROMPT_ERROR(msg);
                }
                break;
            case PRINT:
                if (op.pos.lineno < 1) {
                    op.pos.lineno = 1;
                }
                file.printLines(op.pos.lineno, op.pos.offset);
                break;
            case SAVE: {
                stringstream ss2(line);
                ss2 >> cmd >> data;
                msg = file.saveFile(data);
                PROMPT_ERROR(msg);
                buf_changed = 1;
                break;
            }
            default:
                errMsg("frontEndCli: no such operation '" + cmd + "'");
        }
    }
}

}
