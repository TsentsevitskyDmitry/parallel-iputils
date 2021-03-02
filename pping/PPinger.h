#pragma once
#include <vector>
#include <string>
#include <memory>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

// ICMP packet types
#define ICMP_ECHO_REPLY 0
#define ICMP_DEST_UNREACH 3
#define ICMP_TTL_EXPIRE 11
#define ICMP_ECHO_REQUEST 8

// Minimum ICMP packet size, in bytes
#define ICMP_MIN 8

#pragma pack(push, 1)

// The IP header
struct IPHeader {
    BYTE h_len:4;           // Length of the header in dwords
    BYTE version:4;         // Version of IP
    BYTE tos;               // Type of service
    USHORT total_len;       // Length of the packet in dwords
    USHORT ident;           // unique identifier
    USHORT flags;           // Flags
    BYTE ttl;               // Time to live
    BYTE proto;             // Protocol number (TCP, UDP etc)
    USHORT checksum;        // IP checksum
    ULONG source_ip;
    ULONG dest_ip;
};

// ICMP header
struct ICMPHeader {
    BYTE type;          // ICMP packet type
    BYTE code;          // Type sub code
    USHORT checksum;
    USHORT thread_id;
    USHORT timestamp;    // not part of ICMP, but we need it
};

#pragma pack(pop)

struct PingResult{
    ULONG64 size;
    LONG64 time;
    std::string to_host;
    std::string from_host;
    BYTE ttl;
    BOOL ttl_expire;
    BOOL error;
};

#define MAX_PING_DATA_SIZE 1024
#define MAX_PING_PACKET_SIZE (MAX_PING_DATA_SIZE + sizeof(IPHeader))

class PPinger
{
public:
    static void ping(std::string host, size_t size, size_t ttl, PingResult& res, bool log = true);

private:
    static int initSocket(SOCKET& sd, size_t ttl);
    static int initDest(std::string host, sockaddr_in& dest);
    static int allocBuffers(std::shared_ptr<char>& sendBuff, std::shared_ptr<char>& recvBuff, size_t packetSize);
    static int initPingPacket(std::shared_ptr<char>& sendBuff, size_t packetSize);

    static USHORT getCurrentTime();
    static USHORT getCurrentThreadUniqueId();
    static USHORT calcIpChecksum(USHORT* buffer, size_t size);

    static int sendICMP(SOCKET &sd, const sockaddr_in& dest, std::shared_ptr<char> &sendBuff, size_t packetSize, bool log, std::string host = "");
    static int recvICMP(SOCKET &sd, sockaddr_in& source, std::shared_ptr<char> &recvBuff, size_t packetSize, PingResult& res);
    static int decodeReply(std::shared_ptr<char> &recvBuff, size_t packetSize, sockaddr_in* from, PingResult& res, bool log);
};
