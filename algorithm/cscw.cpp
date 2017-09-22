
#include "data_trans.h"

unsigned local, global;
list<trans_t> outgoing;

void procLocal(const op_t &op) {
    trans_t msg;
    msg.op = op;
    msg.state = global;
    writeServer(msg);
    msg.state = local;
    outgoing.push_back(msg);
    ++local;
}

void procServer(const trans_t &msg) {
    /* Discard acknowledged messages. */
    for (list<trans_t>::iterator m = outgoing.begin();
         m != outgoing.end(); ++m)
    {
        if (m->state < msg.state) {
            list<trans_t>::iterator pre = m;
            --pre;
            outgoing.erase(m);
            m = pre;
        }
    }
    for (list<trans_t>::iterator i = outgoing.begin();
         i != outgoing.end(); ++i)
    {
        /* Transform new message and the ones in the queue. */
        if (xformClient(msg.op, (*i).op) != 0) {
            break;
        }
    }
    if (msg.op.operation != NOOP) {
    	writeLocal(msg.op);
    }
    ++global;
}