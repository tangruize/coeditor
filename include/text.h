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
    textOp(const string &filename = "") {
        if (filename.size()) {
            auto msg = loadFile(filename);
            if (msg != NOERR) {
                throw msg;
            }
        }
        modified = false;
    }

    /* init the editing file */
    prompt_t loadFile(const string &filename);

    /* print buf for debug */
    void printLines(int start = 1, int count = -1, bool lineno = true);

    /* delete a character, if offset is 0, will join a line */
    prompt_t deleteChar(pos_t pos);

    /* insert a character, if it is a newline, will add a line */
    prompt_t insertChar(pos_t pos, char c);

    /* save file */
    prompt_t saveFile(const string &filename = "");

private:
    /* the editing file */
    file_t edit_file;

    /* file name for saving */
    string file_name;

    /* total lines of the editing file */
    int total_lines;

    /* is file modified? */
    bool modified;

    /* current line iterator and line num */
    int cur_line_no;
    file_t::iterator cur_line_it;

    /* locate at a given line */
    file_t::iterator locateLine(int no);
};

#endif
