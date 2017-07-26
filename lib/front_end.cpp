/*************************************************************************
	> File Name: front_end.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-26 08:33
	> Description: 
 ************************************************************************/


/* make shared library excutable, need C linkage, 
 * interpreter, and no return entry
 */
extern "C" {

#include <unistd.h>
#include <string.h>

#ifdef __x86_64__
#define INTERP "/lib64/ld-linux-x86-64.so.2"
#else
#define INTERP "/lib/ld-linux.so.2"
#endif

static const char interp[] __attribute__((section(".interp"))) = INTERP;

extern const char *front_end_version;
extern const char *front_end_author;

void printFrontEndBanner() {
    const char *banner[] = {
        "Welcome to use Co-editor! ",
        "The verson you are now using is ", front_end_version, 
        ", by ", front_end_author, ". Have fun!\n",
    };
    for (int i = 0; i  < sizeof(banner) / sizeof(banner[0]); ++i) {
        write(STDOUT_FILENO, banner[i], strlen(banner[i]));
    }
}

/* invoke the shared library will enter this function */
void __attribute__ ((noreturn))
front_end_main(void)
{
    printFrontEndBanner();
    _exit(0);
}

}
