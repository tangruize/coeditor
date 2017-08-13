/*************************************************************************
	> File Name: front_end_curses.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-26 15:15
	> Description: 
 ************************************************************************/

#include "common.h"
#include "front_end.h"
#include "op.h"
#include "init.h"

#include <stdlib.h>
#include <ncurses.h>
#include <limits.h>
#include <string.h>
#include <signal.h> /* raise() */
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h> /* basename(), dirname() */
#include <sys/types.h>
#include <sys/time.h> /* gettimeofday() */
#include <sys/stat.h> /* stat() */
#include <pwd.h>

#define VERSION "1.1"

extern pthread_t main_thread_id;
extern volatile int buf_changed;
extern int write_op_pos;

/* least window size */
#define MIN_COL 10
#define MIN_ROW 10

/* title and status lines */
#define TITLE_LINES 2
#define STATUS_LINES 3
#define CONTENT_LINES (LINES - TITLE_LINES - STATUS_LINES)
#define HALF_HEIGHT (CONTENT_LINES / 2)

/* show previous n characters if folds */
#define FOLD_PRE_NUM 6

/* current position and line length */
pos_t cur_pos = {1, 1};
int cur_line_size = 0;

/* screen line position */
int screen_start_line = 1;

/* the editing file */
textOp *editing_file;

/* cursor */
int screen_line = TITLE_LINES, screen_line_offset = 0;

/* status bar -- the last two lines */
typedef const vector<const char *> status_t;

bool readonly_flag = false;
bool new_file_flag = false;

/* format 1: status bar (last 2 lines),
 * each token's first 2 chars will draw in reverse color
 */
static status_t main_msgs = {
    "^G Get Help", "^X Exit",
    "^W Write Out", "^C Cur Pos",
    "^T Goto Line", "^O Goto Offset",
    "^Y Prev Page", "^V Next Page",
    "^P Prev Line", "^N Next Line"
};

/* format 2: Choice menu (last 3 lines), 
 * the first token is prompt, second is choices, remainders are format 1
 * Choice: UPPERCASE represents ctrl must pressed, lowercase without
 */
static status_t exit_msgs = {
    "Save modified buffer (ANSWERING \"No\" WILL DESTROY CHANGES) ? ",
    "ynC",
    " Y Yes", " N No", 
    "", "^C Cancel"
};

const char *write_only = "File Name to Write";
const char *write_append = "File Name to Append to";
const char *write_prepend = "File Name to Prepend to";
char write_str[40] = "File Name to Write:";
char write_cand[PATH_MAX] = {0};

/* format 3: string menu (last 3 lines), 
 * the first token is prompt, second is candidate, third is choices,
 * remainders are format 1
 */
static status_t write_out_msgs = {
    write_str, write_cand,
    "ABCP",
    "^A Append", "^C Cancel",
    "^P Prepend", "",
    "^B Backup File"
};

static status_t over_write_msgs = {
    "File exists, OVERWRITE ? ",
    "ynC",
    " Y Yes", " N No", 
    "", "^C Cancel"
};

static status_t another_name_msgs = {
    "Save file under DIFFERENT NAME ? ",
    "ynC",
    " Y Yes", " N No", 
    "", "^C Cancel"
};

static status_t goto_line_msgs = {
    "Enter line number, column number:",
    "", "YCV",
    "^Y First Line", "^C Cancel",
    "^V Last Line"
};

static status_t goto_char_msgs = {
    "Enter offset:",
    "", "YCV",
    "^Y First Char", "^C Cancel",
    "^V Last Char"
};

static status_t help_msgs = {
    "^Y Prev Page", "^V Next Page",
    "^P Prev Line", "^N Next Line",
    "^X Exit"
};

static status_t sure_append = {
    "Append to orignal file ? ",
    "ynC",
    " Y Yes", " N No", 
    "", "^C Cancel"
};

static status_t sure_prepend = {
    "Prepend to orignal file ? ",
    "ynC",
    " Y Yes", " N No", 
    "", "^C Cancel"
};

/* immediate message, the last 3 line */
static status_t invalid_line = { "Invalid line or column number" };
static status_t invalid_offset = { "Invalid offset" };
static status_t cancelled = { "Cancelled" };
static status_t unknown_command = { "Unknown command" };
static status_t unimplemented = { "Unimplemented" };
static status_t view_mode_msg = { "View mode" };
static status_t edit_mode_msg = { "Edit mode" };

struct file_id {
    dev_t fi_dev;
    ino_t fi_ino;
    int fi_scl;
    pos_t fi_cpos;
    uint64_t fi_off;
} __attribute__((packed));

/* read offset since we last edit */
#define MAX_RECORDS 28
#define CACHE_FILE_SIZE  (sizeof(file_id) * MAX_RECORDS)
bool isNewFile() {
    if (!(editing_file->getFilename().size()
       && access(editing_file->getFilename().c_str(), F_OK) == 0))
    {
        return true;
    }
    return false;
}

const char *setCachePath() {
    static string cache_file;
    if (cache_file.size()) {
        return cache_file.c_str();
    }
    struct passwd *ps = getpwuid(getuid());
    if (ps == NULL) {
        return NULL;
    }
    cache_file = ps->pw_dir;
    cache_file += "/.coeditor.cache";
    if (truncate(cache_file.c_str(), CACHE_FILE_SIZE) == -1) {
        cache_file.clear();
        return NULL;
    }
    return cache_file.c_str();
}

struct stat *statSrcFile() {
    static struct stat *cache_file_sb = NULL;
    if (cache_file_sb) {
        return cache_file_sb;
    }
    if (isNewFile()) {
        return NULL;
    }
    static struct stat sb;
    if (stat(editing_file->getFilename().c_str(), &sb) == -1) {
        return NULL;
    }
    cache_file_sb = &sb;
    return cache_file_sb;
}

struct file_id *loadCacheFile(int *n, int *cfd = NULL) {
    struct stat *sb = statSrcFile();
    if (sb == NULL) {
        return NULL;
    }
    const char *cache_path = setCachePath();
    if (cache_path == NULL) {
        return NULL;
    }
    int fd = open(cache_path, O_RDWR | O_CREAT, 0600);
    if (fd == -1) {
        return NULL;
    }
    static char buf[CACHE_FILE_SIZE];
    if (read(fd, buf, CACHE_FILE_SIZE) != CACHE_FILE_SIZE) {
        return NULL;
    }
    if (n) {
        *n = MAX_RECORDS;
    }
    struct file_id *p = (struct file_id *)buf;
    for (int i = 0; i < MAX_RECORDS; ++i)
    {
        if (p[i].fi_dev == sb->st_dev && p[i].fi_ino == sb->st_ino) {
            if (n) {
                *n = i;
            }
            break;
        }
    }
    if (cfd) {
        *cfd = fd;
    }
    else {
        close(fd);
    }
    return (struct file_id *)buf;
}

void readCacheFile() {
    int n;
    struct file_id *p = loadCacheFile(&n);
    if (!p) {
        return;
    }
    if (n != MAX_RECORDS) {
        if (editing_file->getTotalChars() >= p[n].fi_off) {
            screen_start_line = p[n].fi_scl;
            cur_pos = p[n].fi_cpos;
        }
    }
}

