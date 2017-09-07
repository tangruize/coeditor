/*************************************************************************
	> File Name: text_mutex.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-08-02 10:04
	> Description: 
 ************************************************************************/

#include "common.h"
#include "text_mutex.h"
#include "init.h"

prompt_t
textOpMutex::deleteChar(pos_t pos, char *c, uint64_t *off, int *flags) {
    int f = 0;
    char ch;
    uint64_t offset;
    op_t op;
    if (flags) {
        f = *flags;
    }
    if ((f & TM_WROPOFF) || (f & TM_WROP)) {
        if (!c) {
            c = &ch;
        }
        if ((!off) && (f & TM_WROPOFF)) {
            off = &offset;
        }
    }
    if (f & TM_TRYLOCK) {
        if (pthread_mutex_trylock(&mtx2) != 0) {
            *flags |= TM_AGAIN;
            return NOERR;
        }
    }
    else if (!(f & TM_NOLOCK)) {
        pthread_mutex_lock(&mtx2);
    }
    pthread_mutex_lock(&mtx);
    prompt_t msg = textOp::deleteChar(pos, c, off);
    if (msg == NOERR) {
        if (f & TM_WROPOFF) {
            op.operation = CH_DELETE;
            op.data = *c;
            op.char_offset = *off;
            writeOpFifo(op);
        }
        else if (f & TM_WROP) {
            op.operation = DELETE;
            op.data = *c;
            op.pos = pos;
            writeOpFifo(op);
        }
    }
    pthread_mutex_unlock(&mtx);
    if (!(f & TM_NOLOCK)) {
        pthread_mutex_unlock(&mtx2);
    }
    return msg;
}

prompt_t
textOpMutex::insertChar(pos_t pos, char c, uint64_t *off, int *flags) {
    int f = 0;
    uint64_t offset;
    op_t op;
    if (flags) {
        f = *flags;
    }
    if (f & TM_WROPOFF) {
        if (!off) {
            off = &offset;
        }
    }
    if (f & TM_TRYLOCK) {
        if (pthread_mutex_trylock(&mtx2) != 0) {
            *flags |= TM_AGAIN;
            return NOERR;
        }
    }
    else if (!(f & TM_NOLOCK)) {
        pthread_mutex_lock(&mtx2);
    }
    pthread_mutex_lock(&mtx);
    prompt_t msg = textOp::insertChar(pos, c, off);
    if (msg == NOERR) {
        if (f & TM_WROPOFF) {
            op.operation = CH_INSERT;
            op.data = c;
            op.char_offset = *off;
            writeOpFifo(op);
        }
        else if (f & TM_WROP) {
            op.operation = INSERT;
            op.data = c;
            op.pos = pos;
            writeOpFifo(op);
        }
    }
    pthread_mutex_unlock(&mtx);
    if (!(f & TM_NOLOCK)) {
        pthread_mutex_unlock(&mtx2);
    }
    return msg;
}

prompt_t
textOpMutex::deleteCharAt(uint64_t off, char *c, pos_t *p, int *flags) {
    int f = 0;
    char ch;
    pos_t pos;
    op_t op;
    if (flags) {
        f = *flags;
    }
    if ((f & TM_WROPOFF) || (f & TM_WROP)) {
        if (!c) {
            c = &ch;
        }
        if ((!p) && (f & TM_WROP)) {
            p = & pos;
        }
    }
    if (f & TM_TRYLOCK) {
        if (pthread_mutex_trylock(&mtx2) != 0) {
            *flags |= TM_AGAIN;
            return NOERR;
        }
    }
    else if (!(f & TM_NOLOCK)) {
        pthread_mutex_lock(&mtx2);
    }
    pthread_mutex_lock(&mtx);
    prompt_t msg = textOp::deleteCharAt(off, c, p);
    if (msg == NOERR) {
        if (f & TM_WROPOFF) {
            op.operation = CH_DELETE;
            op.data = *c;
            op.char_offset = off;
            writeOpFifo(op);
        }
        else if (f & TM_WROP) {
            op.operation = DELETE;
            op.data = *c;
            op.pos = *p;
            writeOpFifo(op);
        }
    }
    pthread_mutex_unlock(&mtx);
    if (!(f & TM_NOLOCK)) {
        pthread_mutex_unlock(&mtx2);
    }
    return msg;
}

prompt_t
textOpMutex::insertCharAt(uint64_t off, char c, pos_t *p, int *flags) {
    int f = 0;
    pos_t pos;
    op_t op;
    if (flags) {
        f = *flags;
    }
    if (f & TM_WROP) {
        if (!p) {
            p = &pos;
        }
    }
    if (f & TM_TRYLOCK) {
        if (pthread_mutex_trylock(&mtx2) != 0) {
            *flags |= TM_AGAIN;
            return NOERR;
        }
    }
    else if (!(f & TM_NOLOCK)) {
        pthread_mutex_lock(&mtx2);
    }
    pthread_mutex_lock(&mtx);
    prompt_t msg = textOp::insertCharAt(off, c, p);
    if (msg == NOERR) {
        if (f & TM_WROPOFF) {
            op.operation = CH_INSERT;
            op.data = c;
            op.char_offset = off;
            writeOpFifo(op);
        }
        else if (f & TM_WROP) {
            op.operation = INSERT;
            op.data = c;
            op.pos = *p;
            writeOpFifo(op);
        }
    }
    pthread_mutex_unlock(&mtx);
    if (!(f & TM_NOLOCK)) {
        pthread_mutex_unlock(&mtx2);
    }
    return msg;
}

file_t::iterator textOpMutex::locateLine(int no) {
    pthread_mutex_lock(&mtx);
    file_t::iterator it = textOp::locateLine(no);
    pthread_mutex_unlock(&mtx);
    return it;
}

uint64_t textOpMutex::translatePos(const pos_t pos) {
    pthread_mutex_lock(&mtx);
    uint64_t off = textOp::translatePos(pos);
    pthread_mutex_unlock(&mtx);
    return off;
}

pos_t textOpMutex::translateOffset(uint64_t offset) {
    pthread_mutex_lock(&mtx);
    pos_t pos = textOp::translateOffset(offset);
    pthread_mutex_unlock(&mtx);
    return pos;
}
