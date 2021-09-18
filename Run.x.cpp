#include <iostream>

#include "Machines.h"
#include "Select.h"
using namespace gocpp;
int main()
{
    std::cout << std::thread::hardware_concurrency() << std::endl;

    auto writerRoutine = [&](Channel<uint64_t> *channel)
    {
        defer(
            {
                std::cout << "Writes complete, closing channel!\n";
                channel->close();
            });
        for (uint64_t i = 1; i < 0xFFFFFF; i++)
        {
            if ((i & 0xFFFF) == 0)
            {
                std::cout << "Writing " << i << " to channel!\n";
                *channel << i;
            }
        }
    };
    auto readerRoutine = [&](Channel<uint64_t> *channel)
    {
        while (true)
        {
            uint64_t read = 0;
            std::cout << "Read started!\n";
            if (*channel >> read)
            {
                std::cout << "Read " << read << " from channel!\n";
            }
            else
            {
                std::cout << "Reads complete, channel closed!\n";
                return;
            }
        }
    };

    auto selectRoutine = [&](Channel<uint64_t> *channelA, Channel<uint64_t> *channelB)
    {
        uint64_t k = 0, i = 0;
        while (i < 100)
        {
            std::cout << "Loop " << ++i << std::endl;
            Select{
                Case(*channelA >= k, [&]()
                     { std::cout << "Read " << k << " from channel A!\n"; }),
                Case(*channelB >= k, [&]()
                     { std::cout << "Read " << k << " from channel B!\n"; }),
                DefaultCase([&]()
                     { std::cout << "Default case selected!\n"; })}();
        }
    };

    auto channelA = new Channel<uint64_t>();
    auto channelB = new Channel<uint64_t>();
    go(writerRoutine, channelA);
    go(readerRoutine, channelA);

    // go(selectRoutine, channelA, channelB);

    // for (uint64_t i = 0; i < 20; i++)
    // {
    //     if (i & 1ul)
    //     {
    //         *channelB << i;
        
    //     }
    //     else
    //     {
    //         *channelA << i;
    //     }
    // }

    return 0;
}