/* WARNING: race condition (record lock can solve this problem) */
void writeCacheFile() {
    int n, fd;
    struct file_id *p = loadCacheFile(&n, &fd);
    if (!p) {
        return;
    }
    struct stat *sb = statSrcFile();
    if (sb == NULL) {
        return;
    }
    struct file_id fi;
    fi.fi_cpos = cur_pos;
    fi.fi_scl = screen_start_line;
    fi.fi_off = editing_file->translatePos(cur_pos);
    fi.fi_dev = sb->st_dev;
    fi.fi_ino = sb->st_ino;
    if (n == MAX_RECORDS) {
        --n;
    }
    for (; n >= 1; --n) {
        p[n] = p[n - 1];
    }
    p[0] = fi;
    pwrite(fd, p, CACHE_FILE_SIZE, 0);
    close(fd);
}

/* init curses */
void initCurses() {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    TABSIZE = 1;
}

/* finalization on unloading */
void __attribute__ ((destructor)) endCurses() {
    endwin();
}

/* calculate current line content and set cursor position */
string calCurLine() {
    const string &line = editing_file->locateLine(cur_pos.lineno)->line;
    cur_line_size = line.size();
    if (cur_pos.offset > cur_line_size + 1) {
        cur_pos.offset = cur_line_size + 1;
    }
    string cur_line;
    int string_start_offset, count;
    int long_line_folds = cur_pos.offset - (COLS - 1) - 1;
    if (long_line_folds < 0) {
        /* COLS can hold the total line */
        screen_line_offset = cur_pos.offset - 1;
        count = cur_line_size < COLS ? cur_line_size : COLS - 1;
        cur_line = line.substr(0, count);
        if (cur_line_size >= COLS) {
            cur_line += "$";
        }
    }
    else {
        cur_line = "$";
        int fold_edit_len = COLS - FOLD_PRE_NUM - 2;
        screen_line_offset = long_line_folds % fold_edit_len
                             + FOLD_PRE_NUM + 1;
        long_line_folds /= fold_edit_len;
        string_start_offset = long_line_folds * fold_edit_len
                              + COLS - 1 - FOLD_PRE_NUM;
        count = cur_line_size - string_start_offset;
        bool is_cont = false;
        if (count > COLS - 2) {
            count = COLS - 2;
            is_cont = true;
        }
        cur_line += line.substr(string_start_offset, count);
        if (is_cont) {
            cur_line += "$";
        }
    }
    return cur_line;
}

/* update cursor */
void updateCursor() {
    move(screen_line, screen_line_offset);
}

/* replace TAB and carriage return with space */
//void replaceTab(string &line) {
//    for (auto &i: line) {
//        if (i == '\t') {
//            i = ' ';
//        }
//        else if (i == '\r') {
//            i = '$';
//        }
//    }
//}

/* filter input and put to screen */
void mvyDrawString(int y, const string &line) {
    move(y, 0);
    clrtoeol();
    const char *s = line.c_str();
    int size = line.size();
    for (int i = 0; i < size; ++i) {
        if (s[i] == '\t') {
            addch(' ' | A_REVERSE);
        }
        else if (s[i] == '\r') {
            addch('$' | A_REVERSE);
        }
        else {
            addch(s[i]);
        }
    }
}

/* redraw current line and move cursor */
void redrawCurLine() {
    string line = calCurLine();
    mvyDrawString(screen_line, line);
    updateCursor();
    refresh();
}

/* update content */
void redrawContent() {
    int total_lines = editing_file->getTotalLines();
    int i = TITLE_LINES, j = screen_start_line;
    bool cur_line_out_of_screen = true;
    for (; i < LINES - STATUS_LINES && j <= total_lines; ++i, ++j) {
        const string &line = editing_file->locateLine(j)->line;
        string print_content;
        if (cur_pos.lineno != j) {
            unsigned count = line.size() < COLS ? 
                             line.size() : COLS - 1;
            print_content = line.substr(0, count);
            if (line.size() >= COLS) {
                print_content += "$";
            }
        }
        else {
            cur_line_out_of_screen = false;
            screen_line = i;
            print_content = calCurLine();
        }
        mvyDrawString(i, print_content);
    }
    if (cur_line_out_of_screen) {
        /* current line should not out of screen, find a nearest pos */
        if (cur_pos.lineno < screen_start_line) {
            cur_pos.lineno = screen_start_line;
        }
        else {
            cur_pos.lineno = screen_start_line + CONTENT_LINES - 1;
        }
        redrawCurLine();
    }
    /* reaching end of file leaves uncleared area */
    for (; i < LINES - STATUS_LINES; ++i) {
        move(i, 0);
        clrtoeol();
    }
    updateCursor();
    refresh();
}

/* redraw title */
void redrawTitle() {
    string filename = editing_file->getFilename();
    const string prog_name = "  Coeditor " VERSION;
    const char modified_str[] = "Modified";
    const char readonly_str[] = "Read-only";
    bool is_modified = editing_file->isModified();
    if (!filename.size()) {
        /* new file */
        filename = "New Buffer";
    }
    move(0, 0);
    clrtoeol();
    int size = prog_name.size() > COLS - 2 ? COLS - 2: prog_name.size();
    printw(prog_name.substr(0, size).c_str());
    int name_start_col = COLS / 2 - filename.size() / 2;
    if (name_start_col < size + 4) {
        name_start_col = size + 4;
    }
    if (name_start_col < COLS) {
        int restrict_size = COLS - size - 6;
        if (filename.size() > restrict_size) {
            filename = filename.substr(filename.size() - restrict_size,
                                       restrict_size);
        }
        mvprintw(0, name_start_col, filename.c_str());
        size += 4 + filename.size();
    }
    if (editing_file->getFilename() == filename.c_str()
        && access(filename.c_str(), W_OK) == -1
        && errno == EACCES)
    {
        readonly_flag = true;
    }
    else {
        readonly_flag = false;
    }
    if (is_modified && size + 4 + sizeof(modified_str) < COLS) {
        mvprintw(0, COLS - sizeof(modified_str) - 1, modified_str);
    }
    else if (size + 4 + sizeof(readonly_str) < COLS && readonly_flag) {
        mvprintw(0, COLS - sizeof(readonly_str) - 1, readonly_str);
    }
    move(0, 0);
    chgat(-1, A_REVERSE, 0, NULL);
    updateCursor();
}

/* draw status bar (last two lines) */
status_t *drawStatusBar(status_t &msgs, int offset = 0) {
    static status_t *old_msgs = &main_msgs;
    int size = msgs.size() - offset;
    size = size % 2 == 0 ? size : size + 1;
    int width = COLS / (size / 2) - 1;
    int start = 0;
    move(LINES - 2, 0);
    clrtoeol();
    move(LINES - 1, 0);
    clrtoeol();
    for (int i = offset; i < msgs.size() - 1;
         i+=2, start += width + 1)
    {
        string s1 = msgs[i], s2 = msgs[i + 1];
        if (s1.size()) {
            s1.resize(width);
            mvprintw(LINES - 2, start, s1.c_str());
            mvchgat(LINES - 2, start, 2, A_REVERSE, 0, NULL);
        }
        if (s2.size()) {
            s2.resize(width);
            mvprintw(LINES - 1, start, s2.c_str());
            mvchgat(LINES - 1, start, 2, A_REVERSE, 0, NULL);
        }
    }
    if ((msgs.size() - offset) % 2 == 1) {
        string s = msgs[msgs.size() - 1];
        s.resize(width);
        mvprintw(LINES - 2, start, s.c_str());
        mvchgat(LINES - 2, start, 2, A_REVERSE, 0, NULL);
    }
    updateCursor();
    status_t *ret_val = old_msgs;
    old_msgs = &msgs;
    return ret_val;
}

