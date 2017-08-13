/*************************************************************************
	> File Name: text_mutex.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-08-02 10:04
	> Description: 
 ************************************************************************/

#include "common.h"
#include "text_mutex.h"
#include <pthread.h>

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

prompt_t textOpMutex::deleteChar(pos_t pos, char *c) {
    //cerr << "mutex del" << endl;
    int s = pthread_mutex_lock(&mtx);
    if (s != 0) {
        PROMPT_ERROR_EN_S("deleteChar: lock mutex", s);
    }
    prompt_t msg = textOp::deleteChar(pos, c);
    s = pthread_mutex_unlock(&mtx);
    if (s != 0) {
        PROMPT_ERROR_EN_S("deleteChar: unlock mutex", s);
    }
    return msg;
}

prompt_t textOpMutex::insertChar(pos_t pos, char c) {
    //cerr << "mutex ins" << endl;
    int s = pthread_mutex_lock(&mtx);
    if (s != 0) {
        PROMPT_ERROR_EN_S("insertChar: lock mutex", s);
    }
    prompt_t msg = textOp::insertChar(pos, c);
    s = pthread_mutex_unlock(&mtx);
    if (s != 0) {
        PROMPT_ERROR_EN_S("insertChar: unlock mutex", s);
    }
    return msg;
}

prompt_t textOpMutex::deleteCharAt(uint64_t off, char *c, pos_t *p) {
    int s = pthread_mutex_lock(&mtx);
    if (s != 0) {
        PROMPT_ERROR_EN_S("deleteCharAt: lock mutex", s);
    }
    prompt_t msg = textOp::deleteCharAt(off, c, p);
    s = pthread_mutex_unlock(&mtx);
    if (s != 0) {
        PROMPT_ERROR_EN_S("deleteCharAt: unlock mutex", s);
    }
    return msg;
}

prompt_t textOpMutex::insertCharAt(uint64_t off, char c, pos_t *p) {
    int s = pthread_mutex_lock(&mtx);
    if (s != 0) {
        PROMPT_ERROR_EN_S("insertCharAt: lock mutex", s);
    }
    prompt_t msg = textOp::insertCharAt(off, c, p);
    s = pthread_mutex_unlock(&mtx);
    if (s != 0) {
        PROMPT_ERROR_EN_S("insertCharAt: unlock mutex", s);
    }
    return msg;
}

file_t::iterator textOpMutex::locateLine(int no) {
    int s = pthread_mutex_lock(&mtx);
    if (s != 0) {
        PROMPT_ERROR_EN_S("locateLine: lock mutex", s);
    }
    file_t::iterator it = textOp::locateLine(no);
    s = pthread_mutex_unlock(&mtx);
    if (s != 0) {
        PROMPT_ERROR_EN_S("locateLine: unlock mutex", s);
    }
    return it;
}
