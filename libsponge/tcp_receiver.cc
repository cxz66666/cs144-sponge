#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (seg.header().syn) {
        isn = seg.header().seqno + 1;
        setIsn = true;
    }
    //不用理会没有建立的数据包
    if (!setIsn) {
        return;
    }
    //如果这个包是syn的包，则需要使用当前的isn作为unwrap的输入，否则直接使用header中的字段即可
    _reassembler.push_substring(seg.payload().copy(),
                                unwrap(seg.header().syn ? isn : seg.header().seqno, isn, stream_out().bytes_written()),
                                seg.header().fin);
    //如果接受过Fin同时接收达到了Fin的位置，则选择关闭
    if (stream_out().input_ended()) {
        setFin = true;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!setIsn) {
        return {};
    }
    return {wrap(stream_out().bytes_written() + (setFin ? 1 : 0), isn)};
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
