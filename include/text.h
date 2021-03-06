/*************************************************************************
	> File Name: text.h
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-25 21:54
	> Description: 
 ************************************************************************/

#ifndef _TEXT_H
#define _TEXT_H

#include "common.h"
#include "error.h"
#include "op.h"

/* save each line and might add other members */
struct linestruct {
    string line;
};

/* using list to save all lines of file */
typedef list<linestruct> file_t;

class textOp {
public:
    textOp(const string &filename = "");

    /* deleting object of polymorphic class type ‘textOp’
     * which has non-virtual destructor might cause undefined behaviour
     * [-Wdelete-non-virtual-dtor] */
    /* so we define one, which does nothing */
    virtual ~textOp() {}

    /* init the editing file */
    prompt_t loadFile(const string &filename);

    /* locate at a given line */
    virtual file_t::iterator locateLine(int no);

    /* print buf for debug */
    void printLines(int start = 1, int count = -1, bool lineno = true);

    /* delete a character, if offset is 0, will join a line */
    virtual prompt_t deleteChar(pos_t pos, char *c = NULL,
                                uint64_t *off = NULL, int *flags = NULL);

    /* insert a character, if it is a newline, will add a line */
    virtual prompt_t insertChar(pos_t pos, char c,
                                uint64_t *off = NULL, int *flags = NULL);

    /* delete a character at given file offset */
    virtual prompt_t deleteCharAt(uint64_t off, char *c = NULL,
                                  pos_t *p = NULL, int *flags = NULL);

    /* insert a character at given file offset */
    virtual prompt_t insertCharAt(uint64_t off, char c,
                                  pos_t *p = NULL, int *flags = NULL);

    /* save file */
    prompt_t saveFile(const string &filename = "",
                ios_base::openmode mode = ios::out | ios::trunc);

    /* translate (row, col) to file offset */
    virtual uint64_t translatePos(const pos_t pos);

    /* translate file offset to (row, col) */
    virtual pos_t translateOffset(uint64_t offset);

    /* lock buf to prevent editing */
    virtual void lock() {
        return;
    }

    virtual void unlock() {
        return;
    }

    /* meaningless in base class */
    // virtual bool isLocked() const {
    //     return false;
    // }

    /* get file name */
    string getFilename() const {
        return file_name;
    }

    /* get total lines */
    int getTotalLines() const {
        return edit_file.size();
    }

    /* get file size */
    int getTotalChars() const {
        return total_chars;
    }

    /* file is modified? */
    bool isModified() const {
        return modified;
    }

    /* set file name */
    void setFilename(const string &filename) {
        file_name = filename;
    }

    /* clear modified state */
    void clearModified() {
        modified = false;
    }

    /* set modified state */
    void setModified() {
        modified = true;
    }

    /* if file has no break line, 
     * getTotalLines() == real total lines - 1
     * Note that it is meaningless if modified
     */
    bool hasBreakLine() const {
        return has_break_line;
    }

private:
    /* the editing file */
    file_t edit_file;

    /* file name for saving */
    string file_name;

    /* is file modified? */
    bool modified;

    /* is the last line a break line? */
    bool has_break_line;

    /* current line iterator, line num and current line start offset */
    file_t::iterator cur_line_it;
    int cur_line_no;
    uint64_t cur_char;

    /* total characters */
    uint64_t total_chars;
};

#endif
