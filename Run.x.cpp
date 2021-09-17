#include <iostream>

#include "Machines.h"
#include "Select.h"
using namespace gocpp;
int main()
{
    std::cout << std::thread::hardware_concurrency() << std::endl;
    auto routine = [&](int id)
    {
        //sleep(id);
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
        //sleep(1);
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

    auto selfFeedingRoutine = [&](Channel<uint64_t> *channel)
    {
        uint64_t k = 0;
        for (uint64_t i = 1; i < 0xFF; i++)
        {
            std::cout << "Loop " << i << std::endl;
            SelectEvaluator eval{
                CASE((*channel) <= i, [&]()
                     { std::cout << TID << " Wrote " << i << " to channel!\n"; }),
                CASE((*channel) >= k, [&]()
                     { std::cout << TID << " Read " << k << " from channel!\n"; })};
            eval();
        }
    };

    // go(routine1, 1);
    // go(routine1, 2);
    // go(routine1, 3);
    // go(routine1, 4);
    // go(routine1, 5);
    // go(routine1, 6);

    auto channel = new Channel<uint64_t>();
    // go(writerRoutine, channel);
    // go(readerRoutine, channel);

    go(selfFeedingRoutine, channel);

    return 0;
}