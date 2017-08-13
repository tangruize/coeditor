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
#include <sys/time.h>

#include "common.h"
#include "xform.h"
#include "op.h"

//#define DEBUG

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
queue<trans_t> to_recv;
int sleep_before_send = 0;
int sleep_before_recv = 0;

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

enum {LO_CHECK = 1, RE_CHECK = 2};
static int error_check = LO_CHECK | RE_CHECK;
void errorCheck(int fd, const op_t &orig_op) {
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
        cerr << "Error: checking error: " << strerror(errno)
             << " (fd: " << fd << ")" << endl;
    }
    else {
        /* should not happen */
        cerr << "WARNING: error check data unrecognized (fd: "
             << fd << ")" << endl;
    }
}

void writeServer(const trans_t &msg) {
    if (write(TO_SERVER_FILENO, &msg, sizeof(msg)) != -1) {
        if (error_check & RE_CHECK) {
            errorCheck(SERVER_FEEDBACK_FILENO, msg.op);
        }
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
    #ifdef DEBUG
    cerr << "state: " << msg.state << ", ";
    printError(msg.op);
    #endif
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
        #ifdef DEBUG
        cerr << "---before ot: ";
        printError(msg.op, false);
        cerr << " AND ";
        printError((*i).op);
        #endif
        /* Transform new message and the ones in the queue. */
        if (xformClient(msg.op, (*i).op) != 0) {
            #ifdef DEBUG
            cerr << "---after  ot: ";
            printError(msg.op, false);
            cerr << " AND ";
            printError((*i).op);
            #endif
            break;
        }
        #ifdef DEBUG
        cerr << "---after  ot: ";
        printError(msg.op, false);
        cerr << " AND ";
        printError((*i).op);
        #endif
    }
    if (msg.op.operation != NOOP) {
        if (write(TO_LOCAL_FILENO, &msg.op, sizeof(msg.op)) != -1) {
            if (error_check & LO_CHECK) {
                errorCheck(LOCAL_FEEDBACK_FILENO, msg.op);
            }
        }
        else {
            cerr << "ERROR: writing local: " << strerror(errno) << endl;
        }
    }
    ++global;
}

/* delay before receive if delay time is set */
void delayReceive(trans_t &msg) {
    if (sleep_before_recv) {
        to_recv.push(msg);
    }
    else {
        Receive(msg);
    }
}

/* error check may receive no data, prevent from blocking */
/* PS: bad idea, error check may receive partial data */
/*
int setNonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        return -1;
    }
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
    return 0;
}
*/

/* if feedback fds is not open or is not ok, error check is disabled */
void initErrorCheck() {
    int i;
    if (read(LOCAL_FEEDBACK_FILENO, &i, sizeof(i)) == -1 || i == 0) {
        error_check &= ~LO_CHECK;
    }
    if (read(SERVER_FEEDBACK_FILENO, &i, sizeof(i)) == -1 || i == 0) {
        error_check &= ~RE_CHECK;
    }
    if (!(error_check & LO_CHECK)) {
        cerr << "WARNING: error check disabled for local op" << endl;
    }
}

/* upload local operations to server immediately */
void synSend() {
    while (to_send.size()) {
        const trans_t &t = to_send.front();
        writeServer(t);
        to_send.pop();
    }
}

/* apply server operations to local immediately */
void synReceive() {
    while (to_recv.size()) {
        const trans_t &t = to_recv.front();
        trans_t msg = t;
        Receive(msg);
        to_recv.pop();
    }
}

void synchronize() {
    synSend();
    synReceive();
}

/* timer for receive event */
void initTimer() {
    if (sleep_before_recv <= 0) {
        return;
    }
    struct timeval tv;
    sleep_before_recv *= 1000;
    tv.tv_sec = sleep_before_recv / 1000000;
    tv.tv_usec = sleep_before_recv % 1000000;
    struct itimerval itv;
    itv.it_interval = itv.it_value = tv;
    if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
        cerr << "ERROR: setitimer: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }
}

void sigHandler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        /* synchronize all and then exit, so that data won't lose */
        synchronize();
        exit(EXIT_SUCCESS);
    }
    if (sig == SIGALRM) {
        synReceive();
    }
}

void initSignal() {
    struct sigaction sa;
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = sigHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1
        || sigaction(SIGTERM, &sa, NULL) == -1
        || sigaction(SIGALRM, &sa, NULL) == -1)
    {
        cerr << "ERROR: sigaction: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }
    /* Ignore the SIGPIPE signal, so that we find out about
     * broken connection errors via a failure from write(). 
     */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        cerr << "ERROR: signal: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "t:T:fF")) != -1) {
        switch (opt) {
            case 't':
                sleep_before_send = atoi(optarg);
                if (sleep_before_send == 0 && optarg[0] != '0') {
                    cerr << "WARNING: Invalid send delay time: "
                         << optarg << endl;
                }
                break;
            case 'T':
                sleep_before_recv = atoi(optarg);
                if (sleep_before_recv == 0 && optarg[0] != '0') {
                    cerr << "WARNING: Invalid receive delay time: "
                         << optarg << endl;
                }
                break;
            case 'f':
                error_check &= ~LO_CHECK;
                break;
            case 'F':
                error_check &= ~RE_CHECK;
                break;
            default:
                break;
        }
    }
    initErrorCheck();
    initSignal();
    initTimer();
    trans_t msg;
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
            /* timeout if no local input, upload */
            synSend();
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
                        else if (msg.op.operation == SEND_SYN) {
                            synSend();
                        }
                        else if (msg.op.operation == RECV_SYN) {
                            synReceive();
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
                        delayReceive(msg);
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
    synchronize();
    return 0;
}
