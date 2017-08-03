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

#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

string out_file_prefix_no_pid = OUT_FILE_PREFIX;
string out_file_prefix;

textOp *edit_file;

#define TRASH_FILE "/dev/null"
string debug_output_file_name;

string cli_input_fifo_name;

string op_output_fifo_name;

string op_input_fifo_name;

string op_input_feedback_fifo_name;

string to_server_fifo_name;

string to_server_feedback_fifo_name;

string from_server_fifo_name;

string lock_file;

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
    //debug_output_file_name = out_file_prefix_no_pid + DEBUG_FILE_SUFFIX;
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
            edit_dir_fd = open(dirname(path_buf), O_DIRECTORY | O_RDONLY);
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

void deleteOutFile(const string &name, const string &ignore = "") {
    if (name.size() && name != ignore) {
        if (unlinkat(edit_dir_fd, name.c_str(), 0) == -1) {
            PROMPT_ERROR_EN("unlink: " + name);
        }
    }
}

void removeOutFileAtExit() {
    deleteOutFile(lock_file);
    deleteOutFile(debug_output_file_name, TRASH_FILE);
    deleteOutFile(cli_input_fifo_name);
    deleteOutFile(op_output_fifo_name);
    deleteOutFile(op_input_fifo_name);
    deleteOutFile(op_input_feedback_fifo_name);
    deleteOutFile(to_server_fifo_name);
    deleteOutFile(to_server_feedback_fifo_name);
    deleteOutFile(from_server_fifo_name);
}

