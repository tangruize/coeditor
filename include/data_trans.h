#include "common.h"

/* procLocal() will be called if a new op generated */
void procLocal(const op_t &op);

/* procServer() will be called if a remote message received */
void procServer(const trans_t &msg);


/* write an OTed op to editing file */
void writeLocal(const op_t &op);

/* write a message to server */
void writeServer(const trans_t &msg);
