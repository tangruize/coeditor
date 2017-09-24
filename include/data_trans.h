/* WARNING: declarations in this file should not be called */
/* why extern "C": Because of C++ name mangling */

#include "common.h"
#include "op.h"
#include "xform.h"

/* title will show this */
extern const char *ALGO_VER;

/* client code */

/* procLocal() will be called if a new op generated */
extern "C" void procLocal(const op_t &op);

/* procServer() will be called if a remote message received */
extern "C" void procServer(const trans_t &msg);


/* server code */

/* procRemote() will be called if server receives msgs from other servers */
extern "C" void procRemote(const op_t &op);

/* procRemote() will be called if server receives msgs from its client */
extern "C" void procClient(const trans_t &msg);


/* shared */

/* write an op to its buf (client or server locally) */
extern "C" void writeLocal(const op_t &op);

/* write a message to the other end */
extern "C" void writeRemote(const trans_t &msg);