/*************************************************************************
	> File Name: text.cpp
	> Author: Tang Ruize
	> Mail: 151220100@smail.nju.edu.cn
	> Creation Time: 2017-07-25 21:55
	> Description: 
 ************************************************************************/

#include "text.h"
//#define NDEBUG

textOp::textOp(const string &filename) {
    if (filename.size()) {
        prompt_t msg = loadFile(filename);
        if (msg != NOERR) {
            throw msg;
        }
    }
    else {
        modified = false;
        linestruct tmp_node;
        edit_file.push_back(tmp_node);
        cur_line_no = 1;
        cur_line_it = edit_file.begin();
        total_chars = 0;
        cur_char = 0;
        has_break_line = true;
    }
}

/* init the editing file */
prompt_t textOp::loadFile(const string &filename) {
//    assert(cerr << "Loading '" << filename << "'...");
    linestruct tmp_node;
    ifstream infile(filename.c_str(), ios::in);
    if (!infile.is_open()) {
//        assert(cerr << "failed\n");
//        return "loadFile: Cannot open file: " + filename;
        if (file_name == "") {
            /* new file */
            edit_file.clear();
            modified = false;
            file_name = filename;
            edit_file.push_back(tmp_node);
            cur_line_no = 1;
            cur_line_it = edit_file.begin();
            total_chars = 0;
            cur_char = 0;
            has_break_line = true;
            return NOERR;
        }
        else {
            return "loadFile: Cannot open file: " + filename;
        }
    }
    file_name = filename;
    edit_file.clear();
    cur_char = 0;
    modified = false;
    has_break_line = true;
    int string_total_chars = 0;
    while (getline(infile, tmp_node.line)) {
        edit_file.push_back(tmp_node);
        /* if the last line does not contain '\n' (break line),
         * string_total_chars == total_chars + 1
         */
        string_total_chars += tmp_node.line.size() + 1;
    }
    /* clear fail bits to call tellg() */
    infile.clear();
    total_chars = infile.tellg();
    infile.close();
    /* file is empty or has no break line, push a dummy line */
    if (total_chars == string_total_chars) {
        tmp_node.line.clear();
        edit_file.push_back(tmp_node);
        if (total_chars != 0) {
            has_break_line = false;
        }
    }
    cur_line_no = 1;
    cur_line_it = edit_file.begin();
//    assert(cerr << "done\n");
    return NOERR;
}

/* print buf for debug */
void textOp::printLines(int start, int count, bool lineno) {
    assert(cerr << "Printing line " << start << " (+" 
                << (unsigned)count << ")\n");
    if (start <= 0) {
        errMsg("printLines: start should be greater than 0, given " 
               + to_string(start));
        return;
    }
    else if (start > getTotalLines()) {
        errMsg("printLines: start should be no greater than " 
               + to_string(getTotalLines()) + ", given " 
               + to_string(start));
        return;
    }
    file_t::iterator it = edit_file.begin();
    for (int i = 1; i < start; ++i){
        ++it;
    }
    assert(cerr << "---PRINT START---\n");
    for (; count && it != edit_file.end(); --count, ++it) {
        if (lineno) {
            cerr << start++ << ' ';
        }
        cerr << it->line << endl;
    }
    assert(cerr << "---PRINT END---\n");
    cerr << "\n";
}

/* locate at a given line */
file_t::iterator textOp::locateLine(int no) {
    //assert(cerr << "Locating " << no << endl);
    if (no == cur_line_no) {
        return cur_line_it;
    }
    if (no < 1 || no > edit_file.size()) {
        return edit_file.end();
    }
    int distance = no - cur_line_no;
    cur_line_no = no;
    /* user's editing is probably continuous */
    switch (distance) {
        case -1:
            --cur_line_it;
            cur_char -= cur_line_it->line.size() + 1;
            return cur_line_it;
        case 1:
            cur_char += cur_line_it->line.size() + 1;
            return ++cur_line_it;
        default:
            break;
    }
    if ((int)edit_file.size() - no < distance) {
        /* locate from end */
        distance = no - edit_file.size();
        cur_line_it = edit_file.end();
        --cur_line_it;
        cur_char = total_chars - cur_line_it->line.size();
    }
    if (abs(distance) <= no) {
        if (distance <= 0) {
            while (distance++) {
                --cur_line_it;
                cur_char -= cur_line_it->line.size() + 1;
            }
        }
        else {
            while (distance--) {
                cur_char += cur_line_it->line.size() + 1;
                ++cur_line_it;
            }
        }
    }
    else {
        /* locate from beginning */
        cur_line_it = edit_file.begin();
        cur_char = 0;
        while (--no) {
            cur_char += cur_line_it->line.size() + 1;
            ++cur_line_it;
        }
    }
    return cur_line_it;
}

