/*************************************************************************
	> File Name: main.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-08-04 13:45
	> Description: 
 ************************************************************************/

#include "common.h"
#include "op.h"
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>

#define FROM_LOCAL_FILENO      0
#define TO_LOCAL_FILENO        1
/* do not override STDERR_FILENO */
#define FROM_SERVER_FILENO     3
#define TO_SERVER_FILENO       4
#define LOCAL_FEEDBACK_FILENO  5
#define SERVER_FEEDBACK_FILENO 6

#define SIZEOFARRAY(a) (sizeof(a) / sizeof(a[0]))

struct pollfd fds[] = {
    {FROM_LOCAL_FILENO, POLLIN, 0},
    {FROM_SERVER_FILENO, POLLIN, 0},
};

state_t state;
list<trans_t> outgoing;
unsigned sleep_before_send = 0;

void xform(op_t &op, op_t &outop) {
    if (op.operation == CH_DELETE && outop.operation == CH_DELETE) {
        if (op.char_offset > outop.char_offset) {
            --op.char_offset;
        }
        else if (op.char_offset < outop.char_offset) {
            --outop.char_offset;
        }
        else /* if (op.char_offset == outop.char_offset) */ {
            /* delete same char, do nothing */
        }
    }
    else if (op.operation == CH_DELETE && outop.operation == CH_INSERT)
    {
        if (op.char_offset >= outop.char_offset) {
            ++op.char_offset;
        }
        else /* if (op.char_offset < outop.char_offset) */ {
            --outop.char_offset;
        }
    }
    else if (op.operation == CH_INSERT && outop.operation == CH_DELETE)
    {
        if (op.char_offset >= outop.char_offset) {
            --op.char_offset;
        }
        else /* if (op.char_offset < outop.char_offset) */ {
            --outop.char_offset;
        }
        
    }
    else if (op.operation == CH_INSERT && outop.operation
             == CH_INSERT)
    {
        if (op.char_offset > outop.char_offset) {
            ++op.char_offset;
        }
        else if (op.char_offset < outop.char_offset) {
            ++outop.char_offset;
        }
        else if (op.data != outop.data
                 /* && op.char_offset == outop.char_offset */ )
        {
            /* Insert different chars at different offset */
            if (op.id > outop.id) {
                ++op.char_offset;
            }
            else if (op.id < outop.id) {
                ++outop.char_offset;
            }
            else {
                /* should not happen! */
                cerr << "WARNING: IDs should not be same" << endl;
            }
        }
        else /* if (op.data == outop.data
                 && op.char_offset == outop.char_offset) */
        {
            /* insert same char, do nothing */
        }
    }
    else {
        /* unknown operations */
        cerr << "WARNING: unsupported operations: " 
             << (char)op.operation << ", " << (char)outop.operation
             << endl;
    }
}

void printError(const op_t &op) {
    cerr << "---operation: " << (char)-op.operation;
    if (isupper(-op.operation)) {
        cerr << "\n---offset: " << op.char_offset << endl;
    }
    else {
        cerr << "\n---pos: (" << op.pos.lineno << ", "
                     << op.pos.offset << ")\n";
    }
    cerr << "---data: '";
    if (isprint(op.data)) {
        cerr << op.data << "'\n";
    }
    else {
        cerr << "0x" << hex << uppercase << (unsigned)op.data << "'\n";
        cerr.unsetf(ios_base::hex);
    }
}

static bool error_check_disabled = false;
void errorCheck(int fd, const op_t &orig_op) {
    if (error_check_disabled) {
        return;
    }
    trans_t msg;
    ssize_t num_read = read(fd, &msg, sizeof(msg));
    if (num_read == sizeof(msg)) {
        if (msg.op.operation < 0) {
            cerr << "FATAL: writing server op: transmission error\n";
            printError(msg.op);
        }
        cerr << "---state: (" << msg.state.client << ", "
             << msg.state.client << ")" << endl;
    }
    else if (num_read == sizeof(op_t)) {
        if (msg.op.operation < 0) {
            cerr << "FATAL: writing local op: invalid operation or "
                    "offset out of range\n";
            printError(msg.op);
        }
        else if ((msg.op.operation == CH_DELETE
                 || msg.op.operation == DELETE)
                && msg.op.data != orig_op.data)
        {
            cerr << "WARNING: data request to delete is different "
                    "from what really deleted\n---ORIGINAL---\n";
            printError(orig_op);
            cerr << "---FEEDBACK---\n";
            printError(msg.op);
        }
    }
    else if (num_read == -1) {
        if (errno == EAGAIN) {
            cerr << "WARNING: no error check data received (fd: "
                 << fd << ")" << endl;
        }
        else {
            cerr << "Error: checking error: " << strerror(errno)
                 << " (fd: " << fd << ")" << endl;
        }
    }
    else {
        /* should not happen */
        cerr << "WARNING: error check data unrecognized (fd: "
             << fd << ")" << endl;
    }
}

