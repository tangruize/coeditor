/*************************************************************************
	> File Name: main.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creatation Time: 2017-07-24 17:48
 ************************************************************************/

#include "text.h"
#include "error.h"
#include "init.h"
#include "front_end.h"

int main(int argc, char *argv[]) {
    textOp file;
    edit_file = &file;
    if (argc > 1) {
        auto msg = file.loadFile(argv[1]);
    }
    init();
    frontEnd(file);
    return 0;
}
