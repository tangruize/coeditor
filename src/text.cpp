/*************************************************************************
	> File Name: text.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-25 21:55
	> Description: 
 ************************************************************************/

#include "text.h"

/* init the editing file */
prompt_t textOp::loadFile(const string &filename) {
    assert(cerr << "Loading '" << filename << "'...");
    ifstream infile(filename.c_str(), ios::in);
    if (!infile.is_open()) {
        assert(cerr << "failed\n");
        return "loadFile: Cannot open file: " + filename;
    }
    linestruct tmp_node;
    total_lines = 0;
    file_name = filename;
    edit_file.clear();
    while (getline(infile, tmp_node.line)) {
        edit_file.push_back(tmp_node);
        ++total_lines;
    }
    infile.close();
    modified = false;
    /* empty file, push a dummy line */
    if (total_lines == 0) {
        edit_file.push_back(tmp_node);
        ++total_lines;
    }
    cur_line_no = 1;
    cur_line_it = edit_file.begin();
    assert(cerr << "done\n");
    return NOERR;
}

/* print buf for debug */
void textOp::printLines(int start, int count, bool lineno) {
    assert(cerr << "Printing line " << start << " (+" 
                << (unsigned)count << ")\n---PRINT START---\n");
    if (start <= 0) {
        errMsg("printLines: start should be greater than 0, given " 
               + to_string(start));
        return;
    }
    else if (start > total_lines) {
        errMsg("printLines: start should be less than " 
               + to_string(total_lines) + ", given " 
               + to_string(start));
        return;
    }
    auto it = edit_file.begin();
    for (int i = 1; i < start; ++i){
        ++it;
    }
    for (; count && it != edit_file.end(); --count, ++it) {
        if (lineno) {
            cout << start++ << ' ';
        }
        cout << it->line << endl;
    }
    assert(cerr << "---PRINT END---\n");
}

/* locate at a given line */
file_t::iterator textOp::locateLine(int no) {
    assert(cerr << "└─Locating line " << no << '\n');
    if (no < 1 || no > total_lines) {
        return edit_file.end();
    }
    int distance = no - cur_line_no;
    cur_line_no = no;
    /* user's editing is probably continuous */
    switch (distance) {
        case -1:
            return --cur_line_it;
        case 0:
            return cur_line_it;
        case 1:
            return ++cur_line_it;
        default:
            break;
    }
    if (abs(distance) <= no) {
        if (distance < 0) {
            while (distance++) {
                --cur_line_it;
            }
        }
        else {
            while (distance--) {
                ++cur_line_it;
            }
        }
    }
    else {
        cur_line_it = edit_file.begin();
        while (--no) {
            ++cur_line_it;
        }
    }
    return cur_line_it;
}

/* delete a character, if offset is 0, will join a line */
prompt_t textOp::deleteChar(pos_t pos) {
    assert(cerr << "Deleting (" << pos.lineno << ", " 
                << pos.offset << ")\n");
    auto it = locateLine(pos.lineno);
    if (it == edit_file.end() || it->line.size() < pos.offset
        || pos.offset < 0)
    {
        return "deleteChar: position out of range (" 
               + to_string(pos.lineno) + ", " 
               + to_string(pos.offset) + ")";
    }
    if (pos.offset == 0) {
        if (it == edit_file.begin()) {
            return "deleteChar: position out of range (" 
                    + to_string(pos.lineno) + ", "
                    + to_string(pos.offset) + ")";
        }
        auto to_join = it;
        --it;
        cur_line_it = it;
        --cur_line_no;
        it->line += to_join->line;
        edit_file.erase(to_join);
        --total_lines;
    }
    else {
        it->line.erase(it->line.begin() + (pos.offset - 1));
    }
    modified = true;
    return NOERR;
}

/* insert a character, if it is a newline, will add a line */
prompt_t textOp::insertChar(pos_t pos, char c) {
    #ifndef NDEBUG
    cerr << "Inserting '";
    if (isprint(c)) {
        cerr << c;
    }
    else {
        cerr << "\\x" << hex << (unsigned)c;
        cerr.unsetf(ios::hex);
    }
    cerr << "' into (" << pos.lineno << ", " << pos.offset << ")\n";
    #endif
    auto it = locateLine(pos.lineno);
    if (it == edit_file.end() || it->line.size() + 1 < pos.offset
        || pos.offset < 1)
    {
        return "insertChar: position out of range (" 
               + to_string(pos.lineno) + ", " 
               + to_string(pos.offset) + ")";
    }
    if (c == '\n') {
        if (pos.offset == 0) {
            linestruct pre_blank_line;
            edit_file.insert(it, pre_blank_line);
        }
        else if (pos.offset == it->line.size()) {
            linestruct next_blank_line;
            edit_file.insert(++it, next_blank_line);
        }
        else {
            linestruct next_line;
            next_line.line = it->line.substr(pos.offset - 1);
            it->line.resize(pos.offset - 1);
            edit_file.insert(++it, next_line);
        }
        ++total_lines;
        ++cur_line_no;
        cur_line_it = it;
    }
    else {
        char tmpbuf[2] = {c, 0};
        it->line.insert(pos.offset - 1, tmpbuf);
    }
    modified = true;
    return NOERR;
}

/* save file */
prompt_t textOp::saveFile(const string &filename) {
    assert(cerr << "Saving '");
    ofstream outfile;
    if (filename == "") {
        assert(cerr << file_name << "'...");
        if (!modified) {
            assert(cerr << "done\n");
            return NOERR;
        }
        outfile.open(file_name.c_str());
    }
    else {
        assert(cerr << filename << "'...");
        if (filename == file_name && !modified) {
            assert(cerr << "done\n");
            return NOERR;
        }
        outfile.open(filename.c_str());
    }
    if (!outfile.is_open()) {
        return "saveFile: cannot open file: " + filename;
    }
    for (auto &line: edit_file) {
        outfile << line.line << '\n';
    }
    outfile.close();
    if (filename == "" || filename == file_name) {
        modified = false;
    }
    assert(cerr << "done\n");
    return NOERR;
}

extern "C" {

/* init the editing file */
prompt_t loadFile(textOp &file, const string &filename) {
    return file.loadFile(filename);
}

/* print buf for debug */
void printLines(textOp &file, int start, int count) {
    file.printLines(start, count);
}

/* delete a character, if offset is 0, will join a line */
prompt_t deleteChar(textOp &file, pos_t pos) {
    return file.deleteChar(pos);
}

/* insert a character, if it is a newline, will add a line */
prompt_t insertChar(textOp &file, pos_t pos, char c) {
    return file.insertChar(pos, c);
}

/* save file */
prompt_t saveToFile(textOp &file, const string &filename) {
    return file.saveFile(filename);
}

prompt_t saveCurFile(textOp &file) {
    return file.saveFile();
}

/* get file name */
string getFilename(textOp &file) {
    return file.getFilename();
}

}
