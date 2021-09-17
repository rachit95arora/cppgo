# cppgo: Go routine scheduling runtime and concurrency in C++

## What's cppgo?

This library is a stab at implementing some of the above Go runtime features (in an obviously limited manner) in C++ while exposing to users a similarly powerful  
abstraction. It implements goroutine like user threads, channel conduits, select like channel dispatching, defer statements (and potentially more). Following section will
describe the high level approach, tradeoffs and limitations of this implementation.

## Examples
For someone coming from a baseline Golang knowledge, here are a few examples of the features supported by this library.

#### Goroutines, Channels and Defer
```
// Callables (lambda/ functions to run)
    auto writerRoutine = [&](WriteChannel<uint64_t> *channel)
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
    auto readerRoutine = [&](ReadChannel<uint64_t> *channel)
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

    // Creating a channel to share between the two functions
    auto channel = new Channel<uint64_t>();
    // Launching the two routines with the above channels will allow writes to happen on the channel
    // via the writerRoutine, which are then sequentially read by the readerRoutine and printed
    go(writerRoutine, channel);
    go(readerRoutine, channel);

```
## Background

Golang offers a somewhat unique abstraction for parallel computation in the form of goroutines and an equally salient method for communicating  
between them via channels. Synchronization is also handled in part by the channel abstraction. All of this allows for a very simplified user experience  
which is often felt missing in a language like C++

The Golang goroutines are not kernel threads and in fact are just lightweight stacks and contexts that are scheduled on the available kernel threads.  
Therefore unlike C++ std::thread (or pthread) which employ the kernel scheduling system for switching, goroutines are infact really coroutines that  
are being switched in the user space over actual kernel threads via the Golang runtime in a many to many (M:N) threading model.  

This is achieved via a combination of:
* Cooperation : Goroutines yield to runtime scheduler upon encountering function calls
* Preemption: For misbehaving goroutines, timer signals preempt the core back to the scheduler runtime

Therefore several 100s or even 1000s of goroutines can be submitted to the golang runtime (apparently Go devs are notorious for this), while only really
having very few logical cores for their execution. Since user space context switching is significantly cheaper than a kernel space switch, Goroutines are  
performant.

The scheduler runtime contains an abstraction of core (kernel thread) of execution called Machine (M). Each machine tries to acquire a Processor (P).
The processor (P) is what really contains the list of runnable goroutines (R). This system is enabled via a Global Routine Queue (GRQ) and Local Routine Queues  
at each processor (P). M is where a routine really runs, which it requests from a P, which in turn pulls routines from the GRQ.

Furthermore it is possible for idle machines to **steal** routines from other machines with heavy routine queues. This is therefore a two level scheduling  
subsystem. It is designed this way, among other reasons, to allow for efficient routine execution. A machine that is about to execute a blocking network  
or system call, yields its processor (LRQ) for other machines to execute from, so a blocking kernel thread only really blocks the routine requesting it.
Lastly not all IO needs to be blocking in Go, and for IO that can be asynchronous (like some network calls or channel read/writes), Go runtime is very  
efficient as it swaps out a waiting routine into the local wait queue, while executing the next runnable routine. Therefore a channel read and write which  
call which really are blocking for the user, do not actually waste much CPU time as they are swapped intelligently. Infact a seemingly blocking read, followed by
a blocking write on a single kernel thread would succeed due to goroutine scheduling.

Goroutines, Channels, Select and other paradigms in Golang together create a very simple yet powerful and fast way to write concurrent efficient code.