#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), nodes(0), unassembled_size(0), current_begin(0), eof_pos(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        eof_pos = max(eof_pos, static_cast<size_t>(data.size() + index));
        if (current_begin == eof_pos) {
            _output.end_input();
        }
    }
    //如果已经接受了
    size_t end = static_cast<size_t>(index + data.size());
    if (end <= current_begin) {
        return;
    }
    // accepted_size是已经accept的部分的长度,就是当前accept的index已经超过了该部分
    size_t accepted_size = 0;
    if (index < current_begin) {
        accepted_size = current_begin - index;
    }

    //思考了一下，感觉是两个里面的最小值，接收窗口-开始的index，以及还未接受的剩余字符串的长度
    size_t target_size =
        min(static_cast<size_t>(data.size()) - accepted_size, _capacity + _output.bytes_read() - index - accepted_size);

    //刚开始我认为是三个里面的最小值，
    //可以容纳的大小，剩余字符串长度，以及在不超过cap的情况下最长可以放的字符串长度，但是仔细想想还是上面的靠谱
    // target_size实际上是目标字符串的长度
    //    size_t target_size = min(_output.remaining_capacity() - unassembled_size,
    //    static_cast<size_t>(data.unassembled_size()) - accepted_size); target_size = min(target_size, _capacity +
    //    _output.bytes_read() - index - accepted_size);

    if (!target_size) {
        return;
    }
    PushAndCombine(index + accepted_size, data.substr(accepted_size, target_size));
}
void StreamReassembler::PushAndCombine(const size_t index, const string &data) {
    size_t end = static_cast<size_t>(index + data.size());
    unassembled_size += data.size();
    ReassemblerNode new_node{index, end, data};
    //插入排序
    nodes.insert(upper_bound(nodes.begin(), nodes.end(), new_node), new_node);

    //合并并且调整size
    for (auto m = nodes.begin() + 1; m != nodes.end();) {
        auto pre = m - 1;
        if (m->begin > pre->end) {
            m++;
        } else {
            if (m->end <= pre->end) {
                unassembled_size -= m->end - m->begin;
                m = nodes.erase(m);
            } else {
                unassembled_size -= pre->end - m->begin;
                pre->data = pre->data.substr(0, m->begin - pre->begin) + m->data;
                pre->end = m->end;
                m = nodes.erase(m);
            }
        }
    }
    if (nodes[0].begin == current_begin) {
        current_begin += nodes[0].end - nodes[0].begin;
        _output.write(nodes[0].data);
        if (nodes[0].end == eof_pos) {
            _output.end_input();
        }
        unassembled_size -= nodes[0].end - nodes[0].begin;
        nodes.erase(nodes.begin());
    }
}

size_t StreamReassembler::unassembled_bytes() const { return unassembled_size; }

bool StreamReassembler::empty() const { return !unassembled_size; }
