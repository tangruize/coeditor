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
#include <time.h>
#include <sys/timerfd.h>
#include <sys/time.h>
#include <queue>

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

struct pollfd fds[4] = {
    {FROM_LOCAL_FILENO, POLLIN, 0},
    {FROM_SERVER_FILENO, POLLIN, 0},
    {-1, POLLIN, 0},
    {-1, POLLIN, 0},
};

unsigned local, global;
list<trans_t> outgoing;
queue<trans_t> to_send;
queue<trans_t> to_recv;
double sleep_before_send = 0.0;
double sleep_before_recv = 0.0;

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
    ssize_t num_read;
    if (fd == LOCAL_FEEDBACK_FILENO) {
        num_read = read(fd, &msg, sizeof(op_t));
    }
    else {
        num_read = read(fd, &msg, sizeof(trans_t));
    }
    if (num_read == sizeof(op_t)) {
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
    else if (num_read == sizeof(msg)) {
        if (msg.op.operation < 0) {
            cerr << "FATAL: writing server op: transmission error: ";
            msg.op.operation = -msg.op.operation;
            printError(msg.op);
            cerr << "---state: " << msg.state << endl;
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
    if (sleep_before_send == 0.0) {
        /* synchronize immediately */
        writeServer(msg);
    }
    else {
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
void synReceive();
void delayReceive(trans_t &msg) {
    to_recv.push(msg);
    if (sleep_before_recv == 0.0) {
        synReceive();
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
extern int ot_fd;
void synReceive() {
    op_t tmp;
    memset(&tmp, 0, sizeof(op_t));
    tmp.operation = RECV_SYN;
    if (write(TO_LOCAL_FILENO, &tmp, sizeof(tmp)) == -1) {
        return;
    }
    while (read(FROM_LOCAL_FILENO, &tmp, sizeof(op_t)) == sizeof(op_t)) {
        if (tmp.operation == NOOP) {
            break;
        }
        if (tmp.operation == 'D' || tmp.operation == 'I') {
            Generate(tmp);
        }
        else if (tmp.operation == 'U') {
            synSend();
        }
    }
    while (to_recv.size()) {
        const trans_t &t = to_recv.front();
        trans_t msg = t;
        Receive(msg);
        to_recv.pop();
    }
    memset(&tmp, 0, sizeof(op_t));
    tmp.operation = NOOP;
    write(TO_LOCAL_FILENO, &tmp, sizeof(tmp));
    write(ot_fd, &tmp, sizeof(tmp));
}

void synchronize() {
    synReceive();
    synSend();
}

/* timer for receive event */
void setTimer(double timeout, int *tfd, int i) {
    struct timespec ts;
    struct itimerspec its;
    long s = timeout;
    ts.tv_sec = s;
    timeout -= s;
    timeout *= 1e9;
    s = timeout;
    if (s >= 1000000000) {
        s = 999999999;
    }
    ts.tv_nsec = s;
    its.it_interval = ts;
    its.it_value = ts;
    int fd = timerfd_create(CLOCK_REALTIME, 0);
    if (fd == -1) {
        cerr << "ERROR: timerfd_create: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }
    if (timerfd_settime(fd, 0, &its, NULL) == -1) {
        cerr << "ERROR: timerfd_settime: " << strerror(errno) << endl;
        exit(EXIT_FAILURE);
    }
    *tfd = fd;
    fds[i].fd = fd;
}

int send_timer_fd = -1, recv_timer_fd = -1;
void initTimer() {
    int i = 2;
    if (sleep_before_send > 0) {
        setTimer(sleep_before_send, &send_timer_fd, i++);
    }
    if (sleep_before_recv > 0) {
        setTimer(sleep_before_recv, &recv_timer_fd, i);
    }
}

void sigHandler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        /* synchronize all and then exit, so that data won't lose */
        /* exit immediately if twice signaled */
        signal(sig, SIG_DFL);
        synchronize();
        exit(EXIT_SUCCESS);
    }
}

void initSignal() {
    struct sigaction sa;
    sa.sa_flags = SA_RESTART | SA_NODEFER;
    sa.sa_handler = sigHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1
        || sigaction(SIGTERM, &sa, NULL) == -1)
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

int debug_fd = -1, ot_fd = -1;
void initDebug() {
    char buf[32];
    sprintf(buf, "debug_%ld", (long)getpid());
    debug_fd = open(buf, O_WRONLY | O_CREAT, 0644);
    if (debug_fd == -1) {
        cerr << "open debug fd: " << strerror(errno) << endl;
    }
    sprintf(buf, "ot_%ld", (long)getpid());
    ot_fd = open(buf, O_WRONLY | O_CREAT, 0644);
    if (ot_fd == -1) {
        cerr << "open debug fd: " << strerror(errno) << endl;
    }
}

int main(int argc, char *argv[]) {
    int opt;
    char *endptr;
    while ((opt = getopt(argc, argv, "t:T:fF")) != -1) {
        switch (opt) {
            case 't':
                sleep_before_send = strtod(optarg, &endptr);
                if (*endptr != 0) {
                    sleep_before_send = 0.0;
                    cerr << "WARNING: Invalid send delay time: "
                         << optarg << endl;
                }
                break;
            case 'T':
                sleep_before_recv = strtod(optarg, &endptr);
                if (*endptr != 0) {
                    sleep_before_recv = 0.0;
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
    initDebug();
    trans_t msg;
    uint64_t exp;
    int poll_ret, fd_num = 2;
    for (; fd_num < 4 && fds[fd_num].fd != -1; ++fd_num);
    while ((poll_ret = poll(fds, fd_num, -1)) != -1
           || errno == EINTR)
    {
        if (poll_ret == -1) {
            continue;
        }
        int hup = 0;
        for (int i = 0; i < fd_num; ++i) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == FROM_LOCAL_FILENO) {
                    if (read(fds[i].fd, &msg.op, sizeof(op_t))
                        == sizeof(op_t))
                    {
                        write(debug_fd, &msg.op, sizeof(op_t));
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
                else if (fds[i].fd == send_timer_fd) {
                    /* timeout: send timer
                     * we do not care expirations
                     */
                    //cerr << "send timer" << endl;
                    read(fds[i].fd, &exp, sizeof(uint64_t));
                    synSend();
                }
                else if (fds[i].fd == recv_timer_fd) {
                    /* timeout: recv timer */
                    //cerr << "recv timer" << endl;
                    read(fds[i].fd, &exp, sizeof(uint64_t));
                    synReceive();
                }
            }
            else if (fds[i].revents & POLLHUP) {
                if (fds[i].fd == FROM_LOCAL_FILENO) {
                    hup |= 01;
                }
                else if (fds[i].fd == FROM_SERVER_FILENO) {
                    hup |= 02;
                }
            }
            else if (fds[i].revents & POLLNVAL) {
                cerr << "ERROR: Bad file descriptor: "
                     << fds[i].fd << endl;
                exit(EXIT_FAILURE);
            }
        }
        if (hup == (01 | 02)) {
            cerr << "INFO: Pipe closed, exiting..." << endl;
            break;
        }
    }
    synchronize();
    return 0;
}
