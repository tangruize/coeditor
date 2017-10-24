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
#include <pthread.h>

enum {TM_NOLOCK = 01, TM_TRYLOCK = 02, TM_WROP = 04,
      TM_WROPOFF = 010, TM_AGAIN = 020};
class textOpMutex: public textOp
{
public:
    textOpMutex(const string &filename = "") {
        pthread_mutex_init(&mtx, NULL);
        pthread_mutex_init(&mtx2, NULL);
        // locked = false;
    }

    virtual ~textOpMutex() {
        pthread_mutex_destroy(&mtx);
        pthread_mutex_destroy(&mtx2);
    }

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

    /* locate at a given line */
    virtual file_t::iterator locateLine(int no);

    /* translate (row, col) to file offset */
    virtual uint64_t translatePos(const pos_t pos);

    /* translate file offset to (row, col) */
    virtual pos_t translateOffset(uint64_t offset);

    /* lock buf to prevent from editing */
    virtual void lock() {
        // if (!locked) {
            // locked = true;
        pthread_mutex_lock(&mtx2);
        // }
    }

    virtual void unlock() {
        // if (locked) {
        pthread_mutex_unlock(&mtx2);
            // locked = false;
        // }
    }

    // virtual bool isLocked() const {
    //     return locked;
    // }
    
private:
    pthread_mutex_t mtx;
    pthread_mutex_t mtx2;
    bool locked;
};

#endif
