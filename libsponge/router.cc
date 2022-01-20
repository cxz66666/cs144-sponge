#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const std::optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    routers.push_back(RouterDetail{route_prefix, prefix_length, next_hop, interface_num});
}
//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    // ttl<=1直接扔了
    if (dgram.header().ttl <= 1) {
        return;
    }

    int most_prefix = -1;
    size_t index = -1;

    uint32_t dst = dgram.header().dst;

    for (size_t i = 0; i < routers.size(); i++) {
        uint32_t mask =
            routers[i].prefix_length == 0 ? 0 : numeric_limits<int>::min() >> (routers[i].prefix_length - 1);
        if ((dst & mask) == routers[i].route_prefix) {
            if (static_cast<int>(routers[i].prefix_length) > most_prefix) {
                most_prefix = routers[i].prefix_length;
                index = i;
            }
        }
    }
    if (most_prefix < 0) {
        return;
    }
    dgram.header().ttl--;

    interface(routers[index].interface_num)
        .send_datagram(dgram,
                       routers[index].next_hop.has_value() ? routers[index].next_hop.value()
                                                           : Address::from_ipv4_numeric(dst));
    // Your code here.
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