/* translate (row, col) to file offset */
uint64_t textOp::translatePos(const pos_t pos) {
    assert(cerr << "tanslating postion (" << pos.lineno << ", " <<
           pos.offset << ") ... ");
    file_t::iterator it = textOp::locateLine(pos.lineno);
    if (it == edit_file.end() || pos.offset < 0
        || pos.offset > it->line.size() + 1)
    {
        assert(cerr << "failed\n\n");
        return (uint64_t)-1;
    }
    assert(cerr << "result: " << (int64_t)(cur_char + pos.offset - 1) << "\n\n");
    return cur_char + pos.offset - 1;
}

/* translate file offset to (row, col) */
pos_t textOp::translateOffset(uint64_t offset) {
    assert(cerr << "tanslating offset " << offset << " ... ");
    pos_t result = {0, 0};
    if (offset > total_chars) {
        assert(cerr << "failed\n\n");
        return result;
    }
    if ((int64_t)(offset - cur_char) > (int64_t)(total_chars - offset)) {
        textOp::locateLine(edit_file.size());
    }
    else if ((int64_t)(cur_char - offset) > (int64_t)offset) {
        textOp::locateLine(1);
    }
    int64_t distance = (int64_t)(offset - cur_char);
    file_t::iterator it;
    if (distance <= 0) {
        for (; cur_line_no > 0 && distance < 0;
                distance = (int64_t)(offset - cur_char))
        {
            it = textOp::locateLine(cur_line_no - 1);
        }
    }
    else {
        int sz = edit_file.size(), locate_lineno = cur_line_no;
        do {
            it = textOp::locateLine(locate_lineno++);
        } while (cur_char + it->line.size() < offset
                 && cur_line_no < sz);
    }
    result.lineno = cur_line_no;
    result.offset = (int)(offset - cur_char) + 1;
    assert(cerr << "result: (" << result.lineno << ", "
           << result.offset << ")\n\n");
    return result;
}

/* delete a character, if offset is 0, will join a line */
prompt_t
textOp::deleteChar(pos_t pos, char *c, uint64_t *off, int *flags) {
    if (off) {
        *off = textOp::translatePos(pos);
        if (*off == (uint64_t)-1 || *off == total_chars) {
            return "deleteChar: position out of range (" 
                   + to_string(pos.lineno) + ", " 
                   + to_string(pos.offset) + ")";
        }
    }
    assert(cerr << "Deleting (" << pos.lineno << ", " 
                << pos.offset << ")\n");
    assert(cerr << "---Locating line " << pos.lineno << '\n');
    file_t::iterator it = textOp::locateLine(pos.lineno);
    if (!off) {
        if (it == edit_file.end() || it->line.size() + 1 < pos.offset
            || pos.offset < 1
            || (pos.lineno == edit_file.size()
            && it->line.size() + 1 == pos.offset))
        {
            return "deleteChar: position out of range (" 
                   + to_string(pos.lineno) + ", " 
                   + to_string(pos.offset) + ")";
        }
    }
    char delete_char;
    --total_chars;
    assert(cerr << "---delete char: '");
    if (pos.offset == it->line.size() + 1) {
        delete_char = '\n';
        assert(cerr << "\\n'\n" << "---before del: "
               << it->line << endl);
        file_t::iterator to_join = it;
        ++to_join;
        it->line += to_join->line;
        edit_file.erase(to_join);
    }
    else {
        delete_char = it->line[pos.offset - 1];
        #ifndef NDEBUG
        if (isprint(delete_char)) {
            cerr << delete_char;
        }
        else {
            cerr << "0x" << hex << uppercase << (unsigned)delete_char;
            cerr.unsetf(ios_base::hex);
        }
        #endif
        assert(cerr << "'\n" << "---before del: " << it->line << endl);
        it->line.erase(it->line.begin() + (pos.offset - 1));
    }
    if (c) {
        *c = delete_char;
    }
    modified = true;
    assert(cerr << "---after  del: " << it->line << endl);
    assert(cerr << "---lineno: " << cur_line_no << "/"
                << edit_file.size() << ", char: "
                << cur_char << "/" << total_chars << endl);
    assert(cerr << "\n");
    return NOERR;
}

