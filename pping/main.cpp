#include <iostream>
#include <vector>
#include <thread>
#include <mutex>

#include "PPinger.h"

int main(int argc, char** argv)
{
    // Start Winsock up
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 1), &wsaData) != 0) {
        std::cout << "Failed to find Winsock 2.1 or better." << std::endl;
        return -1;
    }

    if (argc < 2) {
        std::cout << "Usage:\n\tpping <host1> [host2] .. [hostN]" << std::endl;
        return -1;
    }

    std::vector<std::string> hosts;
    std::vector<std::thread> threads;
    std::vector<PingResult> results;
    std::mutex mutex;

    size_t size = 32;
    size_t ttl = 30;

    for(int i = 0; i < argc - 1; ++i)
        hosts.push_back(argv[i + 1]);

    for (auto& host : hosts)
    {
        auto routine = [&host, &size, &ttl, &mutex, &results](){
            PingResult res;
            res.to_host = host;

            PPinger::ping(host, size, ttl, res);

            mutex.lock();
            results.push_back(res);
            mutex.unlock();
        };

        threads.emplace_back(routine);
    }

    for(auto& t : threads)
        t.join();

    std::cout << std::endl
              << std::string(60, '-')
              << std::endl;

    for(auto r : results)
        std::cout << "Ping " << r.to_host << (r.ttl_expire * r.error ? " failed" : " sucsess")
                  << ", ttl: " << r.ttl - '\0' << ", time: " << r.time << "ms" << std::endl;

    WSACleanup();
    return 0;
}
