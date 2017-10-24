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
#include "data_trans.h"
#include "dll.h"
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
#include <semaphore.h>
#include <poll.h>
#include <sys/inotify.h>
#include <sys/file.h>
#include <sys/types.h>

#define INFO(MSG) syslog(LOG_INFO, "(%d) " #MSG, getpid())
#define ERROR(MSG) \
do { \
    syslog(LOG_ERR, "(%d) " #MSG ": %s", getpid(), strerror(errno)); \
    exit(EXIT_FAILURE); \
} while (0)

/* inotify buf * 10. */
#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))
/* we don't care the buf contents because there is only one event to monitor */
char buf[BUF_LEN];

string filename = "jupiter-server-"; /* filename <- "/tmp/" + filename + "CSCW"/"CSS" + MD5 */
string semname = "/"; /* semname <- semname + filename*/
auth_t auth; /* first step: algorithm <- auth.id; second step: auth.id <- user id */
int readfd = -1, writefd = -1, inotifyfd = -1;
sem_t *sem; /* online users number */

// prevent other servers writing at the same time
void getFileLock() {
    flock(readfd, LOCK_EX);
}

void releaseFileLock() {
    flock(readfd, LOCK_UN);
}

// log an exit information
void logExitInfo() {
    INFO(exit);
}

// remove sem if no online user
void logout() {
    int sval;
    sem_getvalue(sem, &sval);
    syslog(LOG_INFO, "(%d) exiting, user id: %d, current online: %d", getpid(), auth.id, sval - 1);
    if (sem_trywait(sem) == -1 || sval == 1) {
        sem_unlink(semname.c_str());
        INFO(sem_unlink);
    }
}

// open files, get user id and increase online users
int login() {
    readfd = open(filename.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
    if (readfd != -1) {
        sem_unlink(semname.c_str());
        INFO(try sem_unlink);
    }
    else if (errno == EEXIST)
        readfd = open(filename.c_str(), O_RDWR);
    if (readfd == -1)
        ERROR(open buf);
    writefd = open(filename.c_str(), O_WRONLY | O_APPEND);
    if (readfd == -1 || writefd == -1)
        ERROR(open);
    sem = sem_open(semname.c_str(), O_CREAT | O_EXCL, 0600, 0);
    if (sem == SEM_FAILED) {
        if (errno == EEXIST)
            sem = sem_open(semname.c_str(), 0);
        if (sem == SEM_FAILED)
            ERROR(sem_open);
    }
    else {
        if (ftruncate(readfd, 0) == -1)
            ERROR(ftruncate);
        if (ftruncate(readfd, sizeof(user_id_t)) == -1)
            ERROR(ftruncate);
    }
    if (sem_post(sem) == -1)
        ERROR(sem_post);
    getFileLock();
    user_id_t id;
    if (read(readfd, &id, sizeof(user_id_t)) != sizeof(user_id_t))
        ERROR(read num);
    ++id;
    if (pwrite(readfd, &id, sizeof(user_id_t), 0) != sizeof(user_id_t))
        ERROR(write num);
    releaseFileLock();
    atexit(logout);
    return id;
}

// load algo lib and set user id
void authenticate() {
    if (readn(STDIN_FILENO, &auth, sizeof(auth)) != sizeof(auth))
        ERROR(authenticate);
    if (auth.id >= NR_LIBS)
        ERROR(load lib);
    const char *err = setDllFuncs(0, auth.id);
    if (err != NULL) {
        syslog(LOG_ERR, "(%d) %s", getpid(), err);
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "(%d) algorithm: %s", getpid(), ALGO_VER);
    auth.md5sum[MD5SUM_SIZE] = 0;
    if (strlen(auth.md5sum) != MD5SUM_SIZE) {
        syslog(LOG_ERR, "(%d) MD5 format error", getpid());
        exit(EXIT_FAILURE);
    }
    string tmp = filename;
    tmp += string(ALGO_VER) + string("-") + string(auth.md5sum);
    semname += tmp;
    filename = string("/tmp/") + tmp;
    auth.id = login();
    if (write(STDOUT_FILENO, &auth, sizeof(auth)) != sizeof(auth))
        ERROR(ack);
    int sval;
    sem_getvalue(sem, &sval);
    syslog(LOG_INFO, "(%d) user id: %d, current online: %d", getpid(), auth.id, sval);
}

// init monitering file changes
void initInotify() {
    if ((inotifyfd = inotify_init()) == -1)
        ERROR(inotify_init);
    if (inotify_add_watch(inotifyfd, filename.c_str(), IN_MODIFY) == -1)
        ERROR(inotify_add_watch);
}

// if there are any changes, send them to its client
void localProcess() {
    op_t op;
    ssize_t num_read;
    while ((num_read = read(readfd, &op, sizeof(op_t))) == sizeof(op_t))
        if (op.id != auth.id)
            (*fromLocal)(op);
    if (num_read != 0) {
        if (num_read == -1)
            syslog(LOG_ERR, "(%d) read: %s", getpid(), strerror(errno));
        else
            lseek(readfd, -num_read, SEEK_CUR);
    }
}

// receive msgs from its client
void remoteProcess() {
    trans_t data;
    ssize_t num_read;
    if ((num_read = readn(STDIN_FILENO, &data, sizeof(trans_t))) != sizeof(trans_t)) {
        if (num_read == 0)
            exit(EXIT_SUCCESS);
        ERROR(read client);
    }
    getFileLock();
    localProcess();
    (*fromNet)(data);
    releaseFileLock();        
}

// moniter local and remote
void startMonitor() {
    struct pollfd fds[2] = {
        {inotifyfd, POLLIN, 0},
        {STDIN_FILENO, POLLIN, 0},
    };
    while (poll(fds, 2, -1) != -1) {
        if (fds[0].revents & POLLIN) {
            if (read(inotifyfd, buf, BUF_LEN) <= 0)
                ERROR(read inotifyfd);
            localProcess();
        }
        else if (fds[1].revents & POLLIN)
            remoteProcess();
        else if (fds[1].revents & POLLHUP)
            exit(EXIT_SUCCESS);
    }
    ERROR(poll);
}

// call-back function used by algo (local processing)
void writeLocal(const op_t &op) {
    write(writefd, &op, sizeof(op));
}

// call-back function used by algo (remote processing)
void writeRemote(const trans_t &msg) {
    write(STDOUT_FILENO, &msg, sizeof(msg));
}

// exit if signaled. before exiting, print exiting and logout
void sigExitHandler(int sig) {
    INFO(signaled);
    exit(EXIT_SUCCESS);
}

// add signals
void initSignal() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sigExitHandler;
    if (sigaction(SIGINT, &sa, NULL) == -1
        || sigaction(SIGTERM, &sa, NULL) == -1
        || sigaction(SIGPIPE, &sa, NULL) == -1)
    {
        ERROR(sigaction);
    }
}

int main(int argc, char *argv[]) {
    INFO(start);
    atexit(logExitInfo);
    initSignal();
    authenticate();
    initInotify();
    localProcess(); /* read out all history msgs */
    startMonitor();
    exit(EXIT_SUCCESS);
}