/* we might use stderr to print some debug message */
void redirectStderr() {
//    gotoEditFileDir();
    debug_output_file_name = out_file_prefix + DEBUG_FILE_SUFFIX;
    int fd = openat(edit_dir_fd, debug_output_file_name.c_str(),
             O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd == -1) {
        debug_output_file_name  = TRASH_FILE;
        fd = open(TRASH_FILE, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    }
//    restoreOldCwd();
    if (debug_output_file_name == TRASH_FILE) {
        cerr << "cannot create debug file\n";
    }
    if (dup2(fd, STDERR_FILENO) != STDERR_FILENO) {
        cerr << "cannot dup stderr\n";
        close(fd);
        return;
    }
    close(fd);
}

int makeFifo(string &name) {
    if (mkfifoat(edit_dir_fd, name.c_str(), 0644)) {
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
        name.clear();
        return -1;
    }
    return 0;

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

int op_read_fd = -1, op_write_fd = -1;
int creatOpOutputFifo() {
    op_output_fifo_name = out_file_prefix + OP_OUTPUT_SUFFIX;
    if (makeFifo(op_output_fifo_name) != 0 
        || openFifos(op_output_fifo_name, op_read_fd, op_write_fd) != 0)
    {
        cerr << "OP output not started" << endl;
        return -1;
    }
    return 0;
}

int op_in_feedback_read_fd = -1, op_in_feedback_write_fd = -1;
int creatOpInputFeedbackFifo() {
    op_input_feedback_fifo_name = out_file_prefix
                                  + OP_INPUT_FEEDBACK_SUFFIX;
    if (makeFifo(op_input_feedback_fifo_name) != 0
        || openFifos(op_input_feedback_fifo_name, op_in_feedback_read_fd, 
                     op_in_feedback_write_fd) != 0)
    {
        cerr << "OP input feedback not started" << endl;
        return -1;
    }
    return 0;
}

int op_in_read_fd = -1, op_in_write_fd = -1;
int creatOpInputFifo() {
    op_input_fifo_name = out_file_prefix + OP_INPUT_SUFFIX;
    if (makeFifo(op_input_fifo_name) != 0
        || openFifos(op_input_fifo_name, op_in_read_fd,
                     op_in_write_fd) != 0)
    {
        cerr << "OP input not started" << endl;
        return -1;
    }
    int flags = fcntl(op_in_read_fd, F_GETFL);
    flags &= ~O_NONBLOCK;
    fcntl(op_in_read_fd, F_SETFL, flags);
    return 0;
}

int server_in_read_fd = -1, server_in_write_fd = -1;
int creatServerInputFifo() {
    to_server_fifo_name = out_file_prefix + TO_SERVER_SUFFIX;
    if (makeFifo(to_server_fifo_name) != 0
        || openFifos(to_server_fifo_name, server_in_read_fd,
                     server_in_write_fd) != 0)
    {
        cerr << "Server input not started" << endl;
        return -1;
    }
    int flags = fcntl(server_in_read_fd, F_GETFL);
    flags &= ~O_NONBLOCK;
    fcntl(server_in_read_fd, F_SETFL, flags);
    return 0;
}

int server_out_read_fd = -1, server_out_write_fd = -1;
int creatServerOuputFifo() {
    from_server_fifo_name = out_file_prefix + FROM_SERVER_SUFFIX;
    if (makeFifo(from_server_fifo_name) != 0
        || openFifos(from_server_fifo_name, server_out_read_fd,
                    server_out_write_fd) != 0)
    {
        cerr << "Server output not started" << endl;
        return -1;
    }
    return 0;
}

int server_in_feedback_read_fd = -1, server_in_feedback_write_fd = -1;
int creatServerInputFeedbackFifo() {
    to_server_feedback_fifo_name = out_file_prefix
                                   + TO_SERVER_FEEDBACK_SUFFIX;
    if (makeFifo(to_server_feedback_fifo_name) != 0
        || openFifos(to_server_feedback_fifo_name, server_in_feedback_read_fd,
                     server_in_feedback_write_fd) != 0)
    {
        cerr << "Server input feedback not started" << endl;
        return -1;
    }
    return 0;
}

void reportWriteOpError(const op_t &op, int en) {
    cerr << "Output op failed: ";
    if (en != 0) { 
        cerr << strerror(en);
    }
    cerr << "\n---type: " << (char)op.operation << endl;
    if (isupper(op.operation)) {
        cerr << "---offset: " << op.char_offset << endl;
    }
    else {
        cerr << "---pos: (" << op.pos.lineno << ", "
             << op.pos.offset << ")\n";
    }
    if (op.operation == 'i' || op.operation == 'I') {
        cerr << "---data: '";
        if (isprint(op.data)) {
            cerr << op.data << "'\n";
        }
        else {
            cerr << "0x" << hex << uppercase << (unsigned)op.data
                 << "'\n";
            cerr.unsetf(ios_base::hex);
        }
    }
}

void writeOpFifo(const op_t &op) {
    if (op_write_fd == -1 || op_read_fd == -1) {
        return;
    }
    static char pipe_buf[PIPE_BUF];
    int try_times = 3;
    while (try_times--
           && write(op_write_fd, &op, sizeof(op_t)) != sizeof(op_t))
    {
        if (errno == EAGAIN) {
            /* eat all unread buf */
            read(op_read_fd, pipe_buf, PIPE_BUF);
        }
        else {
            reportWriteOpError(op, errno);
            break;
        }
    }
    if (try_times < 0) {
        reportWriteOpError(op, 0);
    }
}

int readOpInFifo(op_t &op) {
    if (op_in_read_fd == -1) {
        return 0;
    }
    return (int)read(op_in_read_fd, &op, sizeof(op_t));
}

void *readOp_Thread(void *args) {
    pthread_detach(pthread_self());
    op_t op;
    int ret;
    string op_result;
    creatOpInputFeedbackFifo();
    while ((ret = readOpInFifo(op)) != 0) {
        if (ret == -1) {
            PROMPT_ERROR_EN("read op input fifo");
            break;
        }
        char del_ch = 0;
        switch (op.operation) {
            case DELETE:
                op_result = edit_file->deleteChar(op.pos, &del_ch);
                op.data = del_ch;
                break;
            case CH_DELETE:
                op_result = edit_file->deleteCharAt(op.char_offset, 
                                                   &del_ch);
                op.data = del_ch;
                break;
            case INSERT:
                op_result = edit_file->insertChar(op.pos, op.data);
                break;
            case CH_INSERT:
                op_result = edit_file->insertCharAt(op.char_offset,
                                                   op.data);
                break;
            default:
                op_result = "FAILED";
        }
        if (op_result != NOERR) {
            if (op.operation > 0) {
                op.operation = -op.operation;
            }
            else if (op.operation == 0) {
                op.operation = INT_MIN;
            }
        }
        else {
            buf_changed = 1;
        }
        if (op_in_feedback_write_fd != -1) {
            int try_times = 3;
            static char pipe_buf[PIPE_BUF];
            while (try_times-- &&
                   write(op_in_feedback_write_fd, &op, sizeof(op_t)) == -1
                   && errno == EAGAIN)
            {
                read(op_in_feedback_read_fd, pipe_buf, PIPE_BUF);
            }
        }
    }
    close(op_in_read_fd);
    return NULL;
}

int socket_fd = -1;

void *read_server_thread(void *args) {
    pthread_detach(pthread_self());
    trans_t t;
    ssize_t num_read;
    static char pipe_buf[PIPE_BUF];
    while ((num_read = readn(socket_fd, &t, sizeof(trans_t))) > 0)
    {
        int try_times = 3;
        while (try_times-- && write(server_out_write_fd, &t,
               num_read) == -1 && errno == EAGAIN)
        {
            read(server_out_read_fd, pipe_buf, PIPE_BUF);
        }
    }
    if (num_read == -1) {
        PROMPT_ERROR_EN("readn: read socket fd");
    }
    return NULL;
}

void *write_server_thread(void *args) {
    pthread_detach(pthread_self());
    trans_t t;
    ssize_t num_read;
    static char pipe_buf[PIPE_BUF];
    memset(&t, 0, sizeof(t));
    while ((num_read = read(server_in_read_fd, &t, sizeof(trans_t))) > 0) {
        if (writen(socket_fd, &t, sizeof(trans_t)) != sizeof(trans_t))
        {
            /* failed */
            if (t.op.operation > 0) {
                t.op.operation = -t.op.operation;
            }
            else {
                t.op.operation = INT_MIN;
            }
        }
        int try_times = 3;
        while (try_times-- &&
               write(server_in_feedback_write_fd, &t, sizeof(t)) == -1
               && errno == EAGAIN)
        {
            read(server_in_feedback_read_fd, pipe_buf, PIPE_BUF);
        }
        memset(&t, 0, sizeof(t));
    }
    return NULL;
}

int checkFileLock() {
    if (edit_file->getFilename() == "") {
        /* new file */
        return 0;
    }
    lock_file = out_file_prefix_no_pid + LOCK_FILE_SUFFIX;
    int lock_file_fd = openat(edit_dir_fd, lock_file.c_str(), O_CREAT | O_RDWR, 0644);
    if (lock_file_fd == -1) {
        PROMPT_ERROR_EN("open: " + lock_file);
        exit(EXIT_FAILURE);
    }
    if (flock(lock_file_fd, LOCK_EX | LOCK_NB) == -1) {
        if (errno == EWOULDBLOCK) {
            cerr << "File is already locked";
            char buf[16] = {0};
            if (read(lock_file_fd, buf, 15) > 0) {
                cerr << " by PID " << buf;
            }
            cerr << ".\n";
        }
        else {
            PROMPT_ERROR_EN("flock: get lock file");
        }
        exit(EXIT_FAILURE);
    }
    string pid = to_string((long)getpid());
    ftruncate(lock_file_fd, 0);
    write(lock_file_fd, pid.c_str(), pid.size());
    return 0;
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
    struct stat sb;
    if (fstat(tmpfd, &sb) == -1) {
        PROMPT_ERROR_EN("Error stating " + edit_file->getFilename());
        exit(EXIT_FAILURE);
    }
    if (!S_ISREG(sb.st_mode)) {
        cerr << "Error: not a regular file: " + edit_file->getFilename() << endl;
        exit(EXIT_FAILURE);
    }
    close(tmpfd);
}

void init() {
    tryOpen();
    saveDirFds();
    setOutFilenames();
    if (atexit(removeOutFileAtExit) != 0) {
        cerr << "Cannot set exit function\n";
    }
    checkFileLock();
    edit_file->loadFile(edit_file->getFilename());
    //prompt_t msg = edit_file->loadFile(edit_file->getFilename());
    //PROMPT_ERROR(msg);
    creatOpOutputFifo();
    if (creatOpInputFifo() == 0) {
        pthread_t read_op_id;
        int s = pthread_create(&read_op_id, NULL, readOp_Thread, NULL);
        if (s != 0) {
            close(op_in_read_fd);
            close(op_in_write_fd);
            deleteOutFile(op_input_fifo_name);
            op_input_fifo_name.clear();
            cerr << "OP input not started" << endl;
        }
    }
    if (server_addr.size()) {
        socket_fd = inetConnect(server_addr.c_str(), "jupiter", SOCK_STREAM);
        if (socket_fd == -1) {
            PROMPT_ERROR_EN("Error connecting " + server_addr);
            sleep(2);
        }
        else if (creatServerOuputFifo() == -1
                 || creatServerInputFifo() == -1
                 || creatServerInputFeedbackFifo() == -1)
        {
            /* cannot create fifos */
            close(socket_fd);
        }
        else {
            pthread_t read_server_id, write_server_id;
            int s = pthread_create(&read_server_id, NULL, read_server_thread,
                                   NULL);
            if (s != 0) {
                close(server_out_read_fd);
                close(server_out_write_fd);
                deleteOutFile(from_server_fifo_name);
                from_server_fifo_name.clear();
                cerr << "Server output not started" << endl;
            }
            s = pthread_create(&write_server_id, NULL, write_server_thread,
                               NULL);
            if (s != 0) {
                close(server_in_read_fd);
                close(server_in_write_fd);
                close(server_in_feedback_read_fd);
                close(server_in_feedback_write_fd);
                deleteOutFile(to_server_fifo_name);
                deleteOutFile(to_server_feedback_fifo_name);
                to_server_fifo_name.clear();
                to_server_feedback_fifo_name.clear();
                cerr << "Server input not started" << endl;
            }
        }
    }
}