/* determine if a message is showing */
static bool hasMsgImm = false;
/* draw immediate message */
void drawStatusMsgImm(const char *msg) {
    move(LINES - 3, 0);
    clrtoeol();
    if (msg == NULL) {
        updateCursor();
        hasMsgImm = false;
        return;
    }
    string s = msg;
    int start = (COLS - 4 - s.size()) / 2;
    if (s.size() >= COLS - 4) {
        start = 0;
        s.resize(COLS - 4);
    }
    mvprintw(LINES - 3, start, "[ ");
    printw("%s ]", s.c_str());
    mvchgat(LINES - 3, start, s.size() + 4, A_REVERSE, 0, NULL);
    updateCursor();
    hasMsgImm = true;
}

/* string menu and choice menu prompt */
void drawStatusMsgPrompt(const char *msg) {
    move(LINES - 3, 0);
    clrtoeol();
    string s(msg);
    if (s.size() > COLS - 4) {
        s.resize(COLS - 4);
    }
    printw(s.c_str());
    mvchgat(LINES - 3, 0, -1, A_REVERSE, 0, NULL);
    move(LINES - 3, s.size());
}

/* choice menu */
int drawStatusMsgOneChar(status_t &msgs) {
    status_t *old_status = drawStatusBar(msgs, 2);
    drawStatusMsgPrompt(msgs[0]);
    while (true) {
        int ch = getch();
        if (isupper(ch)) {
            ch = tolower(ch);
        }
        if (ch <= 26) {
            ch += 64;
        }
        else if (ch == KEY_RESIZE) {
            ch = 0;
        }
        if (strchr(msgs[1], ch) != NULL) {
            drawStatusBar(*old_status);
            move(LINES - 3, 0);
            clrtoeol();
            updateCursor();
            return ch;
        }
    }
    return 0;
}

/* get string menu input */
int drawStatusMsgInput(int ch, int x, string *s = NULL) {
    int cur_x;
    char buf[2] = {0};
    if (ch == KEY_BACKSPACE) {
        if (s->size()) {
            s->resize(s->size() - 1);
            cur_x = getcurx(stdscr);
            if (cur_x <= x + 1) {
                move(LINES - 3, x - 1);
                clrtoeol();
                if (s->size() < COLS - x) {
                    addch(' ' | A_REVERSE);
                }
                else {
                    addch('$' | A_REVERSE);
                }
                int i = ((s->size() - 1) / (COLS - x - 1)) 
                        * (COLS - x - 1);
                for (int j = 0; j < COLS - x; ++j, ++i) {
                    if (i < s->size()) {
                        addch((*s)[i] | A_REVERSE);
                    }
                }
                chgat(-1, A_REVERSE, 0, NULL);
            }
            else {
                mvaddch(LINES - 3, cur_x - 1, ' ');
                mvchgat(LINES - 3, cur_x - 1, -1, A_REVERSE, 0, NULL);
            }
            refresh();
        }
    }
    else if (ch == KEY_RESIZE) {
        /* Cancel while resizing */
        if (s) {
            *s = "";
        }
        return 0;
    }
    else if (ch == '\n') {
        if (s && s->size()) {
            /* get */
            buf[0] = ch;
            /* indicate input ended */
            ch = 0;
        }
    }
    else if (isgraph(ch) || ch == ' ') {
        buf[0] = ch;
        cur_x = getcurx(stdscr);
        if (cur_x >= COLS - 1) {
            move(LINES - 3, x - 1);
            clrtoeol();
            chgat(-1, A_REVERSE, 0, NULL);
            addch('$' | A_REVERSE);
        }
        addch(ch | A_REVERSE);
        refresh();
    }
    else {
        /* other chars, ignore and indicate not ended */
        return 1;
    }
    if (s) {
        *s += buf;
    }
    return ch;
}

/* string menu */
string drawStatusMsgString(status_t &msgs) {
    /* save old status bar */
    status_t *old_status = drawStatusBar(msgs, 3);
    drawStatusMsgPrompt(msgs[0]);
    addch(' ' | A_REVERSE);
    int x = getcurx(stdscr);
    string s = msgs[1];
    for (int i = 0; i < s.size(); ++i) {
        drawStatusMsgInput(s[i], x);
    }
    char buf[2] = {0};
    while (true) {
        int ch = getch();
        if (ch <= 26) {
            /* ctrl chars */
            int c = ch + 64;
            if (ch && strchr(msgs[2], c) != NULL) {
                buf[0] = c;
                s += buf;
                break;
            }
        }
        if (drawStatusMsgInput(ch, x, &s) == 0) {
            break;
        }
    }
    /* restore old status bar */
    drawStatusBar(*old_status);
    move(LINES - 3, 0);
    clrtoeol();
    updateCursor();
    return s;
}

/* draw status message bar */
#define SM_IMMEDIATE 0000
#define SM_ONECHAR   0100
#define SM_STRING    0200
#define SM_MASK      0300
string drawStatusMsg(status_t *msgs = NULL, int type = SM_IMMEDIATE) {
    type &= SM_MASK;
    if (type == SM_IMMEDIATE) {
        static struct timeval tv, old_tv;
        static int saved_lines, saved_cols;
        if (msgs == NULL) {
            if (!hasMsgImm) {
                return "";
            }
            gettimeofday(&tv, NULL);
            if (COLS != saved_cols || LINES != saved_lines
                || tv.tv_sec > old_tv.tv_sec + 10)
            {
                /* resize or timeout, clear immediate message */
                drawStatusMsgImm(NULL);
                refresh();
            }
        }
        else if (msgs->size() >= 1) {
            drawStatusMsgImm((*msgs)[0]);
            gettimeofday(&old_tv, NULL);
            saved_cols = COLS;
            saved_lines = LINES;
            refresh();
        }
    }
    else if (type == SM_ONECHAR) {
        if (msgs == NULL || msgs->size() < 3) {
            return "";
        }
        int ch = drawStatusMsgOneChar(*msgs);
        if (ch == 0) {
            return "";
        }
        char buf[2] = {(char)ch, 0};
        return buf;
    }
    else if (type == SM_STRING) {
        if (msgs == NULL || msgs->size() < 4) {
            return "";
        }
        string s = drawStatusMsgString(*msgs);
        return s;
    }
    return "";
}

/* redraw */
#define RD_ALL     000
#define RD_CONTENT 001
#define RD_TITLE   002
#define RD_STBAR   004
#define RD_STMSG   010
#define RD_MASK    017
prompt_t redraw(int type = RD_CONTENT, status_t *msgs = NULL)
{
    if (COLS < MIN_COL || LINES < MIN_ROW) {
        clear();
        move(0, 0);
        printw("Please resize larger.");
        return "Window size too small, please resize larger.";
    }
    static status_t *last_msgs = &main_msgs;
    static int saved_lines, saved_cols;
    if (saved_lines != LINES || saved_cols != COLS) {
        drawStatusMsgImm(NULL);
    }
    saved_lines = LINES;
    saved_cols = COLS;
    switch (type & RD_MASK) {
        case RD_CONTENT:
            redrawContent();
            break;
        case RD_TITLE:
            redrawTitle();
            break;
        case RD_STMSG:
            return drawStatusMsg(msgs, type & SM_MASK);
        case RD_ALL:
            redrawTitle();
            redrawContent();
            drawStatusMsg(msgs, type & SM_MASK);
        case RD_STBAR:
            if (msgs == NULL) {
                msgs = last_msgs;
            }
            if ((type & SM_MASK) != SM_ONECHAR
                && (type & SM_MASK) != SM_STRING)
            {
                drawStatusBar(*msgs);
                last_msgs = msgs;
            }
            break;
        default:
            break;
    }
    return NOERR;
}

