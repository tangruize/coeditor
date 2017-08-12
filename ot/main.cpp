/*************************************************************************
	> File Name: main.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-08-04 13:45
	> Description: 
 ************************************************************************/

#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <queue>

#include "common.h"
#include "op.h"

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

unsigned local, global;
list<trans_t> outgoing;
queue<trans_t> to_send;
int sleep_before_send = 0;

int xform(op_t &op, op_t &outop) {
    if (op.operation == 0) {
        return 1;
    }
    else if (outop.operation == 0) {
        return 0;
    }
    if (op.operation == CH_DELETE && outop.operation == CH_DELETE) {
        if (op.char_offset > outop.char_offset) {
            --op.char_offset;
        }
        else if (op.char_offset < outop.char_offset) {
            --outop.char_offset;
        }
        else /* if (op.char_offset == outop.char_offset) */ {
            /* delete same char, do nothing */
            op.operation = NOOP;
            outop.operation = NOOP;
            return 1;
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
        if (op.char_offset > outop.char_offset) {
            --op.char_offset;
        }
        else /* if (op.char_offset <= outop.char_offset) */ {
            ++outop.char_offset;
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
                /* should never happen! */
                cerr << "WARNING: IDs should not be same" << endl;
            }
        }
        else /* if (op.data == outop.data
                 && op.char_offset == outop.char_offset) */
        {
            /* insert same char, do nothing */
            op.operation = NOOP;
            outop.operation = NOOP;
            return 1;
        }
    }
    else {
        /* unknown operations */
        cerr << "WARNING: unsupported operations: " 
             << (char)op.operation << ", " << (char)outop.operation
             << endl;
    }
    return 0;
}

void printError(const op_t &op) {
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
//    cerr << endl;
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
            cerr << "FATAL: writing server op: transmission error: ";
            msg.op.operation = -msg.op.operation;
            printError(msg.op);
            cerr << "---state: " << msg.state << endl;
        }
    }
    else if (num_read == sizeof(op_t)) {
        if (msg.op.operation < 0) {
            msg.op.operation = -msg.op.operation;
            if (msg.op.operation == 'I' || msg.op.operation == 'i'
                || msg.op.operation == 'D' || msg.op.operation == 'd')
            {
                cerr << "FATAL: writing local op: offset out of range: ";
                printError(msg.op);
            }
            else {
                cerr << "FATAL: writing local op: invalid operation: ";
                cerr.setf(ios_base::hex | ios_base::uppercase);
                for (int i = 0; i < sizeof(op_t); ++i) {
                    cerr << setw(2) << setfill('0') << (unsigned)((char *)&msg)[i];
                    if (i % 2) {
                        cerr << ' ';
                    }
                }
                cerr << endl;
                cerr.unsetf(ios_base::hex);
            }
        }
        else if ((msg.op.operation == CH_DELETE
                 || msg.op.operation == DELETE)
                && msg.op.data != orig_op.data)
        {
            cerr << "WARNING: data request to delete is different "
                    "from what really deleted\n---ORIGINAL: ";
            printError(orig_op);
            cerr << "---FEEDBACK: ";
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

void writeServer(const trans_t &msg) {
    if (write(TO_SERVER_FILENO, &msg, sizeof(msg)) != -1) {
        errorCheck(SERVER_FEEDBACK_FILENO, msg.op);
    }
    else {
        cerr << "ERROR: writing server: " << strerror(errno) << endl;
    }
}

void Generate(const op_t &op) {
    trans_t msg;
    msg.op = op;
    msg.state = global;
    if (sleep_before_send == 0) {
        /* synchronize immediately */
        writeServer(msg);
    }
    else {
        /* wait for sync event */
        to_send.push(msg);
    }
    msg.state = local;
    outgoing.push_back(msg);
    ++local;
}

void Receive(trans_t &msg) {
    cerr << "state: " << msg.state << ", ";
    printError(msg.op);
    cerr << endl;
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
    /* ASSERT msg.myMsgs == otherMsgs. */
    /*if (msg.state.client != state.server) {
        cerr << "WARNING: msg.state.client (" << msg.state.client
             << ") != state.server (" << state.server << ")\n";
    }*/
    for (list<trans_t>::iterator i = outgoing.begin();
         i != outgoing.end(); ++i)
    {
        cerr << "---before ot: ";
        printError(msg.op);
        cerr << " AND ";
        printError((*i).op);
        cerr << endl;
        /* Transform new message and the ones in the queue. */
        if (xform(msg.op, (*i).op) != 0) {
            cerr << "---after  ot: ";
            printError(msg.op);
            cerr << " AND ";
            printError((*i).op);
            cerr << endl;
            break;
        }
        cerr << "---after  ot: ";
        printError(msg.op);
        cerr << " AND ";
        printError((*i).op);
        cerr << endl;
    }
    if (msg.op.operation != NOOP) {
        if (write(TO_LOCAL_FILENO, &msg.op, sizeof(msg.op)) != -1) {
            errorCheck(LOCAL_FEEDBACK_FILENO, msg.op);
        }
        else {
            cerr << "ERROR: writing local: " << strerror(errno) << endl;
        }
    }
    ++global;
}

int setNonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        return -1;
    }
    flags &= ~O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
    return 0;
}

void initErrorCheck() {
    int errfds[] = {LOCAL_FEEDBACK_FILENO, SERVER_FEEDBACK_FILENO};
    for (int i = 0; i < SIZEOFARRAY(errfds); ++i) {
        if (setNonblock(errfds[i]) != 0) {
            error_check_disabled = true;
            cerr << "WARNING: error check disabled" << endl;
            return;
        }
    }
}

void synchronize() {
    while (to_send.size()) {
        const trans_t &t = to_send.front();
        writeServer(t);
        to_send.pop();
    }
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
            case 't':
                sleep_before_send = atoi(optarg);
                if (sleep_before_send == 0 && optarg[0] != '0') {
                    cerr << "WARNING: Invalid sleep time: "
                         << optarg << endl;
                }
                break;
            default:
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
    int poll_ret, timeout;
    if (sleep_before_send <= 0) {
        timeout = -1;
    }
    else {
        timeout = sleep_before_send;
    }
    while ((poll_ret = poll(fds, SIZEOFARRAY(fds), timeout)) != -1
           || errno == EINTR) {
        if (poll_ret == 0) {
            /* timeout */
            synchronize();
            continue;
        }
        int hup = 0;
        for (int i = 0; i < SIZEOFARRAY(fds); ++i) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == FROM_LOCAL_FILENO) {
                    if (read(fds[i].fd, &msg.op, sizeof(op_t))
                        == sizeof(op_t))
                    {
                        if (msg.op.operation == SYN) {
                            /* user synchronism signal */
                            synchronize();
                        }
                        else {
                            Generate(msg.op);
                        }
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
