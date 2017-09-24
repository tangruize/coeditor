/*************************************************************************
	> File Name: init.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-28 14:18
	> Description: 
 ************************************************************************/

#include "common.h"
#include "text.h"
#include "error.h"
#include "init.h" 
#include "op.h"
#include "text.h"
#include "front_end.h"
#include "inet_sockets.h"
#include "data_trans.h"

#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/timerfd.h>
#include <ncurses.h>

#define TRASH_FILE "/dev/null"

string debug_output_file_name;
string cli_input_fifo_name;
string ot_time_arg;
string ot_recv_time_arg;
string server_addr;
string out_file_prefix_no_pid = OUT_FILE_PREFIX;
string out_file_prefix;
textOp *edit_file;

bool no_debug = true;

double sleep_before_send = 0.0;
double sleep_before_recv = 0.0;

int send_timer_fd = -1, recv_timer_fd = -1;
int socket_fd = -1;

queue<trans_t> to_send;
queue<trans_t> to_recv;

user_id_t program_id;
queue<op_t> *pos_to_transform;

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
}

void initTimer() {
    char *endptr;
    sleep_before_send = strtod(ot_time_arg.c_str(), &endptr);
    if (*endptr != 0) {
        sleep_before_send = 0.0;
        cerr << "WARNING: Invalid send delay time: "
             << ot_time_arg << endl;
    }
    sleep_before_recv = strtod(ot_recv_time_arg.c_str(), &endptr);
    if (*endptr != 0) {
        sleep_before_recv = 0.0;
        cerr << "WARNING: Invalid receive delay time: "
             << ot_recv_time_arg << endl;
    }
    int i = 2;
    if (sleep_before_send > 0) {
        setTimer(sleep_before_send, &send_timer_fd, i++);
    }
    if (sleep_before_recv > 0) {
        setTimer(sleep_before_recv, &recv_timer_fd, i);
    }
}

string getCliInputFifoName() {
    return cli_input_fifo_name;
}

/* set out files name */
void setOutFilenames() {
    string filename = edit_file->getFilename();
    if (!filename.size()) {
        filename = "new-buf";
    }
    else {
        char path_buf[PATH_MAX];
        path_buf[PATH_MAX - 1] = 0;
        strncpy(path_buf, filename.c_str(), PATH_MAX - 1);
        filename = basename(path_buf);
    }
    out_file_prefix_no_pid += filename + ".";
    out_file_prefix = out_file_prefix_no_pid
                      + to_string((long)getpid()) + string(".");
}

/* save old cwd fd */
static int old_cwd_fd = -1;
static int edit_dir_fd = -1;
int saveDirFds() {
    old_cwd_fd = open(".", O_DIRECTORY | O_RDONLY);
    if (edit_file->getFilename().size() == 0) {
        edit_dir_fd = old_cwd_fd;
    }
    else {
        char path_buf[PATH_MAX] = {0};
        strncpy(path_buf, edit_file->getFilename().c_str(),
                PATH_MAX - 1);
        char *edit_dir_name = dirname(path_buf);
        if (strcmp(edit_dir_name, ".") == 0) {
            edit_dir_fd = old_cwd_fd;
        }
        else {
            edit_dir_fd = open(edit_dir_name, O_DIRECTORY | O_RDONLY);
        }
    }
    if (old_cwd_fd == -1 || edit_dir_fd == -1) {
        PROMPT_ERROR_EN("open: save dir fds: "
                        + to_string(old_cwd_fd) + ", "
                        + to_string(edit_dir_fd));
        return -1;
    }
    return 0;
}

/* go to the directory containing the editing file */
int gotoEditFileDir() {
    if (fchdir(edit_dir_fd) == -1) {
        PROMPT_ERROR_EN("fchdir: goto editing file dir");
        return -1;
    }
    return 0;
}

/* restore old cwd after some change dir operations */
int restoreOldCwd() {
    if (fchdir(old_cwd_fd) == -1) {
        PROMPT_ERROR_EN("fchdir: restore old cwd");
        return -1;
    }
    return 0;
}

