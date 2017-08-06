/*************************************************************************
	> File Name: dummy_server.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-08-01 11:28
	> Description: 
 ************************************************************************/

#include "op.h"
#include <syslog.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    trans_t data;
    ssize_t num_read;
    memset(&data, 0, sizeof(trans_t));
    syslog(LOG_INFO , "Jupiter server started [PID %ld]", (long)getpid());
    while ((num_read = read(STDIN_FILENO, &data, sizeof(trans_t))) > 0) {
        //syslog(LOG_INFO , "read %ld bytes", num_read);
        if (num_read != sizeof(trans_t)) {
            if (data.op.operation > 0) {
                data.op.operation = -data.op.operation;
            }
            else if (data.op.operation == 0) {
                data.op.operation = CHAR_MIN;
            }
        }
        if (write(STDOUT_FILENO, &data, sizeof(trans_t)) != sizeof(trans_t)) {
            syslog(LOG_ERR, "write() failed: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        else {
            //syslog(LOG_INFO , "write %ld bytes", sizeof(trans_t));
        }
        memset(&data, 0, sizeof(trans_t));
    }
    if (num_read == -1) {
        syslog(LOG_ERR, "Error from read(): %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO , "Jupiter server exiting [PID %ld]", (long)getpid());
    exit(EXIT_SUCCESS);
}