void Generate(op_t &op) {
    trans_t msg;
    msg.op = op;
    msg.state = state;
    if (sleep_before_send) {
        sleep(sleep_before_send);
    }
    if (write(TO_SERVER_FILENO, &msg, sizeof(msg)) != -1) {
        errorCheck(SERVER_FEEDBACK_FILENO, msg.op);
    }
    else {
        cerr << "ERROR: writing server: " << strerror(errno) << endl;
    }
    outgoing.push_back(msg);
    ++state.client;
}

void Receive(trans_t &msg) {
    /* Discard acknowledged messages. */
    for (list<trans_t>::iterator m = outgoing.begin();
         m != outgoing.end(); ++m)
    {
        if (m->state.client < msg.state.server) {
            list<trans_t>::iterator pre = m;
            --pre;
            outgoing.erase(m);
            m = pre;
        }
    }
    /* ASSERT msg.myMsgs == otherMsgs. */
    if (msg.state.client != state.server) {
        cerr << "WARNING: msg.state.client != state.server" << endl;
    }
    for (list<trans_t>::iterator i = outgoing.begin();
         i != outgoing.end(); ++i)
    {
        /* Transform new message and the ones in the queue. */
        xform(msg.op, (*i).op);
    }
    if (write(TO_LOCAL_FILENO, &msg.op, sizeof(msg.op)) != -1) {
        errorCheck(LOCAL_FEEDBACK_FILENO, msg.op);
    }
    else {
        cerr << "ERROR: writing local: " << strerror(errno) << endl;
    }
    ++state.server;
}

void initErrorCheck() {
    int errfds[] = {LOCAL_FEEDBACK_FILENO, SERVER_FEEDBACK_FILENO};
    for (int i = 0; i < SIZEOFARRAY(errfds); ++i) {
        int flags = fcntl(errfds[i], F_GETFL);
        if (flags == -1) {
            error_check_disabled = true;
            cerr << "WARNING: error check disabled" << endl;
            return;
        }
        flags &= ~O_NONBLOCK;
        fcntl(errfds[i], F_SETFL, flags);
    }
}

int main(int argc, char *argv[]) {
    int opt;
    int time;
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
            case 't':
                time = atoi(optarg);
                if (time > 0) {
                    sleep_before_send = time;
                }
                else {
                    cerr << "Invalid sleep time: " << optarg << endl;
                    exit(EXIT_FAILURE);
                }
                break;
        }
    }
    initErrorCheck();
    trans_t msg;
    /* Ignore the SIGPIPE signal, so that we find out about
     * broken connection errors via a failure from write(). 
     */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        cerr << "ERROR: signal: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }
    while (poll(fds, SIZEOFARRAY(fds), -1) != -1 || errno == EINTR) {
        int hup = 0;
        for (int i = 0; i < SIZEOFARRAY(fds); ++i) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == FROM_LOCAL_FILENO) {
                    if (read(fds[i].fd, &msg.op, sizeof(op_t))
                        == sizeof(op_t))
                    {
                        Generate(msg.op);
                    }
                }
                else if (fds[i].fd == FROM_SERVER_FILENO) {
                    if (read(fds[i].fd, &msg, sizeof(trans_t))
                        == sizeof(trans_t))
                    {
                        Receive(msg);
                    }
                }
            }
            else if (fds[i].revents & POLLHUP) {
                ++hup;
            }
            else if (fds[i].revents & POLLNVAL) {
                cerr << "ERROR: Bad file descriptor: "
                     << fds[i].fd << endl;
                exit(EXIT_FAILURE);
            }
        }
        if (hup == SIZEOFARRAY(fds)) {
            cerr << "INFO: Pipe closed, exiting..." << endl;
            break;
        }
    }
    return 0;
}
