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

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "c:t:T:a:pwdl")) != -1) {
        switch (opt) {
            case 'c':
                server_addr = optarg;
                break;
            case 'p':
                write_op_pos = 1;
                break;
            case 'w':
                write_op = 0;
                break;
            case 't':
                ot_time_arg = optarg;
                break;
            case 'T':
                ot_recv_time_arg = optarg;
                break;
            case 'd':
                no_debug = false;
                break;
            case 'l':
                no_cli = 0;  
                break;
            case 'a':
                if (strcmp(optarg, "cscw") == 0)
                    program_id = 1;
                else if (strcmp(optarg, "css") == 0)
                    program_id = 2;
                break;
            default:
                cerr << "Invalid arguments" << endl;
                exit(EXIT_FAILURE);
        }
    }
    textOpMutex file;
//    textOp file;
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
