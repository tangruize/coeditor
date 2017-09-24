#include "common.h"
#include "op.h"
#include "xform.h"

/* client code */

/* procLocal() will be called if a new op generated */
void procLocal(const op_t &op);

/* procServer() will be called if a remote message received */
void procServer(const trans_t &msg);


/* server code */

/* procRemote() will be called if server receives msgs from other servers */
void procRemote(const op_t &op);

/* procRemote() will be called if server receives msgs from its client */
void procClient(const trans_t &msg);


/* shared */

/* write an op to its buf (client or server locally) */
void writeLocal(const op_t &op);

/* write a message to the other end */
void writeRemote(const trans_t &msg);