/* insert a character, if it is a newline, will add a line */
prompt_t
textOp::insertChar(pos_t pos, char c, uint64_t *off, int *flags) {
    if (off) {
        *off = textOp::translatePos(pos);
        if (*off == (uint64_t)-1) {
            return "insertChar: position out of range (" 
                   + to_string(pos.lineno) + ", " 
                   + to_string(pos.offset) + ")";
        }
    }
    #ifndef NDEBUG
    cerr << "Inserting '";
    if (isprint(c)) {
        cerr << c;
    }
    else {
        cerr << "0x" << hex << uppercase << (unsigned)c;
        cerr.unsetf(ios_base::hex);
    }
    cerr << "' into (" << pos.lineno << ", " << pos.offset << ")\n";
    #endif
    assert(cerr << "---Locating line " << pos.lineno << '\n');
    file_t::iterator it = textOp::locateLine(pos.lineno);
    if (!off) {
        if (it == edit_file.end() || it->line.size() + 1 < pos.offset
            || pos.offset < 1)
        {
            return "insertChar: position out of range (" 
                   + to_string(pos.lineno) + ", " 
                   + to_string(pos.offset) + ")";
        }
    }
    assert(cerr << "---before ins: " << it->line << endl);
    if (c == '\n') {
        if (pos.offset == 0) {
            linestruct pre_blank_line;
            edit_file.insert(it, pre_blank_line);
            ++cur_char;
        }
        else if (pos.offset == it->line.size() + 1) {
            linestruct next_blank_line;
            cur_char += it->line.size() + 1;
            edit_file.insert(++it, next_blank_line);
            --it;
        }
        else {
            linestruct next_line;
            next_line.line = it->line.substr(pos.offset - 1);
            it->line.resize(pos.offset - 1);
            cur_char += it->line.size() + 1;
            edit_file.insert(++it, next_line);
            --it;
        }
        ++cur_line_no;
        cur_line_it = it;
    }
    else {
        char tmpbuf[2] = {c, 0};
        it->line.insert(pos.offset - 1, tmpbuf);
    }
    modified = true;
    ++total_chars;
    assert(cerr << "---after  ins: " << it->line << endl);
    assert(cerr << "---lineno: " << cur_line_no << "/"
                << edit_file.size() << ", char: "
                << cur_char << "/" << total_chars << endl);
    assert(cerr << "\n");
    return NOERR;
}

/* delete a character at given file offset */
prompt_t
textOp::deleteCharAt(uint64_t off, char *c, pos_t *p, int *flags) {
    pos_t pos = textOp::translateOffset(off);
    if (pos.lineno < 1) {
        return "deleteCharAt: offset out of range: " + to_string(off);
    }
    if (p) {
        *p = pos;
    }
    return textOp::deleteChar(pos, c);
}

/* insert a character at given file offset */
prompt_t
textOp::insertCharAt(uint64_t off, char c, pos_t *p, int *flags) {
    pos_t pos = textOp::translateOffset(off);
    if (pos.lineno < 1) {
        return "insertCharAt: offset out of range: " + to_string(off);
    }
    if (p) {
        *p = pos;
    }
    return textOp::insertChar(pos, c);
}

/* save file */
prompt_t textOp::saveFile(const string &filename, 
        ios_base::openmode mode)
{
    assert(cerr << "Saving '");
    ofstream outfile;
    
    if (filename == "") {
        assert(cerr << file_name << "' ... ");
        if (!modified) {
            assert(cerr << "unchanged\n\n");
            return NOERR;
        }
        outfile.open(file_name.c_str(), mode);
    }
    else {
        assert(cerr << filename << "' ... ");
        if (filename == file_name && !modified) {
            assert(cerr << "ignored\n\n");
            return NOERR;
        }
        outfile.open(filename.c_str(), mode);
    }
    if (!outfile.is_open()) {
        assert(cerr << "failed\n\n");
        return "Error saving " + filename + ": cannot open file";
    }
    int i = 0, sz = (int)edit_file.size() - 1;
    file_t::iterator it = edit_file.begin();
    for (; i < sz; ++i, ++it) {
        outfile << it->line << '\n';
    }
    /* the last line may not contain '\n' */
    outfile << it->line << flush;
    outfile.close();
    if ((filename == "" || filename == file_name)
        && mode == ios_base::out)
    {
        modified = false;
    }
    assert(cerr << "done\n\n");
    return NOERR;
}
