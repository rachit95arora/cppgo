#include <iostream>

#include "Machines.h"
#include "Select.h"
using namespace gocpp;
int main()
{
    std::cout << std::thread::hardware_concurrency() << std::endl;
    auto routine = [&](int id)
    {
        for (uint64_t i = 0; i < 0xFFFFFFF; i++)
        {
            if ((i & 0xFFFF) == 0)
            {
                std::cout << TID << " : Hello World " << id << " - " << i << "!\n";
            }
        }
    };
    auto routine1 = [&](int id)
    {
        std::cout << TID << " : Nesting " << id << "!\n";
        go(routine, id);
    };

    auto writerRoutine = [&](Channel<uint64_t> *channel)
    {
        defer(
            {
                std::cout << TID << " ] Writes complete, closing channel!\n";
                channel->close();
            });
        for (uint64_t i = 1; i < 0xFFFFFF; i++)
        {
            if ((i & 0xFFFF) == 0)
            {
                std::cout << TID << " ] Writing " << i << " to channel!\n";
                (*channel) << i;
            }
        }
    };
    auto readerRoutine = [&](Channel<uint64_t> *channel)
    {
        while (true)
        {
            uint64_t read = 0;
            std::cout << TID << " ] Read started!\n";
            if ((*channel) >> read)
            {
                std::cout << TID << " ] Read " << read << " from channel!\n";
            }
            else
            {
                std::cout << TID << " ] Reads complete, channel closed!\n";
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
                Case((*channelA) >= k, [&]()
                     { std::cout << TID << " Read " << k << " from channel A!\n"; }),
                Case((*channelB) >= k, [&]()
                     { std::cout << TID << " Read " << k << " from channel B!\n"; }),
                DefaultCase([&]()
                     { std::cout << TID << " Default case selected!\n"; })}();
        }
    };

    // go(routine1, 1);
    // go(routine1, 2);
    // go(routine1, 3);
    // go(routine1, 4);
    // go(routine1, 5);
    // go(routine1, 6);

    auto channelA = new Channel<uint64_t>();
    auto channelB = new Channel<uint64_t>();
    // go(writerRoutine, channel);
    // go(readerRoutine, channel);

    go(selectRoutine, channelA, channelB);

    for (uint64_t i = 0; i < 20; i++)
    {
        if (i & 1ul)
        {
            *channelB << i;
        
        }
        else
        {
            *channelA << i;
        }
    }

    return 0;
}