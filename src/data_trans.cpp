#include "data_trans.h"
#include "text_mutex.h"
#include <unistd.h>

extern textOp *edit_file;
extern queue<op_t> *pos_to_transform;
extern int buf_changed;
extern int socket_fd;
extern queue<trans_t> to_send;
extern double sleep_before_send;

void printError(const op_t &op, bool newline = true) {
    if (isalpha(op.operation)) {
        cerr << (char)op.operation << " ";
    }
    else {
        cerr << (unsigned)op.operation << " ";
    }
    if (islower(op.operation)) {
        cerr << op.pos.lineno << " " << op.pos.offset << " ";
    }
    else {
        cerr << op.char_offset << " ";
    }
    if (isprint(op.data)) {
        cerr << op.data;
    }
    else {
        cerr << "0x" << hex << uppercase << (unsigned)op.data;
        cerr.unsetf(ios_base::hex);
    }
    if (newline) {
        cerr << endl;
    }
}

/* write an OTed op to editing file */
void writeLocal(const op_t &arg_op) {
    op_t op = arg_op;
    op_t q_op = op;
    int flags = TM_NOLOCK;
    string op_result;
    switch (op.operation) {
        case DELETE:
            op_result = edit_file->deleteChar(op.pos,
                                   &op.data, NULL, &flags);
            break;
        case CH_DELETE:
            op_result = edit_file->deleteCharAt(op.char_offset, 
                                   &op.data, &q_op.pos, &flags);
            q_op.operation = DELETE;
            break;
        case INSERT:
            op_result = edit_file->insertChar(op.pos,
                                   op.data, NULL, &flags);
            break;
        case CH_INSERT:
            op_result = edit_file->insertCharAt(op.char_offset,
                                   op.data, &q_op.pos, &flags);
            q_op.operation = INSERT;
            break;
        case NOOP:
            break;
        default:
            op_result = "FAILED";
    }
    if (op.operation != NOOP && op_result == NOERR) {
        pos_to_transform->push(q_op);
        buf_changed = 1;
    }
    if (op_result != NOERR) {
        cerr << op_result << endl;
    }
    else if (op.data != arg_op.data) {
        cerr << "delete: different: ";
        printError(arg_op, false);
        cerr << " -> ";
        printError(op, true);
    }
}

void doWriteRemote(const trans_t &msg) {
    if (write(socket_fd, &msg, sizeof(msg)) != sizeof(msg)) {
        PROMPT_ERROR_EN("write server");
    }
}

/* write a message to server */
void writeRemote(const trans_t &msg) {
    if (sleep_before_send == 0.0) {
        doWriteRemote(msg);
    }
    else {
        to_send.push(msg);
    }
}