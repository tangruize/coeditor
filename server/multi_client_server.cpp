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

string filename = "/tmp/jupiter-server-";
auth_t auth;
pid_t prog_id;
int readfd = -1, writefd = -1;
off_t last_state = SHM_OFFSET;

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
    memset(&tmp, 0, sizeof(tmp));
    pwrite(readfd, &tmp, sizeof(tmp), sizeof(n) + sizeof(tmp) * i);
    pwrite(readfd, &n, sizeof(n), 0);
    releaseFileLock();
}

void logExitInfo() {
    syslog(LOG_INFO , "Jupiter server exiting [PID %ld]", (long)prog_id);
}

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
    while ((num_read = read(readfd, &t, sizeof(t))) == sizeof(t)) {
        if (t.op.id != auth.id) {
            write(STDOUT_FILENO, &t, sizeof(t));
        }
    }
}

void transform(op_t *op) {
    op_t tmp;
    while (pread(readfd, &tmp, sizeof(op_t), last_state) > 0) {
        if (xform(*op, tmp) != 0) {
            break;
        }
        pwrite(readfd, &tmp, sizeof(op_t), last_state);
        last_state += sizeof(trans_t);
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
    /* read local op */
    while ((num_read = readn(STDIN_FILENO, &data, sizeof(trans_t))) > 0) {
        getFileLock();
        /* block sighup while writing */
        sigprocmask(SIG_BLOCK, &block_set, &prev_mask);
        transform(&data.op);
        last_state = lseek(writefd, 0, SEEK_CUR);
        if (data.op.operation != NOOP) {
            if (write(writefd, &data, sizeof(trans_t)) != sizeof(trans_t)) {
                exit(EXIT_FAILURE);
            }
            notifyOtherServers();
        }
        /* wake up other servers */
        sigprocmask(SIG_SETMASK, &prev_mask, NULL);
        releaseFileLock();
    }
    if (num_read == -1) {
        syslog(LOG_ERR, "Error from read(): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
