#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return time_since_last_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    //重置接收到的时间
    time_since_last_received = 0;

    //如果rst，设置完error后立刻结束
    if (seg.header().rst) {
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _active = false;
        return;
    }

    //给receive处理
    _receiver.segment_received(seg);
    //如果receive已经成功处理好ack number，则说明正在请求建立连接
    if (!_receiver.ackno().has_value()) {
        return;
    }

    //如果带了ack，则更新sender的窗口和ack
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    //判断自己方发送，对方接收的这半条连接是否在另外半条之后结束，换句话数：对方先结束
    if (!_sender.stream_in().eof() && _receiver.stream_out().eof()) {
        _linger_after_streams_finish = false;
    }

    // keep-alive handle
    if (_receiver.ackno().has_value() and (seg.length_in_sequence_space() == 0) and
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    }
    _sender.fill_window();

    //如果该段不为空，同时发送队列为空，则发送一个空的段
    if (seg.length_in_sequence_space() && _sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }

    send_segment();
}

void TCPConnection::send_segment(const bool rst) {
    while (!_sender.segments_out().empty()) {
        TCPSegment &segment = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            segment.header().ackno = _receiver.ackno().value();
            segment.header().ack = true;
        }
        segment.header().win = _receiver.window_size();
        segment.header().rst = rst;
        _segments_out.push(segment);
        //        cout << segment.length_in_sequence_space() << endl;
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    if (data.empty()) {
        return 0;
    }
    size_t ans = _sender.stream_in().write(data);
    _sender.fill_window();
    send_segment();
    return ans;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    time_since_last_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);

    //如果大于重传次数
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        //如果空，则需要发送一个空的
        if (_sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
        send_segment(true);
        //结束自己的
        _receiver.stream_out().set_error();
        _sender.stream_in().set_error();
        _active = false;
        return;
    }

    //判断是否结束
    if (!unassembled_bytes() && _receiver.stream_out().eof() && _sender.stream_in().eof() && !bytes_in_flight() &&
        (!_linger_after_streams_finish || (time_since_last_segment_received() >= 10 * _cfg.rt_timeout))) {
        _active = false;
        return;
        //需要等待
    }

    if (_sender.next_seqno_absolute() == 0) {
        return;
    }
    send_segment();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segment();
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_segment();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            _sender.send_empty_segment();
            send_segment(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
