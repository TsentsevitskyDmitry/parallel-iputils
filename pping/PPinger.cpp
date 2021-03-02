#include <iostream>
#include <ws2tcpip.h>
#include <chrono>
#include <thread>

#include "PPinger.h"

void PPinger::ping(std::string host, size_t size, size_t ttl, PingResult &res, bool log)
{
    std::shared_ptr<char> sendBuff;
    std::shared_ptr<char> recvBuff;

    SOCKET sd;
    sockaddr_in dest, source;

    size_t packetSize = std::max(sizeof(ICMPHeader),
            std::min(static_cast<size_t>(MAX_PING_DATA_SIZE), size));

    // Init everything
    if(initSocket(sd, ttl) < 0 || initDest(host, dest) < 0 || allocBuffers(sendBuff, recvBuff, packetSize) < 0)
    {
        return;
    }

    initPingPacket(sendBuff, packetSize);

    if (sendICMP(sd, dest, sendBuff, packetSize, log, host) >= 0)
    {
        if (recvICMP(sd, source, recvBuff, MAX_PING_PACKET_SIZE, res) < 0) {
            return;
        }

        decodeReply(recvBuff, packetSize, &source, res, log);
    }
}

int PPinger::initSocket(SOCKET &sd, size_t ttl)
{
    // Create the socket
    sd = WSASocket(AF_INET, SOCK_RAW, IPPROTO_ICMP, nullptr, 0, 0);
    if (sd == INVALID_SOCKET) {
        std::cout << "Failed to create raw socket: " << WSAGetLastError() <<
                std::endl;
        return -1;
    }

    if (setsockopt(sd, IPPROTO_IP, IP_TTL, reinterpret_cast<const char*>(&ttl),
            sizeof(ttl)) == SOCKET_ERROR) {
        std::cout << "TTL setsockopt failed: " << WSAGetLastError() << std::endl;
        return -1;
    }

    return 0;
}

int PPinger::initDest(std::string host, sockaddr_in &dest)
{
    // Initialize the destination host info block
    memset(&dest, 0, sizeof(dest));

    // Turn first passed parameter into an IP address to ping
    unsigned int addr = inet_addr(host.c_str());
    if (addr != INADDR_NONE) {
        // It was a dotted quad number, so save result
        dest.sin_addr.s_addr = addr;
        dest.sin_family = AF_INET;
    }
    else {
        // Not in dotted quad form, so try and look it up
        hostent* hp = gethostbyname(host.c_str());
        if (hp != nullptr) {
            // Found an address for that host, so save it
            memcpy(&(dest.sin_addr), hp->h_addr, static_cast<size_t>(hp->h_length));
            dest.sin_family = hp->h_addrtype;
        }
        else {
            // Not a recognized hostname either!
            std::cout << "Failed to resolve " << host << std::endl;
            return -1;
        }
    }

    return 0;
}

int PPinger::allocBuffers(std::shared_ptr<char> &sendBuff, std::shared_ptr<char> &recvBuff, size_t packetSize)
{
    sendBuff.reset(new char[packetSize]);
    recvBuff.reset(new char[MAX_PING_PACKET_SIZE + sizeof(IPHeader)]);
    return 0;
}

int PPinger::initPingPacket(std::shared_ptr<char> &sendBuff, size_t packetSize)
{
    auto icmp_hdr = reinterpret_cast<ICMPHeader*>(sendBuff.get());
    // Set up the packet's fields
    icmp_hdr->type = ICMP_ECHO_REQUEST;
    icmp_hdr->code = 0;
    icmp_hdr->checksum = 0;
    icmp_hdr->thread_id = getCurrentThreadUniqueId();
    icmp_hdr->timestamp = getCurrentTime();

    //char* datapart = (char*)icmp_hdr + sizeof(ICMPHeader);
    //size_t datalen = packet_size - sizeof(ICMPHeader);
    // new char[] already returns trush

    // Calculate a checksum on the result
    icmp_hdr->checksum = calcIpChecksum(reinterpret_cast<USHORT*>(icmp_hdr), packetSize);

    return 0;
}

USHORT PPinger::getCurrentTime()
{
    auto now = std::chrono::high_resolution_clock::now();
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    return static_cast<USHORT>(now_ms.time_since_epoch().count());
}

