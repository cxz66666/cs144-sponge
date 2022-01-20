#include "network_interface.hh"

#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    auto cache_item = find_if(
        cache.begin(), cache.end(), [&](const ARPCache &item) -> bool { return item.ip_address == next_hop_ip; });
    EthernetFrame frame;
    if (cache_item == cache.end()) {
        auto pending_item =
            find_if(_frame_pending_out.begin(),
                    _frame_pending_out.end(),
                    [&](const pair<InternetDatagram, uint32_t> &item) -> bool { return item.second == next_hop_ip; });

        //如果已经有人在等了,同时没有超时
        if (pending_item != _frame_pending_out.end() && last_retx_time[next_hop_ip] + retx_waiting_time > now_time_ms) {
            //无论有没有人在等，自己都得进队列
            _frame_pending_out.push_back({dgram, next_hop_ip});
            return;
        }
        _frame_pending_out.push_back({dgram, next_hop_ip});

        //没人等，发送arp
        send_arp_request(next_hop_ip);
        //记录发送arp的时间
        last_retx_time[next_hop_ip] = now_time_ms;
    } else {
        send_ipv4(dgram, cache_item->ethernet_address);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    //如果不符合，直接不处理
    if (frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address) {
        return {};
    }
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram datagram;
        if (datagram.parse(frame.payload()) == ParseResult::NoError) {
            return {datagram};
        }
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arpMessage;
        if (arpMessage.parse(frame.payload()) == ParseResult::NoError) {
            //更新缓存
            auto arp_cache = find_if(cache.begin(), cache.end(), [&](ARPCache &item) -> bool {
                if (item.ethernet_address == arpMessage.sender_ethernet_address) {
                    //更新缓存
                    item.ip_address = arpMessage.sender_ip_address;
                    item.created_time = now_time_ms;
                    return true;
                }
                return false;
            });
            //缓存中没有,加入缓存
            if (arp_cache == cache.end()) {
                cache.push_back(
                    ARPCache{arpMessage.sender_ethernet_address, arpMessage.sender_ip_address, now_time_ms});
            }
            //发送回复
            if (arpMessage.opcode == ARPMessage::OPCODE_REQUEST &&
                arpMessage.target_ip_address == _ip_address.ipv4_numeric()) {
                send_arp_reply(arpMessage.sender_ethernet_address, arpMessage.sender_ip_address);
            }
            //等待arp结果的容器，等到结果后直接发送，同时不要忘记删除已经发送的
            for (auto m = _frame_pending_out.begin(); m != _frame_pending_out.end();) {
                if (m->second == arpMessage.sender_ip_address) {
                    send_ipv4(m->first, arpMessage.sender_ethernet_address);
                    m = _frame_pending_out.erase(m);
                } else {
                    m++;
                }
            }
        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    now_time_ms += ms_since_last_tick;
    for (auto m = cache.begin(); m != cache.end();) {
        if (m->created_time + expired_time <= now_time_ms) {
            m = cache.erase(m);
        } else {
            m++;
        }
    }
}

// message is the arp message, and next_hop_ip is next jump ip
void NetworkInterface::send_arp_request(uint32_t next_hop_ip) {
    ARPMessage arpMessage;
    arpMessage.opcode = ARPMessage::OPCODE_REQUEST;
    arpMessage.sender_ethernet_address = _ethernet_address;
    arpMessage.sender_ip_address = _ip_address.ipv4_numeric();
    arpMessage.target_ip_address = next_hop_ip;
    EthernetFrame frame;
    frame.payload() = arpMessage.serialize();
    frame.header().type = EthernetHeader::TYPE_ARP;
    frame.header().src = _ethernet_address;
    frame.header().dst = ETHERNET_BROADCAST;
    _frames_out.push(frame);
}
void NetworkInterface::send_ipv4(const InternetDatagram &dgram, EthernetAddress &dst) {
    EthernetFrame frame;
    frame.payload() = dgram.serialize();
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.header().src = _ethernet_address;
    frame.header().dst = dst;
    _frames_out.push(frame);
}
void NetworkInterface::send_arp_reply(EthernetAddress &dst, uint32_t dst_ip) {
    ARPMessage arp_reply;
    EthernetFrame frame_reply;
    arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
    arp_reply.sender_ethernet_address = _ethernet_address;
    arp_reply.target_ip_address = dst_ip;
    arp_reply.target_ethernet_address = dst;
    arp_reply.opcode = ARPMessage::OPCODE_REPLY;

    frame_reply.payload() = arp_reply.serialize();
    frame_reply.header().src = _ethernet_address;
    frame_reply.header().dst = dst;
    frame_reply.header().type = EthernetHeader::TYPE_ARP;
    _frames_out.push(frame_reply);
}
