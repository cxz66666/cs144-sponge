#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _retransmission_timeout{retx_timeout}
    , _now_retransmission_timeout{0}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _ack_seqno; }

void TCPSender::fill_window() {
    TCPSegment segment;
    string data;

    //如果满足发送FIN,注意这里需要满足一个额外的条件，
    //窗口还有空余或者窗口为0，这里的窗口一定接受过ack，所以如果被ack中置为0，那么当作1用，否则就是正常的，需要等待有窗口了才能发
    if (_stream.eof() && _next_seqno == _stream.bytes_written() + 1 &&
        (this->bytes_in_flight() < windows_size || !windows_size)) {
        this->start_rto_clock();
        segment.header().seqno = wrap(_next_seqno, _isn);
        segment.header().fin = true;
        _segments_out.push(segment);
        _outstanding_node.push(segment);
        _next_seqno += segment.length_in_sequence_space();
        return;
        //如果FIN已经发送完了，也就是啥都不用发送了，直接返回
    } else if (_stream.eof() && _next_seqno == _stream.bytes_written() + 2) {
        return;
        //如果buffer里面没有东西需要发送了，同时已经建立了tcp连接，但是仍然没有关闭，直接返回，因为没东西发
    } else if (_next_seqno > 0 && _stream.buffer_empty()) {
        return;
        //如果等待建立连接中，直接返回
    } else if (_ack_seqno == 0 && _next_seqno == 1) {
        return;
    }
    //如果窗口大小为0
    if (!windows_size) {
        //如果已经发送过东西了（1 byte），则不能再发送了，
        if (!_outstanding_node.empty()) {
            return;
        }
        this->start_rto_clock();
        //刚开始，发送SYN
        if (_next_seqno == 0) {
            segment.header().syn = true;
            segment.header().seqno = _isn;
            //读取一个字符，因为特殊情况已经特判过了
        } else {
            segment.payload() = Buffer(_stream.read(1));
            segment.header().seqno = wrap(_next_seqno, _isn);
            data = segment.payload().copy();
        }
        _segments_out.push(segment);
        _outstanding_node.push(segment);
        _next_seqno += segment.length_in_sequence_space();

    } else {
        while (!_stream.buffer_empty() && this->bytes_in_flight() < windows_size) {
            this->start_rto_clock();

            TCPSegment s;
            s.payload() = Buffer(_stream.read(
                min(TCPConfig::MAX_PAYLOAD_SIZE, static_cast<size_t>(windows_size - this->bytes_in_flight()))));

            //得看看有没有FIN信号的位置，这里要注意一点，就是size可以等于TCPConfig::MAX_PAYLOAD_SIZE，因为本来就不占用payload
            // unassembled_size，但是不能超过windows unassembled_size，因为FIN占用一个windows unassembled_size
            if (_stream.eof() &&
                s.length_in_sequence_space() < static_cast<size_t>(windows_size - this->bytes_in_flight())) {
                s.header().fin = true;
            }
            s.header().seqno = wrap(_next_seqno, _isn);
            _segments_out.push(s);
            _outstanding_node.push(s);
            _next_seqno += s.length_in_sequence_space();
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window unassembled_size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t ackno_u64 = unwrap(ackno, _isn, _stream.bytes_read());
    //没有新东西，或者ack号甚至超过了发送号,直接返回
    if (ackno_u64 < _ack_seqno || ackno_u64 > _next_seqno) {
        return;
    }
    //如果这个和上一个相等，则取大的窗口后即可返回
    if (ackno_u64 == _ack_seqno) {
        windows_size = max(static_cast<uint64_t>(window_size), windows_size);
        return;
    }

    _ack_seqno = ackno_u64;
    windows_size = window_size;
    _retransmission_timeout = _initial_retransmission_timeout;
    retransmission_count = 0;

    while (!_outstanding_node.empty()) {
        TCPSegment &tmp = _outstanding_node.front();
        if (unwrap(tmp.header().seqno, _isn, _stream.bytes_read()) + tmp.length_in_sequence_space() <= _ack_seqno) {
            _outstanding_node.pop();
        } else {
            break;
        }
    }

    //判断还有没有等待的，如果有则将超时置为0，没有则将flag置为false
    if (!_outstanding_node.empty()) {
        _now_retransmission_timeout = 0;
    } else {
        start_rto = false;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!start_rto) {
        return;
    }
    _now_retransmission_timeout += ms_since_last_tick;
    //如果还没超时，或者没有处于未确认状态的，直接返回
    if (_now_retransmission_timeout < _retransmission_timeout || !_outstanding_node.size()) {
        return;
    }

    this->start_rto_clock();
    _segments_out.push(_outstanding_node.front());

    //如果是重发SYN帧，或者之后的windows_size不为0
    if (windows_size || _outstanding_node.front().header().syn) {
        retransmission_count++;
        _retransmission_timeout *= 2;
    }
    _now_retransmission_timeout = 0;
}

unsigned int TCPSender::consecutive_retransmissions() const { return retransmission_count; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    //只需要设置seqno
    segment.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(segment);
}
