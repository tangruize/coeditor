/* write an OTed op to editing file */
void writeLocal(const op_t &op) {
    op_t q_op = op;
    int flags = TM_NOLOCK;
    string op_result;
    edit_file->lock();
    switch (op.operation) {
        case DELETE:
            op_result = edit_file->deleteChar(op.pos,
                                   &op.data, NULL, &flags);
            break;
        case CH_DELETE:
            op_result = edit_file->deleteCharAt(op.char_offset, 
                                   &op.data, &q_op.pos, &flags);
            q_op.operation = DELETE;
            break;
        case INSERT:
            op_result = edit_file->insertChar(op.pos,
                                   op.data, NULL, &flags);
            break;
        case CH_INSERT:
            op_result = edit_file->insertCharAt(op.char_offset,
                                   op.data, &q_op.pos, &flags);
            q_op.operation = INSERT;
            break;
        default:
            op_result = "FAILED";
    }
    edit_file->unlock();
    if (op_result != NOERR) {
        PROMPT_ERROR(op_result);
    }
    else {
        pos_to_transform->push(q_op);
        buf_changed = 1;
    }
}

/* write a message to server */
void writeServer(const trans_t &msg) {
    if (write(socket_fd, &msg, sizeof(msg)) != sizeof(msg)) {
        PROMPT_ERROR_EN("write server");
    }
}
