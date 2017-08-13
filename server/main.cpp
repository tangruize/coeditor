/*************************************************************************
	> File Name: multi_client_server.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-08-01 11:28
	> Description: 
 ************************************************************************/

#include "op.h"
#include "rdwrn.h"
#include "common.h"
#include "xform.h"
#include <set>
#include <syslog.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/types.h>

//#define DEBUG

string filename = "/tmp/jupiter-server-";
auth_t auth;
pid_t prog_id;
int readfd = -1, writefd = -1;
unsigned local, global;
list<trans_t> outgoing;

int xform(op_t &op, op_t &outop);

void getFileLock() {
    flock(readfd, LOCK_EX);
}

void releaseFileLock() {
    flock(readfd, LOCK_UN);
}

void login() {
    static int retry_times = 1;
    readfd = open(filename.c_str(), O_RDWR);
    if (readfd == -1) {
        /* file probably not created */
        if (errno != ENOENT) {
            /* other failure */
            exit(EXIT_FAILURE);
        }
        /* file does not exist, create it exclusively */
        readfd = open(filename.c_str(), O_RDWR | O_EXCL | O_CREAT, 0600);
        if (readfd == -1) {
            if (errno == EEXIST) {
                /* file exists, race condition */
                if (retry_times) {
                    --retry_times;
                    struct timespec ts;
                    ts.tv_sec = 0;
                    ts.tv_nsec = 1000;
                    /* sleep a while */
                    nanosleep(&ts, NULL);
                    login();
                    return;
                }
                else {
                    /* should not happen */
                    exit(EXIT_FAILURE);
                }
            }
            else {
                /* other failure */
                exit(EXIT_FAILURE);
            }
        }
        /* file is surely created by us, truncate file */
        if (ftruncate(readfd, 0) == -1) {
            exit(EXIT_FAILURE);
        }
        if (ftruncate(readfd, SHM_OFFSET) == -1) {
            exit(EXIT_FAILURE);
        }
    }
    writefd = open(filename.c_str(), O_WRONLY | O_APPEND);
    if (writefd == -1) {
        exit(EXIT_FAILURE);
    }
    /* ensure file is writed by one process at a time */
    getFileLock();
    int n = 0;
    if (read(readfd, &n, sizeof(n)) == 0 || n == 0) {
        if (ftruncate(readfd, 0) == -1) {
            exit(EXIT_FAILURE);
        }
        if (ftruncate(readfd, SHM_OFFSET) == -1) {
            exit(EXIT_FAILURE);
        }
    }
    if (n == USER_MAX) {
        exit(EXIT_FAILURE);
    }
    file_id_t fi, tmp;
    fi.client_id = auth.id;
    fi.server_id = prog_id;
    int i = 0;
    for (; i < USER_MAX; ++i) {
        if (read(readfd, &tmp, sizeof(tmp)) != sizeof(tmp)) {
            exit(EXIT_FAILURE);
        }
        if (tmp.client_id == fi.client_id) {
            /* same id not allowed */
            exit(EXIT_FAILURE);
        }
        if (tmp.server_id == 0) {
            break;
        }
    }
    for (int j = i + 1; j < USER_MAX; ++j) {
        if (read(readfd, &tmp, sizeof(tmp)) != sizeof(tmp)) {
            exit(EXIT_FAILURE);
        }
        if (tmp.client_id == fi.client_id) {
            /* same id not allowed */
            exit(EXIT_FAILURE);
        }   
    }
    if (i == USER_MAX) {
        /* should not happen */
        exit(EXIT_FAILURE);
    }
    pwrite(readfd, &fi, sizeof(fi), sizeof(n) + sizeof(fi) * i);
    ++n;
    pwrite(readfd, &n, sizeof(n), 0);
    releaseFileLock();
    lseek(readfd, SHM_OFFSET, SEEK_SET);
}

void logout() {
    /* _exit() in this function should not be called */
    lseek(readfd, 0, SEEK_SET);
    getFileLock();
    int n = 0;
    read(readfd, &n, sizeof(n));
    --n;
    if (n < 0) {
        _exit(EXIT_FAILURE);
    }
    file_id_t tmp;
    int i = 0;
    for (; i < USER_MAX; ++i) {
        if (read(readfd, &tmp, sizeof(tmp)) != sizeof(tmp)) {
            _exit(EXIT_FAILURE);
        }
        if (tmp.server_id == prog_id) {
            break;
        }
    }
    if (i == USER_MAX) {
        _exit(EXIT_FAILURE);
    }
    /* clear the seat */
    memset(&tmp, 0, sizeof(tmp));
    pwrite(readfd, &tmp, sizeof(tmp), sizeof(n) + sizeof(tmp) * i);
    pwrite(readfd, &n, sizeof(n), 0);
    releaseFileLock();
}

void logExitInfo() {
    syslog(LOG_INFO , "Jupiter server exiting [PID %ld]", (long)prog_id);
}

/* send signal to any other servers */
void notifyOtherServers() {
    file_id_t fi;
    int n;
    pread(readfd, &n, sizeof(n), 0);
    --n;
    for (int i = 0; i < USER_MAX && n; ++i) {
        pread(readfd, &fi, sizeof(fi), sizeof(int) + sizeof(fi) * i);
        if (fi.server_id != prog_id && fi.server_id != 0) {
            kill(fi.server_id, SIGHUP);
            --n;
        }
    }
}