void deleteOutFile(string &name, const string &ignore = "") {
    if (name.size() && name != ignore) {
        if (unlinkat(edit_dir_fd, name.c_str(), 0) == -1) {
            //PROMPT_ERROR_EN("unlink: " + name);
        }
        name.clear();
    }
}

void removeOutFileAtExit() {
    deleteOutFile(debug_output_file_name, TRASH_FILE);
    deleteOutFile(cli_input_fifo_name);
}

/* we might use stderr to print some debug message */
void redirectStderr() {
//    gotoEditFileDir();
    if (no_debug) {
        debug_output_file_name = TRASH_FILE;
    }
    else {
        debug_output_file_name = out_file_prefix + DEBUG_FILE_SUFFIX;
    }
    int fd = openat(edit_dir_fd, debug_output_file_name.c_str(),
             O_CREAT | O_TRUNC | O_WRONLY | O_APPEND, 0600);
    if (fd == -1) {
        debug_output_file_name  = TRASH_FILE;
        fd = open(TRASH_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    }
//    restoreOldCwd();
    if (dup2(fd, STDERR_FILENO) != STDERR_FILENO) {
        //cerr << "cannot dup stderr\n";
        close(fd);
        return;
    }
    close(fd);
}

int makeFifo(string &name) {
    if (mkfifoat(edit_dir_fd, name.c_str(), 0600)) {
        name.clear();
        return -1;
    }
    return 0;
}

int openFifos(string &name, int &readfd, int &writefd) {
    readfd = openat(edit_dir_fd, name.c_str(), O_RDONLY | O_NONBLOCK);
    writefd = openat(edit_dir_fd, name.c_str(), O_WRONLY | O_NONBLOCK);
    if (writefd == -1 || readfd == -1) {
        close(readfd);
        close(writefd);
        readfd = writefd = -1;
        deleteOutFile(name);
        return -1;
    }
    return 0;
}

void setBlock(int fd) {
    int flags = fcntl(fd, F_GETFL);
    flags &= ~O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

int cli_read_fd = -1, cli_write_fd = -1;
int creatCliInputFifo() {
    cli_input_fifo_name = out_file_prefix + CLI_INPUT_SUFFIX;
    if (makeFifo(cli_input_fifo_name) != 0
        || openFifos(cli_input_fifo_name, cli_read_fd, cli_write_fd) != 0)
    {
        cerr << "CLI not started" << endl;
        return -1;
    }
    close(cli_read_fd);
    return 0;
}

static queue<op_t> local_ops;
static pthread_t write_op_id;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t recv_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t send_mtx = PTHREAD_MUTEX_INITIALIZER;

void doWriteRemote(const trans_t &msg);
void synSend() {
    int s = pthread_mutex_lock(&send_mtx);
    if (s != 0) {
        PROMPT_ERROR_EN_S("pthread_mutex_lock", s);
    }
    while (to_send.size()) {
        const trans_t &t = to_send.front();
        doWriteRemote(t);
        to_send.pop();
    }
    s = pthread_mutex_unlock(&send_mtx);
    if (s != 0) {
        PROMPT_ERROR_EN_S("pthread_mutex_unlock", s);
    }
}

void *sendTimer_Thread(void *args) {
    pthread_detach(pthread_self());
    uint64_t exp;
    while (read(send_timer_fd, &exp, sizeof(exp)) > 0) {
        synSend();
    }
    return NULL;
}

void procServerWrapper(const trans_t &t) {
    edit_file->lock();
    while (local_ops.size());
    procServer(t);
    edit_file->unlock();
}

void synRecv() {
    int s = pthread_mutex_lock(&recv_mtx);
    if (s != 0) {
        PROMPT_ERROR_EN_S("pthread_mutex_lock", s);
    }
    while (to_recv.size()) {
        const trans_t &t = to_recv.front();
        procServerWrapper(t);
        to_recv.pop();
    }
    s = pthread_mutex_unlock(&recv_mtx);
    if (s != 0) {
        PROMPT_ERROR_EN_S("pthread_mutex_unlock", s);
    }
}

void *recvTimer_Thread(void *args) {
    pthread_detach(pthread_self());
    uint64_t exp;
    while (read(recv_timer_fd, &exp, sizeof(exp)) > 0) {
        synRecv();
    }
    return NULL;
}

void writeOpFifo(op_t &op) {
    switch (op.operation) {
        case SYN:
            synSend();
            synRecv();
            return;
        case RECV_SYN:
            synRecv();
            return;
        case SEND_SYN:
            synSend();
            return;
        default:
            break;
    }
    op.id = program_id;
    int s = pthread_mutex_lock(&mtx);
    if (s != 0) {
        PROMPT_ERROR_EN_S("pthread_mutex_lock", s);
    }
    /* Let consumer know another unit is available */
    local_ops.push(op);
    s = pthread_mutex_unlock(&mtx);
    if (s != 0) {
        PROMPT_ERROR_EN_S("pthread_mutex_unlock", s);
    }
    /* Wake sleeping consumer */
    s = pthread_cond_signal(&cond);
    if (s != 0) {
        PROMPT_ERROR_EN_S("pthread_cond_signal", s);
    }
}

void *writeOp_Thread(void *args) {
    pthread_detach(pthread_self());
    while (true) {
        int s = pthread_mutex_lock(&mtx);
        if (s != 0) {
            PROMPT_ERROR_EN_S("pthread_mutex_lock (consumer)", s);
        }
        /* Wait for something to consume */
        while (local_ops.empty()) {
            s = pthread_cond_wait(&cond, &mtx);
            if (s != 0) {
                PROMPT_ERROR_EN_S("pthread_cond_wait (consumer)", s);
            }
        }
        /* Consume all available units */
        while (local_ops.size()) {
            const op_t &op = local_ops.front();
            procLocal(op);
            local_ops.pop();
        }
        s = pthread_mutex_unlock(&mtx);
        if (s != 0) {
            PROMPT_ERROR_EN_S("pthread_mutex_unlock (consumer)", s);
        }
    }
    return NULL;
}

void *readServer_Thread(void *args) {
    pthread_detach(pthread_self());
    trans_t t;
    ssize_t num_read;
    if (recv_timer_fd == -1) {
        while ((num_read = readn(socket_fd, &t, sizeof(trans_t))) > 0) {
            procServerWrapper(t);
        }
    }
    else {
        pthread_t t1;
        int s = pthread_create(&t1, NULL, recvTimer_Thread, NULL);
        if (s != 0) {
            PROMPT_ERROR_EN_S("recv timer", s);
            close(recv_timer_fd);
            recv_timer_fd = -1;
        }
        while ((num_read = readn(socket_fd, &t, sizeof(trans_t))) > 0) {
            if (recv_timer_fd != -1)
                to_recv.push(t);
            else
                procServerWrapper(t);
        }
    }
    if (num_read == -1) {
        PROMPT_ERROR_EN("readn: read socket fd");
    }
    return NULL;
}

void tryOpen() {
    if (edit_file->getFilename() == "") {
        return;
    }
    int tmpfd = open(edit_file->getFilename().c_str(), O_RDONLY);
    if (tmpfd == -1) {
        if (errno != ENOENT) {
            PROMPT_ERROR_EN("Error opening " + edit_file->getFilename());
            exit(EXIT_FAILURE);
        }
    }
    else {
        struct stat sb;
        if (fstat(tmpfd, &sb) == -1) {
            PROMPT_ERROR_EN("Error stating " + edit_file->getFilename());
            exit(EXIT_FAILURE);
        }
        if (!S_ISREG(sb.st_mode)) {
            cerr << "Error: not a regular file: " 
                 << edit_file->getFilename() << endl;
            exit(EXIT_FAILURE);
        }
        close(tmpfd);
    }
}

const op_t* queuedOp(int enqueue, op_t *op) {
    if (pos_to_transform == NULL) {
        return NULL;
    }
    if (enqueue) {
        pos_to_transform->push(*op);
    }
    else {
        if (pos_to_transform->empty()) {
            return NULL;
        }
        static op_t tmp;
        tmp = pos_to_transform->front();
        pos_to_transform->pop();
        if (op) {
            *op = tmp;
        }
        return &tmp;
    }
    return NULL;
}

static void sigHandler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        exit(EXIT_SUCCESS);
    }
    else if (sig == SIGSEGV) {
        endwin();
        removeOutFileAtExit();
        signal(sig, SIG_DFL);
    }
}

