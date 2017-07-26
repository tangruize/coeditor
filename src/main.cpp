/*************************************************************************
	> File Name: main.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creatation Time: 2017-07-24 17:48
 ************************************************************************/

#include "text.h"
#include "error.h"
#include "front_end.h"

int main(int argc, char *argv[]) {
    program_name = argv[0];
    if (argc < 2) {
        errExit("main: Too few arguments");
    }
    textOp file;
    auto msg = file.loadFile(argv[1]);
    EXIT_ERROR(msg);
    frontEnd(file);
    return 0;
}
