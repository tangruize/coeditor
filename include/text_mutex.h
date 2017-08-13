/*************************************************************************
	> File Name: text_mutex.h
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-08-02 10:32
	> Description: 
 ************************************************************************/

#ifndef _TEXT_MUTEX_H
#define _TEXT_MUTEX_H

#include "text.h"

class textOpMutex: public textOp
{
public:
    virtual ~textOpMutex() {}

    /* delete a character, if offset is 0, will join a line */
    virtual prompt_t deleteChar(pos_t pos, char *c = NULL);

    /* insert a character, if it is a newline, will add a line */
    virtual prompt_t insertChar(pos_t pos, char c);

    /* delete a character at given file offset */
    virtual prompt_t deleteCharAt(uint64_t off, char *c = NULL,
                                  pos_t *p = NULL);

    /* insert a character at given file offset */
    virtual prompt_t insertCharAt(uint64_t off, char c,
                                  pos_t *p = NULL);

    /* locate at a given line */
    virtual file_t::iterator locateLine(int no);
};

#endif
