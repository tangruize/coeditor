/*************************************************************************
	> File Name: main.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creatation Time: 2017-07-24 17:48
 ************************************************************************/

#include "text_mutex.h"
#include "error.h"
#include "init.h"
#include "front_end.h"
#include "common.h"

#include <unistd.h>

string server_addr;

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "c:pw")) != -1) {
        switch (opt) {
            case 'c':
                server_addr = optarg;
                break;
            case 'p':
                write_op_pos = 1;
                break;
            case 'w':
                write_op = 1;
                break;
            default:
                cerr << "Invalid arguments" << endl;
                exit(EXIT_FAILURE);
        }
    }
    textOpMutex file;
    edit_file = &file;
    if (optind < argc) {
//        auto msg = file.loadFile(argv[optind]);
//        EXIT_ERROR(msg);
        file.setFilename(argv[optind]);
    }
    init();
    frontEnd(file);
    return 0;
}
