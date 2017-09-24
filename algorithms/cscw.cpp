#include "data_trans.h"

const char *ALGO_VER = "CSCW";

static unsigned local, global;
static list<trans_t> outgoing;

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
    list<trans_t>::iterator m = outgoing.begin();
    for (; m != outgoing.end(); ++m)
        if (m->state >= msg.state) break;
    outgoing.erase(outgoing.begin(), m);

    /* Transform new message and the ones in the queue. */
    for (m = outgoing.begin(); m != outgoing.end(); ++m)
        if (xform(msg.op, (*m).op) != 0) break;
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