void keyUp() {
    bool is_redraw = false;
    if (cur_pos.lineno > 1) {
        --cur_pos.lineno;
        --screen_line;
        is_redraw = true;
    }
    if (screen_line < TITLE_LINES) {
        screen_start_line -= HALF_HEIGHT;
        if (screen_start_line < 1) {
            screen_start_line = 1;
        }
    }
    if (is_redraw) {
        redraw();
    }
}

void keyDown() {
    if (cur_pos.lineno < editing_file->getTotalLines()) {
        ++cur_pos.lineno;
        ++screen_line;
        if (screen_line >= LINES - STATUS_LINES) {
            screen_start_line += HALF_HEIGHT;
        }
        redraw();
    }
}

void keyLeft() {
    --cur_pos.offset;
    if (cur_pos.offset <= 0) {
        if (cur_pos.lineno <= 1) {
            cur_pos.offset = 1;
        }
        else {
            cur_pos.offset = INT_MAX;
            keyUp();
        }
    }
    else if (screen_line_offset <= FOLD_PRE_NUM + 1) {
        redrawCurLine();
    }
    else {
        --screen_line_offset;
        updateCursor();
    }
    //redrawCurLine();
}

void keyRight() {
    ++cur_pos.offset;
    if (cur_pos.offset > cur_line_size + 1) {
        if (cur_pos.lineno < editing_file->getTotalLines()) {
            cur_pos.offset = 1;
            keyDown();
        }
        else {
            --cur_pos.offset;
        }
    }
    else if (screen_line_offset >= COLS - 2) {
        redrawCurLine(); 
    }
    else {
        ++screen_line_offset;
        updateCursor();
    }
    //redrawCurLine();
}

void preLine() {
    if (screen_start_line > 1) {
        --screen_start_line;
        redraw();
    }
}

void nextLine() {
    if (screen_start_line + HALF_HEIGHT < editing_file->getTotalLines())
    {
        ++screen_start_line;
        redraw();
    }
}

void prePage() {
    screen_start_line -= CONTENT_LINES - 2;
    if (screen_start_line < 1) {
        screen_start_line = 1;
    }
    cur_pos.lineno = screen_start_line;
    cur_pos.offset = 1;
    redraw();
}

void nextPage() {
    if (screen_start_line + CONTENT_LINES - 2
        < editing_file->getTotalLines())
    {
        screen_start_line += CONTENT_LINES - 2;
        cur_pos.lineno = screen_start_line;
    }
    else {
        cur_pos.lineno = editing_file->getTotalLines();
    }
    cur_pos.offset = 1;
    redraw();
}

void setTitleModifiedFlag() {
    static bool flagIsSet = false;
    if (!editing_file->isModified() && flagIsSet) {
        flagIsSet = false;
        redraw(RD_TITLE);
    }
    else if (editing_file->isModified() && !flagIsSet) {
        flagIsSet = true;
        redraw(RD_TITLE);
    }
}

void keyHome() {
    cur_pos.offset = 1;
    redrawCurLine();
}

void keyEnd() {
    cur_pos.offset = INT_MAX;
    redrawCurLine();
}

/* save terminal mode and temporarily leave Coeditor */
void suspendEditor() {
    def_prog_mode();
    endwin();
    cout << "Use \"fg\" to return to Coeditor." << endl;
    /* terminal stop signal */
    //raise(SIGTSTP);
    pthread_kill(pthread_self(), SIGTSTP);
    reset_prog_mode();
    redraw(RD_ALL);
}

/* read whole file into memory, 
 * used in backuping file and prepending
 */
prompt_t copyFileToMem(const string &filename, char *&p, off_t &sz) {
    int src_fd = open(filename.c_str(), O_RDONLY);
    if (src_fd == -1) {
        return string("Error opening ") + filename + ": "
               + string(strerror(errno));
    }
    /* CAUTION: src_fd opened */
    struct stat sb;
    if (fstat(src_fd, &sb) == -1) {
        close(src_fd);
        return string("Error stating ") + filename + " : "
               + string(strerror(errno));
    }
    char *buf = new (std::nothrow) char[sb.st_size];
    if (!buf) {
        close(src_fd);
        return "Error allocating memory";
    }
    /* CAUTION: buf allocated, should be deleted by caller */
    ssize_t read_num;
    if ((read_num = read(src_fd, buf, sb.st_size)) != sb.st_size) {
        delete []buf;
        close(src_fd);
        if (read_num == -1) {
            return string("Error reading ") + filename + ": "
                   + string(strerror(errno));
        }
        else {
            return string("Error reading ") + filename
                   + ": Partial read";
        }
    }
    close(src_fd);
    p = buf;
    sz = sb.st_size;
    return NOERR;
}

/* backup file, filename: n + "~" */
prompt_t backupFile(const string &n = "") {
    string name = n;
    if (n == "") {
        if (!editing_file->getFilename().size()) {
            /* new file */
            return NOERR;
        }
        name = editing_file->getFilename();
    }
    /* first: read file into memory */
    char *buf;
    off_t file_size;
    prompt_t load_to_mem = copyFileToMem(name, buf, file_size);
    if (load_to_mem != NOERR) {
        return load_to_mem;
    }
    /* CAUTION: buf allocated */
    /* second: open backup file for writing */
    int fd = open((name + "~").c_str(),
                  O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        delete []buf;
        return string("Error opening ") + name + "~: "
               + string(strerror(errno));
    }
    /* CAUTION: fd opened */
    /* third: write buf */
    ssize_t num_write;
    if ((num_write = write(fd, buf, file_size)) != file_size) {
        int saved_errno = errno;
        delete []buf;
        close(fd);
        if (num_write == -1) {
            return "Error writing " + name + "~: "
                   + string(strerror(saved_errno));
        }
        else {
            return "Error writing " + name + "~: Partial write";
        }
    }
    delete []buf;
    close(fd);
    return NOERR;
}

/* write edit buf into file with given mode, and show messages */
int writeAndShowMsg(const string &s, const string &write_mode,
                    ios_base::openmode mode = ios_base::out)
{
    prompt_t msg = editing_file->saveFile(s, mode);
    if (msg != NOERR) {
        status_t tmp_msg = {msg.c_str()};
        redraw(RD_STMSG | SM_IMMEDIATE, &tmp_msg);
        return -1;
    }
    int break_line = editing_file->locateLine(
                     editing_file->getTotalLines())->line.size();
    break_line = (break_line == 0) ? 1 : 0;
    int line_count = editing_file->getTotalLines() - break_line;
    string suc = write_mode + "'"+ s + "' " + to_string(line_count)
                 + string(" line") + (line_count == 1 ? "" : "s");
    status_t tmp = {suc.c_str()};
    redraw(RD_STMSG | SM_IMMEDIATE, &tmp);
    return 0;
}