USHORT PPinger::getCurrentThreadUniqueId()
{
    return static_cast<USHORT>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

USHORT PPinger::calcIpChecksum(USHORT *buffer, size_t size)
{
    unsigned long cksum = 0;

    // Sum all the words together, adding the final byte if size is odd
    while (size > 1) {
        cksum += *buffer++;
        size -= sizeof(USHORT);
    }
    if (size) {
        cksum += *reinterpret_cast<UCHAR*>(buffer);
    }

    // Do a little shuffling
    cksum = (cksum >> 16) + (cksum & 0xffff);
    cksum += (cksum >> 16);

    // Return the bitwise complement of the resulting mishmash
    return static_cast<USHORT>(~cksum);
}

int PPinger::sendICMP(SOCKET &sd, const sockaddr_in &dest, std::shared_ptr<char> &sendBuff, size_t packetSize, bool log, std::string host)
{
    // Send the ping packet in send_buf as-is
    if(!host.empty()) host += " ";
    if(log)
        std::cout << "Sending " << packetSize << " bytes to " << host << "[" << inet_ntoa(dest.sin_addr) << "]... " << std::endl;

    int sended = sendto(sd, sendBuff.get(), static_cast<int>(packetSize),
                        0, reinterpret_cast<const sockaddr*>(&dest), sizeof(dest));

    if (sended == SOCKET_ERROR) {
        std::cout << "send failed: " << WSAGetLastError() << std::endl;
        return -1;
    }
    else if (sended < static_cast<int>(packetSize)) {
        std::cout << "but sent " << sended << " bytes..." << std::endl;
    }

    return 0;
}

int PPinger::recvICMP(SOCKET &sd, sockaddr_in &source, std::shared_ptr<char>& recvBuff, size_t packetSize, PingResult& res)
{
    while(1)
    {
        fd_set fds ;
        int n ;
        struct timeval tv ;

        // Set up the file descriptor set.
        FD_ZERO(&fds) ;
        FD_SET(sd, &fds) ;

        // Set up the struct timeval for the timeout.
        tv.tv_sec = 0;
        tv.tv_usec = 500'000; // 500 ms

        // Wait until timeout or data received.
        n = select(static_cast<int>(sd), &fds, nullptr, nullptr, &tv);
        if ( n == 0) // timeout
        {
          std::cout << "Timeout.." << std::endl;
          res.ttl_expire = true;
          res.size = packetSize;
          res.error = true;
          res.from_host = "???";
          res.time = -1;
          res.ttl = 0;
          return -1;
        }

        // Wait for the ping reply
        int fromlen = sizeof(source);
        int readed = recvfrom(sd, recvBuff.get(), static_cast<int>(packetSize + sizeof(IPHeader)),
                              0, reinterpret_cast<sockaddr*>(&source), &fromlen);

        if (readed == SOCKET_ERROR)
        {
            std::cout << "read failed: " << WSAGetLastError() << std::endl;
            return -1;
        }

        auto reply = reinterpret_cast<IPHeader*>(recvBuff.get());
        unsigned short header_len = reply->h_len * 4;
        ICMPHeader* icmphdr = reinterpret_cast<ICMPHeader*>(reinterpret_cast<char*>(reply) + header_len);

        if(icmphdr->type == ICMP_TTL_EXPIRE)
            icmphdr->thread_id = *reinterpret_cast<USHORT*>(reinterpret_cast<char*>(icmphdr) + /*ICMPHeader*/ 8 + sizeof(IPHeader) + /*Send header*/ 4);

        if (icmphdr->thread_id == getCurrentThreadUniqueId())
            break;

    }

    return 0;
}

int PPinger::decodeReply(std::shared_ptr<char> &recvBuff, size_t packetSize, sockaddr_in *from, PingResult &res, bool log)
{
    auto reply = reinterpret_cast<IPHeader*>(recvBuff.get());
    auto recvTime = getCurrentTime();
    res.ttl_expire = false;
    res.error = true;

    // Skip ahead to the ICMP header within the IP packet
    unsigned short header_len = reply->h_len * 4;
    ICMPHeader* icmphdr = reinterpret_cast<ICMPHeader*>(reinterpret_cast<char*>(reply) + header_len);

    // Make sure the reply is sane
    if (packetSize < header_len + static_cast<size_t>(ICMP_MIN)) {
        std::cout << "too few bytes from " << inet_ntoa(from->sin_addr) << std::endl;
        return -1;
    }
    else if (icmphdr->type != ICMP_ECHO_REPLY) {
        if (icmphdr->type != ICMP_TTL_EXPIRE) {
            if (icmphdr->type == ICMP_DEST_UNREACH) {
                std::cout << "Destination unreachable" << std::endl;
            }
            else {
                std::cout << "Unknown ICMP packet type " << int(icmphdr->type) << " received" << std::endl;
            }
            return -1;
        }
        // If "TTL expired", fall through.  Next test will fail if we
        // try it, so we need a way past it.
    }

    if (icmphdr->type == ICMP_TTL_EXPIRE) {
        icmphdr->timestamp = *reinterpret_cast<USHORT*>(reinterpret_cast<char*>(icmphdr) + /*ICMPHeader*/ 8 + sizeof(IPHeader) +
                                                        /*Send header*/ 4 + sizeof(icmphdr->thread_id));
        res.ttl_expire = true;
    }

    in_addr source;
    source.S_un.S_addr = reply->source_ip;

    res.from_host = inet_ntoa(source);
    res.time = recvTime - icmphdr->timestamp;
    res.ttl = reply->ttl;
    res.size = packetSize;

    if(res.ttl_expire)
        std::cout << "TTL expired on " << res.from_host << std::endl;
    else if (log)
        std::cout << "Got " << res.size << " bytes from " << res.to_host <<
                     ", ttl " << static_cast<int>(res.ttl) << ", " <<
                     "time: " << res.time <<" ms." <<
                     std::endl;

    res.error = false;
    return 0;
}
