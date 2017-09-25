#include "data_trans.h"

const char *ALGO_VER = "CSCW";

static unsigned local, global;
static deque<trans_t> outgoing;

static void Generate(const op_t &op) {
    trans_t msg;
    msg.op = op;
    msg.state = global;
    writeRemote(msg);
    msg.state = local;
    outgoing.push_back(msg);
    ++local;
}

static void Receive(trans_t msg) {
    /* Discard acknowledged messages. */
    int i;
    for (i = 0; i < outgoing.size(); ++i)
        if (outgoing[i].state >= msg.state) break;
    for (int j = 0; j < i; ++j) outgoing.pop_front();

    /* Transform new message and the ones in the queue. */
    for (i = 0; i < outgoing.size(); ++i)
        if (xform(msg.op, outgoing[i].op) != 0) break;
    writeLocal(msg.op);
    ++global;
}

/* client code */

void procLocal(const op_t &op) {
    Generate(op);
}

void procServer(const trans_t &msg) {
    Receive(msg);
}

/* server code */

void procRemote(const op_t &op) {
    Generate(op);
}

void procClient(const trans_t &msg) {
    Receive(msg);
}