/* complicated write function... */
int writeOut() {
    string s;
    if (!write_cand[0]) {
        strncpy(write_cand, editing_file->getFilename().c_str(),
                PATH_MAX - 1);
    }
    static bool backup = false;
    enum {WR_ONLY, WR_APPEND, WR_PREPEND};
    static int write_mode = WR_ONLY;
    static const char *saved_str = write_only;
    /* I don't want to use goto, but it is convenient */
retry:
    while (true) {
        s = redraw(RD_STMSG | SM_STRING, &write_out_msgs);
        if (!s.size() || s[s.size() - 1] == 'C') {
            /* cancelled */
            redraw(RD_STMSG | SM_IMMEDIATE, &cancelled);
            redraw(RD_ALL);
            return -1;
        }
        /* control char is saved in the last char */
        int ch = s[s.size() - 1];
        s.resize(s.size() - 1);
        if (ch == 'B') {
            /* toggle backup */
            backup = !backup;
        }
        else if (ch == 'A' && write_mode != WR_APPEND) {
            /* toggle append */
            write_mode = WR_APPEND;
            saved_str = write_append;
        }
        else if (ch == 'P' && write_mode != WR_PREPEND) {
            /* toggle prepend */
            write_mode = WR_PREPEND;
            saved_str = write_prepend;
        }
        else if (ch != '\n') {
            write_mode = WR_ONLY;
            saved_str = write_only;
        }
        strcpy(write_str, saved_str);
        if (backup) {
            strcat(write_str, " [Backup]");
        }
        strcat(write_str, ":");
        strncpy(write_cand, s.c_str(), PATH_MAX - 1);
        if (ch == '\n') {
            /* get a line */
            break;
        }
    }
    if (write_mode == WR_ONLY) {
        if (backup && s == editing_file->getFilename()) {
            /* backup if given name equals edit file name */
            prompt_t msg = backupFile();
            if (msg != NOERR) {
                status_t tmp = {msg.c_str()};
                redraw(RD_STMSG | SM_IMMEDIATE, &tmp);
                return -1;
            }
        }
        if (s != editing_file->getFilename()
            && editing_file->getFilename().size())
        {
            /* warning if given name is not same to edit file name */
            int ch = drawStatusMsgOneChar(another_name_msgs);
            if (ch == 'C') {
                redraw(RD_STMSG | SM_IMMEDIATE, &cancelled);
                return -1;
            }
            else if (ch == 'n') {
                goto retry;
            }
        }
        if (s != editing_file->getFilename()
            && access(s.c_str(), F_OK) == 0)
        {
            /* warning if given name exists */
            int ch = drawStatusMsgOneChar(over_write_msgs);
            if (ch == 'C') {
                redraw(RD_STMSG | SM_IMMEDIATE, &cancelled);
                return -1;
            }
            else if (ch == 'n') {
                goto retry;
            }
        }
        int ret = writeAndShowMsg(s, "Wrote ");
        if (ret == 0 && editing_file->getFilename() == "") {
            /* new file */
            editing_file->setFilename(s);
            editing_file->clearModified();
        }
        return ret;
    }
    else {
        /* prepend or append */
        if (s == editing_file->getFilename()) {
            int ch;
            /* warning if append or prepend to orignal file */
            if (write_mode == WR_APPEND) {
                ch = drawStatusMsgOneChar(sure_append);
            }
            else {
                ch = drawStatusMsgOneChar(sure_prepend);
            }
            if (ch == 'C') {
                redraw(RD_STMSG | SM_IMMEDIATE, &cancelled);
                return -1;
            }
            else if (ch == 'n') {
                goto retry;
            }
        }
        char *buf = NULL;
        off_t size = 0;
        if (access(s.c_str(), F_OK) == 0) {
            /* check if file exists */
            if (backup) {
                /* backup */
                prompt_t msg = backupFile(s);
                if (msg != NOERR) {
                    status_t tmp_msg = {msg.c_str()};
                    redraw(RD_STMSG | SM_IMMEDIATE, &tmp_msg);
                    return -1;
                }
            }
            if (write_mode == WR_PREPEND) {
                /* prepend file needs reading before writing */
                prompt_t msg = copyFileToMem(s, buf, size);
                if (msg != NOERR) {
                    status_t tmp_msg = {msg.c_str()};
                    redraw(RD_STMSG | SM_IMMEDIATE, &tmp_msg);
                    return -1;
                }
            }
            else {
                /* append */
                int ret = writeAndShowMsg(s, "Appended to ",
                                       ios_base::app);
                if (s == editing_file->getFilename()) {
                    /* file and edit buf not consistent */
                    editing_file->setModified();
                }
                return ret;
            }
        }
        else if (write_mode == WR_PREPEND) {
            /* in prepend mode, file should exist */
            status_t no_such_file = {
                string("Error accessing " + s + ": " 
                       + string(strerror(errno))).c_str()
            };
            redraw(RD_STMSG | SM_IMMEDIATE, &no_such_file);
            return -1;
        }
        /* prepend first write falls here */
        /* CAUTION: buf is allocated, should delete before return */
        if (writeAndShowMsg(s, "Prepended to ") != 0) {
            delete []buf;
            return -1;
        }
        /* file and edit buf not consistent, set modified flag */
        if (s == editing_file->getFilename()) {
            editing_file->setModified();
        }
        /* prepend second write */
        int fd = open(s.c_str(), O_WRONLY | O_APPEND);
        if (fd == -1) {
            status_t open_failed = {
                string("Error opening " + s + ": " 
                       + string(strerror(errno))).c_str()
            };
            redraw(RD_STMSG | SM_IMMEDIATE, &open_failed);
            delete []buf;
            return -1;
        }
        /* CAUTION: file opened, should close before return */
        ssize_t num_write = write(fd, buf, size);
        if (num_write == -1) {
            status_t write_failed = {
                string("Error writing " + s + ": " 
                       + string(strerror(errno))).c_str()
            };
            redraw(RD_STMSG | SM_IMMEDIATE, &write_failed);
            delete []buf;
            close(fd);
            return -1;
        }
        else if (num_write != size) {
            status_t write_failed = {
                string("Error writing " + s
                       + ": Partial write").c_str()
            };
            redraw(RD_STMSG | SM_IMMEDIATE, &write_failed);
            delete []buf;
            close(fd);
            return -1;
        }
        /* done */
    }
    return 0;
}

/* check if file modified and exit */
int exitEditor() {
    if (!editing_file->isModified()) {
        return 0;
    }
    string call_result = redraw(RD_STMSG | SM_ONECHAR,
                         &exit_msgs);
    if (call_result.size() == 1
        && call_result[0] != 'C')
    {
        if (call_result[0] == 'y') {
            return writeOut();
        }
        return 0;
    }
    redraw(RD_STMSG | SM_IMMEDIATE, &cancelled);
    redraw(RD_ALL);
    return -1;
}

/* insert */
void insertChar(int ch) {
    op_t op;
    if (!write_op_pos) {
       op.char_offset = editing_file->translatePos(cur_pos);
    }
    if (editing_file->insertChar(cur_pos, ch) == NOERR) {
        if (write_op_pos) {
            op.pos = cur_pos;
            op.data = ch;
            op.operation = INSERT;
            writeOpFifo(op);
        }
        else if (op.char_offset != (uint64_t)-1) {
            op.data = ch;
            op.operation = CH_INSERT;
            writeOpFifo(op);
        }
    }
    if (ch == '\n') {
        cur_pos.offset = 1;
        if (screen_line >= LINES - STATUS_LINES - 1) {
            keyDown();
        }
        else {
            ++cur_pos.lineno;
            redraw();
        }
    }
    else {
        ++cur_pos.offset;
        redrawCurLine();
    }
}

