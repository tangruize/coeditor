/*************************************************************************
	> File Name: init.h
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-28 14:19
	> Description: 
 ************************************************************************/

#ifndef _INIT_H
#define _INIT_H

#include <string>
using namespace std;

#define OUT_FILE_PREFIX ".coeditor."
#define DEBUG_FILE_SUFFIX "debug"
#define CLI_INPUT_SUFFIX "cli"
#define OP_OUTPUT_SUFFIX "local.op.output"
#define OP_INPUT_SUFFIX "local.op.input"
#define OP_INPUT_FEEDBACK_SUFFIX "local.op.input.feedback"
#define TO_SERVER_SUFFIX "server.input"
#define TO_SERVER_FEEDBACK_SUFFIX "server.input.feedback"
#define FROM_SERVER_SUFFIX "server.output"
#define LOCK_FILE_SUFFIX "lock"

extern string out_file_prefix_no_pid;
extern string out_file_prefix;
extern string debug_output_file_name;
extern string cli_input_fifo_name;
extern string op_output_fifo_name;
extern string op_input_fifo_name;
extern string op_input_feedback_fifo_name;
extern string to_server_fifo_name;
extern string to_server_feedback_fifo_name;
extern string from_server_fifo_name;
extern string server_addr;
extern string lock_file;
extern string ot_time_arg;
extern string ot_recv_time_arg;
extern bool remove_fifo_ot;
extern bool no_debug;
extern textOp *edit_file;

void removeOutFileAtExit();
void setOutFilenames();
int saveOldCwd();
int gotoEditFileDir();
int restoreOldCwd();
void redirectStderr();
int creatCliInputFifo();
void writeOpFifo(op_t &op);
void init();
string getCliInputFifoName();
const op_t* queuedOp(int enqueue, op_t *op = NULL);

#endif
