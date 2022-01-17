#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    WrappingInt32 seq = seg.header().seqno;
    if (seg.header().syn) {
        isn = seg.header().seqno;
        seq = seq + 1;
        setIsn = true;
    }
    //不用理会没有建立的数据包
    if (!setIsn) {
        return;
    }
    //如果这个包是syn的包，则需要使用当前的isn作为unwrap的输入，否则直接使用header中的字段即可
    //更关键的是，_reassembler中的序号是从0开始的，因此要对绝对序号-1，因为绝对序号中0代表SYN，最后一位代表FIN，同时我记得测试点中还存在着SYN包中带数据的，因此要特殊处理SYN包，
    _reassembler.push_substring(
        seg.payload().copy(), unwrap(seq, isn, stream_out().bytes_written()) - 1, seg.header().fin);
    //如果接受过Fin同时接收达到了Fin的位置，则选择关闭
    if (stream_out().input_ended()) {
        setFin = true;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!setIsn) {
        return {};
    }
    //这里，+1是加了SYN序号，+2是加了SYN序号和末尾的FIN序号
    return {wrap(stream_out().bytes_written() + (setFin ? 2 : 1), isn)};
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