void writeClient(int sig) {
    trans_t t;
    ssize_t num_read;
    while ((num_read = read(readfd, &t.op, sizeof(op_t)))
           == sizeof(op_t))
    {
        if (t.op.id != auth.id) {
            t.state = local;
            writen(STDOUT_FILENO, &t, sizeof(t));
            t.state = global;
            outgoing.push_back(t);
            ++global;
        }
    }
    if (num_read != 0) {
        if (num_read == -1) {
            syslog(LOG_ERR, "read op file: %s", strerror(errno));
        }
        else {
            /* race condition: read partial data, "put back" */
            lseek(readfd, -num_read, SEEK_CUR);
        }
    }
}

void sigExitHandler(int sig) {
    /* call exit() so that we can logout properly */
    exit(EXIT_SUCCESS);
}

#ifdef DEBUG
#include <sstream>
const char *strop(const op_t &op, char *buf2 = NULL) {
    /* static buf, not reentrant. */
    static char buf[32];
    stringstream ss;
    if (isalpha(op.operation)) {
        ss << (char)op.operation;
    }
    else {
        ss << (unsigned)op.operation;
    }
    ss << " ";
    if (islower(op.operation)) {
        ss << op.pos.lineno << " " << op.pos.offset << " ";
    }
    else {
        ss << op.char_offset << " ";
    }
    ss << op.data;
    if (buf2) {
        /* reentrant. */
        ss.getline(buf2, 31);
        return buf2;
    }
    else {
        ss.getline(buf, 31);
        return buf;
    }
}
#endif

void transform(trans_t &loc) {
    /* Discard acknowledged messages. */
    for (list<trans_t>::iterator m = outgoing.begin();
         m != outgoing.end(); ++m)
    {
        if (m->state < loc.state) {
            list<trans_t>::iterator pre = m;
            --pre;
            outgoing.erase(m);
            m = pre;
        }
    }
    #ifdef DEBUG
    char buf[32];
    syslog(LOG_INFO, "[%hd]state: %d, %s", auth.id, loc.state,
           strop(loc.op));
    #endif
    for (list<trans_t>::iterator i = outgoing.begin();
         i != outgoing.end(); ++i)
    {
        #ifdef DEBUG
        syslog(LOG_INFO, "---[%hd]before: %s AND %s", auth.id,
               strop(loc.op), strop((*i).op, buf));
        #endif
        if (xformServer(loc.op, (*i).op) != 0) {
            #ifdef DEBUG
            syslog(LOG_INFO, "---[%hd]after : %s AND %s", auth.id,
                   strop(loc.op), strop((*i).op, buf));
            #endif
            break;
        }
        #ifdef DEBUG
        syslog(LOG_INFO, "---[%hd]after : %s AND %s", auth.id,
               strop(loc.op), strop((*i).op, buf));
        #endif
    }
}

int main(int argc, char *argv[]) {
    trans_t data;
    char auth_ok = 0;
    ssize_t num_read;
    prog_id = getpid();
    memset(&data, 0, sizeof(trans_t));
    syslog(LOG_INFO , "Jupiter server started [PID %ld]", (long)prog_id);
    atexit(logExitInfo);
    /* authenticate */
    if (readn(STDIN_FILENO, &auth, sizeof(auth)) != sizeof(auth)) {
        exit(EXIT_FAILURE);
    }
    else {
        auth.md5sum[MD5SUM_SIZE] = 0;
        if (strlen(auth.md5sum) != MD5SUM_SIZE) {
            /* length must match */
            exit(EXIT_FAILURE);
        }
        filename += auth.md5sum;
        login();
        atexit(logout);
        /* ack */
        if (writen(STDOUT_FILENO, &auth_ok, sizeof(auth_ok))
            != sizeof(auth_ok)) {
            exit(EXIT_FAILURE);
        }
    }
    /* newcomer can get history operations immediately */
    writeClient(SIGHUP);
    /* set sighup handler */
    sigset_t block_set, prev_mask;
    sigemptyset(&block_set);
    sigaddset(&block_set, SIGHUP);
    struct sigaction sa;
    sa.sa_handler = writeClient;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        exit(EXIT_FAILURE);
    }
    /* sigint and sigterm handler */
    sa.sa_handler = sigExitHandler;
    if (sigaction(SIGINT, &sa, NULL) == -1
        || sigaction(SIGTERM, &sa, NULL) == -1)
    {
        exit(EXIT_FAILURE);
    }
    /* read local op, readn promises reading sizeof(trans_t) bytes data */
    while ((num_read = readn(STDIN_FILENO, &data, sizeof(trans_t))) > 0) {
        getFileLock();
        /* block sighup while writing (become deaf temporarily) */
        sigprocmask(SIG_BLOCK, &block_set, &prev_mask);
        transform(data);
        if (write(writefd, &data.op, sizeof(op_t))
            != sizeof(op_t))
        {
            exit(EXIT_FAILURE);
        }
        ++local;
        /* wake up other servers */
        notifyOtherServers();
        /* restore pre sigmask */
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        releaseFileLock();
    }
    if (num_read == -1) {
        syslog(LOG_ERR, "Error from read(): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}