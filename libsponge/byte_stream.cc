#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : cap(capacity), left_size(0), data_left(), _eoi(false), read_bytes(0), write_bytes(0) {}

size_t ByteStream::write(const string &data) {
    size_t now = 0;
    size_t tmp = min(data.size() - now, cap - left_size);
    data_left.append(data.begin() + now, data.begin() + now + tmp);
    now += tmp;
    left_size += tmp;

    write_bytes += now;
    return now;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string ans;
    if (left_size > len) {
        ans = data_left.substr(0, len);
    } else {
        ans = data_left;
    }
    return ans;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (left_size > len) {
        read_bytes += len;
        data_left.erase(data_left.begin(), data_left.begin() + len);
    } else {
        read_bytes += left_size;
        data_left.clear();
    }
    left_size = data_left.size();
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string ans = peek_output(len);
    pop_output(len);

    return ans;
}

void ByteStream::end_input() { _eoi = true; }

bool ByteStream::input_ended() const { return _eoi; }

size_t ByteStream::buffer_size() const { return left_size; }

bool ByteStream::buffer_empty() const { return !left_size; }

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return write_bytes; }

size_t ByteStream::bytes_read() const { return read_bytes; }

size_t ByteStream::remaining_capacity() const { return cap - left_size; }
