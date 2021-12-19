#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), nodes(0), size(0), current_begin(0), eof_pos(0) {}

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
    // data_index_begin是已经accept的部分的长度
    size_t accepted_size = 0;
    if (index < current_begin) {
        accepted_size = current_begin - index;
    }
    //三个里面的最小值， 可以容纳的大小，剩余字符串长度，以及在不超过cap的情况下最长可以放的字符串长度
    // target_size实际上是目标字符串的长度
    size_t target_size = min(_output.remaining_capacity() - size, static_cast<size_t>(data.size()) - accepted_size);
    target_size = min(target_size, _capacity + _output.bytes_read() - index - accepted_size);
    if (!target_size) {
        return;
    }
    PushAndCombine(index + accepted_size, data.substr(accepted_size, target_size));
}
void StreamReassembler::PushAndCombine(const size_t index, const string &data) {
    size_t end = static_cast<size_t>(index + data.size());
    nodes.push_back(ReassemblerNode(index, end, data));
    size += data.size();
    sort(nodes.begin(), nodes.end());
    //合并并且调整size
    for (auto m = nodes.begin(); m != nodes.end();) {
        if (m == nodes.begin()) {
            m++;
            continue;
        }
        auto pre = m - 1;
        if (m->begin > pre->end) {
            m++;
        } else {
            if (m->end <= pre->end) {
                size -= m->end - m->begin;
                m = nodes.erase(m);
            } else {
                size -= pre->end - m->begin;
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
        size -= nodes[0].end - nodes[0].begin;
        nodes.erase(nodes.begin());
    }
    return;
}

size_t StreamReassembler::unassembled_bytes() const { return size; }

bool StreamReassembler::empty() const { return !size; }
