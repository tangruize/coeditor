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

/* using dynamic linking, C linkage is preferred */
extern "C" {

#define DEFAULT_PS ">> "
static string ps = DEFAULT_PS;

const char *front_end_version = "CLI 0.1";
const char *front_end_author = "Ruize Tang";

/* the front-end function */
void frontEnd(textOp &file) {
    string line, cmd, data;
    op_t op;
    for (cerr << ps; getline(cin, line); cerr << ps) {
        if (line.size() == 0) {
            continue;
        }
        stringstream ss(line);
        cmd.clear();
        data.clear();
        memset(&op, -1, sizeof(op));
        ss >> cmd >> op.pos.lineno >> op.pos.offset >> data;
        if (cmd.size() != 1 || ((data.size() > 2 
                                || op.pos.lineno == -1) 
                                && cmd[0] != SAVE && cmd[0] != PRINT))
        {
            errMsg("frontEnd: syntax error: " + line);
            continue;
        }
        op.operation = cmd[0];
        string msg;
        switch (op.operation) {
            case DELETE: {
                msg = file.deleteChar(op.pos);
                PROMT_ERROR(msg);
                break;
            }
            case INSERT: {
                if (!data.size() 
                    || (data[0] != '\\' && data.size() != 1)) 
                {
                    errMsg("frontEnd: syntax error: " + line);
                    continue;
                }
                if (data[0] == '\\' && data.size() == 2) {
                    const static char *esc = "abfnrtv";
                    const static char *raw = "\a\b\f\n\r\t\v";
                    const char *pos = strchr(esc, data[1]);
                    if (pos == NULL) {
                        errMsg("frontEnd: No such escaped character: "
                               + data);
                        continue;
                    }
                    data[0] = *((unsigned long)pos
                                - (unsigned long)esc + raw);
                }
                op.data = data[0];
                msg = file.insertChar(op.pos, (char)op.data);
                PROMT_ERROR(msg);
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
                PROMT_ERROR(msg);
                break;
            }
            default: {
                errMsg("frontEnd: no such operation '" + cmd + "'");
            }
        }
    }
}

}