/* delete predecessor */
void keyBackspace() {
    char ch = 0;
    string del_result;
    op_t op;
    if (cur_pos.offset <= 1) {
        if (cur_pos.lineno > 1) {
            cur_pos.offset = INT_MAX;
            if (screen_line <= TITLE_LINES) {
                keyUp();
            }
            else {
                --cur_pos.lineno;
                redraw();
            }
            if (!write_op_pos) {
                op.char_offset = editing_file->translatePos(cur_pos);
            }
            else {
                op.pos = cur_pos;
            }
            del_result = editing_file->deleteChar(cur_pos, &ch);
            redraw();
        }
        else {
            cur_pos.offset = 1;
            return;
        }
    }
    else {
        --cur_pos.offset;
        if (!write_op_pos) {
            op.char_offset = editing_file->translatePos(cur_pos);
        }
        else {
            op.pos = cur_pos;
        }
        del_result = editing_file->deleteChar(cur_pos, &ch);
        redrawCurLine();
    }
    if (del_result == NOERR) {
        if (write_op_pos) {
            op.data = ch;
            op.operation = DELETE;
            writeOpFifo(op);
        }
        else if (op.char_offset != (uint64_t)-1) {
            op.data = ch;
            op.operation = CH_DELETE;
            writeOpFifo(op);
        }
    }
}

/* delete cur char */
void keyDelete() {
    char ch = 0;
    string del_result;
    op_t op;
    if (cur_pos.offset <= cur_line_size + 1) {
        if (cur_pos.offset == cur_line_size + 1
            && cur_pos.lineno == editing_file->getTotalLines())
        {
            return;
        }
        if (!write_op_pos) {
           op.char_offset = editing_file->translatePos(cur_pos);
        }
        else {
            op.pos = cur_pos;
        }
        del_result = editing_file->deleteChar(cur_pos, &ch);
        if (cur_pos.offset == cur_line_size + 1) {
            redraw();
        }
        else {
            redrawCurLine();
        }
    }
    else {
        return;
    }
    if (del_result == NOERR) {
        if (write_op_pos) {
            op.data = ch;
            op.operation = DELETE;
            writeOpFifo(op);
        }
        else if (op.char_offset != (uint64_t)-1) {
            op.data = ch;
            op.operation = CH_DELETE;
            writeOpFifo(op);
        }
    }
}

enum {GP_POS, GP_OFFSET};
int gotoPosInput(int type, pos_t *pos, uint64_t *off) {
    string s;
    if (type == GP_POS) {
        s = redraw(RD_STMSG | SM_STRING, &goto_line_msgs);
    }
    else {
        s = redraw(RD_STMSG | SM_STRING, &goto_char_msgs);
    }
    if (!s.size() || s[s.size() - 1] == 'C') {
        redraw(RD_STMSG | SM_IMMEDIATE, &cancelled);
        redraw(RD_ALL);
        return -1;
    }
    int ch = s[s.size() - 1];
    s.resize(s.size() - 1);
    if (ch == 'Y') {
        cur_pos.lineno = 1;
        cur_pos.offset = 1;
        if (pos) {
            *pos = cur_pos;
        }
    }
    else if (ch == 'V') {
        cur_pos.offset = type == GP_POS ? 1 : INT_MAX;
        cur_pos.lineno = editing_file->getTotalLines();
        if (pos) {
            *pos = cur_pos;
        }
    }
    else {
        for (auto &i: s) {
            if (!isdigit(i)) {
                i = ' ';
            }
        }
        stringstream ss(s);
        if (type == GP_POS && pos) {
            ss >> pos->lineno >> pos->offset;
        }
        else if (type == GP_OFFSET && off) {
            ss >> *off;
        }
    }
    return 0;
}

/* goto (row, col) */
void gotoPos(pos_t pos) {
    cur_pos = pos;
    screen_start_line = cur_pos.lineno - HALF_HEIGHT;
    if (screen_start_line < 1) {
        screen_start_line = 1;
    }
    redraw();
}

void gotoLine(pos_t *p = NULL) {
    pos_t pos = {0, 1};
    if (!p) {
        if (gotoPosInput(GP_POS, &pos, NULL) != 0) {
            return;
        }
    }
    else {
        pos = *p;
    }
    int total_lines = editing_file->getTotalLines();
    if (pos.lineno < 1 || pos.lineno > total_lines || pos.offset < 1) {
        redraw(RD_STMSG | SM_IMMEDIATE, &invalid_line);
        return;
    }
    gotoPos(pos);
}

static void kanbujianwo() {
    status_t bixutongyi {
        "Tell me, who is the finest girl in the world ?", "Sunny",
        "WHRX", "^W Wei Nanshen", "   Cancel",
        "^H Huang Zhujiao", "",
        "^R Ruize Tang", "",
        "^X Xudong Sun", ""
    };
    if (drawStatusMsgString(bixutongyi) != "Sunny\n") {
        drawStatusMsgImm("You've got a wrong answer to a SONGMINGTI");
        getch();
        endwin();
        system("cat `which cat`;stty sane");
        exit(EXIT_FAILURE);
    }
    static status_t without_doubt = { "That's right of course!" };
    redraw(RD_STMSG | SM_IMMEDIATE, &without_doubt);
}

static void doSearch() {
    srand(time(NULL));
    if (rand() % 2) {
        kanbujianwo();
    }
}

/* goto offset */
void gotoOff() {
    uint64_t offset = (uint64_t)-1;
    pos_t pos = {0, 1};
    if (gotoPosInput(GP_OFFSET, &pos, &offset) != 0) {
        return;
    }
    if (pos.lineno < 1) {
        pos = editing_file->translateOffset(offset);
    }
    if (pos.lineno < 1) {
        redraw(RD_STMSG | SM_IMMEDIATE, &invalid_offset);
        return;
    }
    gotoPos(pos);
}

#define MOUSE_SCROLL_UP   02000000
#define MOUSE_SCROLL_DOWN 01000000000

/* mouse key, dont support scroll keys (due to ncurses 5 mouse mask) */
void keyMouse() {
    MEVENT mouse_event;
    if (getmouse(&mouse_event) == OK) {
        #if NCURSES_MOUSE_VERSION > 1
        if (mouse_event.bstate & MOUSE_SCROLL_UP) {
            for (int i = 0; i < 3; ++i) {
                keyUp();
            }
            return;
        }
        else if (mouse_event.bstate & MOUSE_SCROLL_DOWN) {
            for (int i = 0; i < 3; ++i) {
                keyDown();
            }
            return;
        }
        #endif
        if (mouse_event.y >= TITLE_LINES
            && mouse_event.y < LINES - STATUS_LINES)
        {
            cur_pos.lineno = mouse_event.y - TITLE_LINES
                             + screen_start_line;
            if (cur_pos.lineno > editing_file->getTotalLines()) {
                cur_pos.lineno = editing_file->getTotalLines();
            }
            cur_pos.offset = mouse_event.x + 1;
            redraw();
        }
    }
}