void initSignal() {
    struct sigaction sa;
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = sigHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1
        || sigaction(SIGTERM, &sa, NULL) == -1
        || sigaction(SIGSEGV, &sa, NULL) == -1)
    {
        PROMPT_ERROR_EN("Error establishing signal handlers");
        exit(EXIT_FAILURE);
    }
}

void init() {
    initSignal();
    tryOpen();
    saveDirFds();
    setOutFilenames();
    initTimer();
    if (atexit(removeOutFileAtExit) != 0) {
        cerr << "Cannot set exit function\n";
    }
    pos_to_transform = new queue<op_t>;
    if (server_addr.size()) {
        int s = pthread_create(&write_op_id, NULL, writeOp_Thread, NULL);
        if (s != 0) {
            cerr << "OP output not started" << endl;
        }
    }
    else {
        write_op_pos = 1;
    }
    if (server_addr.size()) {
        socket_fd = inetConnect(server_addr.c_str(),
                                "13127", SOCK_STREAM);
        if (socket_fd == -1) {
            PROMPT_ERROR_EN("Error connecting " + server_addr);
            exit(EXIT_FAILURE);
        }
        auth_t auth = {
            0, "d41d8cd98f00b204e9800998ecf8427e"
        };
        if (edit_file->getFilename().size()
            && access(edit_file->getFilename().c_str(), R_OK) == 0)
        {
            cerr << "Calculating '" << edit_file->getFilename()
                 << "' md5sum..." << endl;
            string md5sum_file = "/tmp/" + out_file_prefix + "md5sum";
            string cmd = "md5sum " + edit_file->getFilename() + " >"
                         + md5sum_file;
            int sys_ret = system(cmd.c_str());
            if (sys_ret != 0) {
                EXIT_ERROR("Error calculating md5sum");
            }
            int md5_fd = open(md5sum_file.c_str(), O_RDONLY);
            unlink(md5sum_file.c_str());
            if (md5_fd == -1) {
                PROMPT_ERROR_EN("Error reading md5sum");
                exit(EXIT_FAILURE);
            }
            if (read(md5_fd, auth.md5sum, MD5SUM_SIZE) != MD5SUM_SIZE)
            {
                EXIT_ERROR("Error reading md5sum");
            }
            close(md5_fd);
        }
        cerr << "Connecting " << server_addr << "..." << endl;
        if (writen(socket_fd, &auth, sizeof(auth)) != sizeof(auth)
            || read(socket_fd, &auth, sizeof(auth)) <=0)
        {
            EXIT_ERROR("Error authenticating");
        }
        program_id = auth.id;
        pthread_t read_server_id;
        int s = pthread_create(&read_server_id, NULL, readServer_Thread,
                               NULL);
        if (s != 0) {
            cerr << "Server output not started" << endl;
        }
    }
    if (send_timer_fd != -1) {
        pthread_t send_timer_id;
        int s = pthread_create(&send_timer_id, NULL, sendTimer_Thread,
                               NULL);
        if (s != 0) {
            cerr << "Send timer not started" << endl;
            close(send_timer_fd);
            send_timer_fd = -1;
        }
    }
    edit_file->loadFile(edit_file->getFilename());
    //prompt_t msg = edit_file->loadFile(edit_file->getFilename());
    //PROMPT_ERROR(msg);
}
