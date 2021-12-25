#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    n += isn.raw_value();
    return WrappingInt32{static_cast<uint32_t>(n & 0x0FFFFFFFF)};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t tmp = (checkpoint >> 32) << 32;
    if (n.raw_value() >= isn.raw_value()) {
        tmp += static_cast<uint64_t>(n.raw_value() - isn.raw_value());
    } else {
        tmp += 0x0100000000 - isn.raw_value() + n.raw_value();
    }

    if (tmp < checkpoint) {
        if (tmp + 0x080000000 < checkpoint) {
            return tmp + 0x0100000000;
        }
        return tmp;
    } else {
        if (checkpoint + 0x080000000 < tmp && tmp >= 0x0100000000) {
            return tmp - 0x0100000000;
        }
        return tmp;
    }
}