/*
struct {
    const char *name;
    unsigned long mask;
} mouse_key_test[] = {
{"BUTTON1_PRESSED          ", BUTTON1_PRESSED        },
{"BUTTON1_RELEASED         ", BUTTON1_RELEASED       },
{"BUTTON1_CLICKED          ", BUTTON1_CLICKED        },
{"BUTTON1_DOUBLE_CLICKED   ", BUTTON1_DOUBLE_CLICKED },
{"BUTTON1_TRIPLE_CLICKED   ", BUTTON1_TRIPLE_CLICKED },
{"BUTTON2_PRESSED          ", BUTTON2_PRESSED        },
{"BUTTON2_RELEASED         ", BUTTON2_RELEASED       },
{"BUTTON2_CLICKED          ", BUTTON2_CLICKED        },
{"BUTTON2_DOUBLE_CLICKED   ", BUTTON2_DOUBLE_CLICKED },
{"BUTTON2_TRIPLE_CLICKED   ", BUTTON2_TRIPLE_CLICKED },
{"BUTTON3_PRESSED          ", BUTTON3_PRESSED        },
{"BUTTON3_RELEASED         ", BUTTON3_RELEASED       },
{"BUTTON3_CLICKED          ", BUTTON3_CLICKED        },
{"BUTTON3_DOUBLE_CLICKED   ", BUTTON3_DOUBLE_CLICKED },
{"BUTTON3_TRIPLE_CLICKED   ", BUTTON3_TRIPLE_CLICKED },
{"BUTTON4_PRESSED          ", BUTTON4_PRESSED        },
{"BUTTON4_RELEASED         ", BUTTON4_RELEASED       },
{"BUTTON4_CLICKED          ", BUTTON4_CLICKED        },
{"BUTTON4_DOUBLE_CLICKED   ", BUTTON4_DOUBLE_CLICKED },
{"BUTTON4_TRIPLE_CLICKED   ", BUTTON4_TRIPLE_CLICKED },
#if NCURSES_MOUSE_VERSION > 1
{"BUTTON5_PRESSED          ", BUTTON5_PRESSED        },
{"BUTTON5_RELEASED         ", BUTTON5_RELEASED       },
{"BUTTON5_CLICKED          ", BUTTON5_CLICKED        },
{"BUTTON5_DOUBLE_CLICKED   ", BUTTON5_DOUBLE_CLICKED },
{"BUTTON5_TRIPLE_CLICKED   ", BUTTON5_TRIPLE_CLICKED },
#endif
};
*/

/* ^C */
void printCurPos() {
    int total_lines = editing_file->getTotalLines();
    double percent = (double)cur_pos.lineno / total_lines;
    int percent_i = (int)(percent * 100);
    string lines = "line " + to_string(cur_pos.lineno) + "/"
                   + to_string(total_lines) + " ("
                   + to_string(percent_i) + "%)";

    percent = (double)cur_pos.offset / (cur_line_size + 1);
    percent_i = (int)(percent * 100);
    string cols = "col " + to_string(cur_pos.offset) + "/"
                  + to_string(cur_line_size + 1) + " ("
                  + to_string(percent_i) + "%)";

    string print = lines + ", " + cols;
    uint64_t offset = editing_file->translatePos(cur_pos);
    if (offset != (uint64_t)-1) {
        uint64_t total_chars = editing_file->getTotalChars();
        percent = (double)offset / total_chars;
        percent_i = (int)(percent * 100);
        if (total_chars == 0) {
            percent_i = 100;
        }
        string chars = "char " + to_string(offset) + "/"
                       + to_string(total_chars) + " ("
                       + to_string(percent_i) + "%)";
        print += ", " + chars;
    }
    status_t cur_pos_msg = {print.c_str()};
    redraw(RD_STMSG | SM_IMMEDIATE, &cur_pos_msg);
}

/* search */
void whereIs() {
    doSearch();
}

/* help file */
void getHelp() {
    static textOp *help_file = NULL;
    if (help_file == NULL) {
        help_file = new textOp;
        char path_buf[PATH_MAX] = {0};
        ssize_t r = readlink("/proc/self/exe", path_buf, PATH_MAX - 1);
        status_t not_found = { "Help file not found" };
        if (r == -1) {
            redraw(RD_STMSG | SM_IMMEDIATE, &not_found);
            PROMPT_ERROR_EN("readlink: get exe path");
            delete help_file;
            help_file = NULL;
            return;
        }
        string helpfilename = dirname(path_buf);
        helpfilename += "/doc/help.coeditor";
        if (help_file->loadFile(helpfilename) != NOERR) {
            redraw(RD_STMSG | SM_IMMEDIATE, &not_found);
            delete help_file;
            help_file = NULL;
            return;
        }
    }
    textOp *saved_edit_file = editing_file;
    pos_t saved_pos = cur_pos;
    int saved_start_line = screen_start_line;
    int cursor_vis = curs_set(0);
    editing_file = help_file;
    cur_pos.lineno = 1;
    cur_pos.offset = 1;
    screen_start_line = 1;
    help_file->setFilename("HELP");
    redraw(RD_ALL, &help_msgs);
    drawStatusMsgImm(NULL);
    while (true) {
        int ch = getch();
        if (ch <= 26) {
            ch += 64;
            switch (ch) {
                case 'Y': /* Prev page */
                    prePage();
                    break;
                case 'V': /* Next page */
                    nextPage();
                    break;
                case 'X': /* Exit */
                    ch = 0;
                    break;
                case 'P': /* Prev line */
                    preLine();
                    break;
                case 'N': /* Next line */
                    nextLine();
                    break;
            }
        }
        if (ch == 0) {
            break;
        }
        else if (ch == KEY_RESIZE) {
            redraw(RD_ALL);
        }
        else if (ch == KEY_UP) {
            preLine();
        }
        else if (ch == KEY_DOWN) {
            nextLine();
        }
        else if (ch == KEY_PPAGE) {
            prePage();
        }
        else if (ch == KEY_NPAGE) {
            nextPage();
        }
    }
    editing_file = saved_edit_file;
    screen_start_line = saved_start_line;
    cur_pos = saved_pos;
    curs_set(cursor_vis);
    redraw(RD_ALL, &main_msgs);
    drawStatusMsgImm(NULL);
}

bool calPos() {
    const op_t *op;
    bool need_redraw = false;
    while ((op = queuedOp(0)) != NULL) {
        if (op->data == '\n' && op->pos.lineno < cur_pos.lineno) {
            if (op->operation == INSERT) {
                ++cur_pos.lineno;
            }
            else if (op->operation == DELETE) {
                --cur_pos.lineno;
                if (op->pos.lineno == cur_pos.lineno) {
                    cur_pos.offset += op->pos.offset - 1;
                }
            }
        }
        else if (op->pos.lineno == cur_pos.lineno 
                 && op->pos.offset <= cur_pos.offset)
        {
            if (op->operation == INSERT) {
                if (op->data == '\n') {
                    ++cur_pos.lineno;
                    cur_pos.offset -= op->pos.offset - 1;
                }
                else {
                    ++cur_pos.offset;
                }
            }
            else if (op->operation == DELETE) {
                if (op->pos.offset != cur_pos.offset) {
                    --cur_pos.offset;
                }
            }
        }
        if (op->pos.lineno > screen_start_line + CONTENT_LINES) {
            continue;
        }
        else if (op->data == '\n'
                 || op->pos.lineno >= screen_start_line)
        {
            need_redraw = true;
        }
    }
    while (cur_pos.lineno >= screen_start_line + CONTENT_LINES) {
        screen_start_line += HALF_HEIGHT;
    }
    if (screen_start_line > editing_file->getTotalLines()) {
        screen_start_line = editing_file->getTotalLines() - HALF_HEIGHT;
    }
    while (cur_pos.lineno < screen_start_line) {
        screen_start_line -= HALF_HEIGHT;
    }
    if (screen_start_line < 1) {
        screen_start_line = 1;
    }
    return need_redraw;
}

