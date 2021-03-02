#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>

#include "PPinger.h"

using namespace std::chrono_literals;

int main(int argc, char** argv)
{
    // Start Winsock up
    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 1), &wsaData) != 0) {
        std::cout << "Failed to find Winsock 2.1 or better." << std::endl;
        return -1;
    }

    if (argc < 2) {
        std::cout << "Usage:\n\tptracert <host1> [host2] .. [hostN]" << std::endl;
        return -1;
    }

    std::vector<std::string> hosts;
    std::vector<std::thread> threads;
    std::vector<std::vector<PingResult>> results;
    std::mutex mutex;

    size_t size = 32;
    size_t max_ttl = 30;

    for(int i = 0; i < argc - 1; ++i)
        hosts.push_back(argv[i + 1]);
    results.resize(hosts.size());

    size_t idx = 0;
    for (auto& host : hosts)
    {
        auto& results_vec = results[idx++];

        auto routine = [&host, &size, &max_ttl, &mutex, &results_vec](){
            PingResult res;
            res.to_host = host;
            res.ttl_expire = true;
            uint8_t ttl = 1;

            do{
                if(ttl <= max_ttl){
                    PPinger::ping(host, size, ttl, res);
                    mutex.lock();
                    results_vec.push_back(res);
                    mutex.unlock();
                    ++ttl;
                }
                else {
                    break;
                }
            } while (res.ttl_expire);
        };

        threads.emplace_back(routine);
    }

    for(auto& t : threads)
        t.join();

    std::cout << std::endl
              << std::string(60, '-')
              << std::endl;

    for(auto& ress : results)
    {
        std::cout << "Tracing to " << "host" << ":" << std::endl;

        size_t idx = 1;
        for(auto& r : ress){
            std::cout << idx++ << " " << r.time << "ms " << r.from_host << std::endl;
        }

        std::cout << std::endl;
    }

    WSACleanup();
    return 0;
}