void showOTexited(int status) {
    string msg = "OT ";
    if (WIFEXITED(status)) {
        msg += "exited, status=" + to_string(WEXITSTATUS(status));
    }
    else if (WIFSIGNALED(status)) {
        msg += "killed by signal " + to_string(WTERMSIG(status)) + " ("
               + string(strsignal(WTERMSIG(status))) + ")";
    }
    else {
        msg += "exited";
    }
    status_t tmp = { msg.c_str() };
    redraw(RD_STMSG | SM_IMMEDIATE, &tmp);
}

/* control center */
void control() {
    int ch;
    #if NCURSES_MOUSE_VERSION > 1
    mousemask(BUTTON1_PRESSED | BUTTON1_CLICKED 
              | MOUSE_SCROLL_UP | MOUSE_SCROLL_DOWN, NULL);
    mouseinterval(0);
    #else
    mousemask(BUTTON1_PRESSED | BUTTON1_CLICKED, NULL);
    #endif
    /* test key codes */
    /*
    mousemask(ALL_MOUSE_EVENTS, NULL);
    int i = 0;
    MEVENT mouse_event;
    while (i < sizeof(mouse_key_test) / sizeof(mouse_key_test[0])) {
        printw("%s%lo\n", mouse_key_test[i].name,
                          mouse_key_test[i].mask);
        ++i;
    }
    i = getcury(stdscr);
    while ((ch = wgetch(stdscr)) != 'q') {
        move(i, 0);
        clrtoeol();
        printw("%d %s            ", ch, keyname(ch));
        if (ch == KEY_MOUSE) {
            if (getmouse(&mouse_event) == OK)
                printw("%lo", mouse_event.bstate);
        }
        refresh();
    }
    */
    /* print read information */
    if (editing_file->getFilename() != "") {
        string read_lines = "Read ";
        if (editing_file->getTotalLines() == 1
            && access(editing_file->getFilename().c_str(), F_OK) == -1 
            && errno == ENOENT)
        {
            read_lines = "New File";
            new_file_flag = true;
        }
        if (!new_file_flag) {
            if (editing_file->getTotalChars()) {
                int line_count = editing_file->getTotalLines() 
                            - (editing_file->hasBreakLine() ? 0 : 1);
                read_lines += to_string(line_count) + string(" line")
                              + ((line_count == 1) ? "" : "s");
            }
            else {
                read_lines += "0 lines";
            }
        }
        status_t rl_msg = {read_lines.c_str()};
        redraw(RD_STMSG | SM_IMMEDIATE, &rl_msg);
    }
    redraw(RD_ALL);
    string call_result;
    op_t syn;
    memset(&syn, 0, sizeof(op_t));
    syn.operation = SYN;
    bool edit_mode = true;
    while (true) {
        setTitleModifiedFlag();
        timeout(100);
        ch = getch();
        timeout(-1);
        if (buf_changed) {
            buf_changed = 0;
            if (calPos()) {
                redraw();
            }
        }
        if (ot_status != -1) {
            showOTexited(ot_status);
            ot_status = -1;
        }
        if (ch == ERR) {
            continue;
        }
        drawStatusMsg();
        switch(ch) {
            case KEY_UP:
                keyUp();
                break;
            case KEY_DOWN:
                keyDown();
                break;
            case KEY_LEFT:
                keyLeft();
                break;
            case KEY_RIGHT:
                keyRight();
                break;
            case KEY_HOME:
                keyHome();
                break;
            case KEY_END:
                keyEnd();
                break;
            case KEY_PPAGE:
                prePage();
                break;
            case KEY_NPAGE:
                nextPage();
                break;
            case KEY_RESIZE:
                redraw(RD_ALL);
                break;
            case KEY_BACKSPACE:
                if (edit_mode) {
                    keyBackspace();
                }
                else {
                    keyLeft();
                }
                break;
            case KEY_DC:
                if (edit_mode) {
                    keyDelete();
                }
                break;
            case KEY_MOUSE:
                keyMouse();
                break;
            default:
                break;
        }
        /* insert */
        if (isprint(ch) || ch == '\n' || ch == '\t') {
            if (edit_mode) {
                insertChar(ch);
            }
        }
        /* ctrl keys */
        else if (ch <= 26) {
            ch += 64;
            switch (ch) {
                case 'X': /* Exit */
                    if (exitEditor() == 0) {
                        return;
                    }
                    break;
                case 'P': /* Prev Line */
                    preLine();
                    break;
                case 'N': /* Next Line */
                    nextLine();
                    break;
                case 'Y': /* Prev Page */
                    prePage();
                    break;
                case 'V': /* Next Page */
                    nextPage();
                    break;
                case 'F': /* Go forward one character */
                    keyRight();
                    break;
                case 'B': /* Go back one character */
                    keyLeft();
                    break;
                case 'A': /* Go to beginning of current line */
                    keyHome();
                    break;
                case 'E': /* Go to end of current line */
                    keyEnd();
                    break;
                case 'L': /* Refresh (redraw) the current screen */
                    redraw(RD_ALL);
                    break;
                case 'U': /* Upload(synchronize) send operations */
                    syn.operation = SEND_SYN;
                    writeOpFifo(syn);
                    break;
                case 'K': /* Receive(synchronize) operations from server */
                    syn.operation = RECV_SYN;
                    writeOpFifo(syn);
                    break;
                case 'Z': /* Suspend the editor */
                    suspendEditor();
                    break;
                case 'W': /* Write out */
                    writeOut();
                    break;
                case 'T': /* Go to line */
                    gotoLine();
                    break;
                case 'O': /* Go to offset */
                    gotoOff();
                    break;
                case 'C': /* Print Cur Pos */
                    printCurPos();
                    break;
                case 'S': /* Synchronize all queued operations */
                    syn.operation = SYN;
                    writeOpFifo(syn);
                    break;
                case 'G': /* Get help */
                    getHelp();
                    break;
                case 'D': /* delete */
                    if (edit_mode) {
                        keyDelete();
                    }
                    break;
                case 'H': /* backspace */
                    if (edit_mode) {
                        keyBackspace();
                    }
                    else {
                        keyLeft();
                    }
                    break;
                case 'R': /* troggle view and edit mode */
                    if (edit_mode) {
                        redraw(RD_STMSG | SM_IMMEDIATE, &view_mode_msg);
                    }
                    else {
                        redraw(RD_STMSG | SM_IMMEDIATE, &edit_mode_msg);
                    }
                    edit_mode = !edit_mode;
                    break;
                default: /* Clear status message bar */
                    drawStatusMsgImm(NULL);
                    break;
            }
        }
    }
}

/* the front-end function using curses */
extern "C" void frontEndCurses(textOp &file) {
    editing_file = &file;
    initCurses();
    readCacheFile();
    control();
    writeCacheFile();
}
