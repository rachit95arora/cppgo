# cppgo: Go routine scheduling runtime and concurrency in C++

## What's cppgo?

This library is a stab at implementing some of the above Go runtime features (in an obviously limited manner) in C++ while exposing to users a similarly powerful
abstraction. It implements goroutine like user threads, channel conduits, select like channel dispatching, defer statements (and potentially more). Following sections will
list a few examples, describe some Golang background and finally the high level approach, tradeoffs and limitations of this implementation.

## Examples

For someone coming from a baseline Golang knowledge, here are a few examples of the features supported by this library. For someone looking to read about these constructs in Golang,
[Tour of Go](https://tour.golang.org/) might come handy!

#### Goroutines, Channels and Defer

Go code
```go
func writerRoutine(ch chan int) {
	defer fmt.Println("Writes complete, closing channel!")
	defer close(ch)

	for i := 0; i < 0xFFFFFF; i++ {
		if i&0xFFFF == 0 {
			fmt.Println("Writing ", i, " to channel!")
			ch <- i
		}
	}
}

func readerRoutine(ch chan int) {
	for {
		fmt.Println("Read started")
		val, ok := <-ch
		if ok {
			fmt.Println("Read ", val, " from channel!")
		} else {
			fmt.Println("Reads complete, channel closed!")
			return
		}
	}
}

func main() {
	ch := make(chan int)
	go writerRoutine(ch)
	readerRoutine(ch)
}

```

Analogous C++ code
```cpp
// Callables (lambda/ functions to run)
    auto writerRoutine = [&](Channel<uint64_t> *channel)
    {
        // Notice the use of defer to run cleanup code block
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

    // Creating a channel to share between the two functions
    auto channel = new Channel<uint64_t>();
    // Launching the two routines with the above channels will allow writes to happen on the channel
    // via the writerRoutine, which are then sequentially read by the readerRoutine and printed
    go(writerRoutine, channel);
    go(readerRoutine, channel);

```

#### Select

```cpp

    // Tries to read from the two channels, executes default case otherwise
    auto selectRoutine = [&](Channel<uint64_t> *channelA, Channel<uint64_t> *channelB)
    {
        uint64_t k = 0, i = 0;
        while (i < 40)
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

    auto channelA = new Channel<uint64_t>();
    auto channelB = new Channel<uint64_t>();

    go(selectRoutine, channelA, channelB);

    // Write on the two channels for the select to read from alternately
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
```
**Caveat**: Do observe that channel operations inside a Select, use the `<=` and `=>` operators instead of the `<<` and `>>` stream operators used outside of
select. This is **intentional** to keep coding style as similar to golang as possible while allowing Select to work on the list of channels as "descriptors".

## Background

Golang offers a somewhat unique abstraction for parallel computation in the form of goroutines and an equally salient method for communicating
between them via channels. Synchronization is also handled in part by the channel abstraction. All of this allows for a very simplified user experience
which is often felt missing in a language like C++.

The Golang goroutines are not kernel threads and in fact are just lightweight stacks and contexts that are scheduled on the available kernel threads.
Therefore unlike C++ std::thread (or pthread) which employ the kernel scheduling system for switching, goroutines are infact really coroutines that
are being switched in the user space over actual kernel threads via the Golang runtime in a many to many (M:N) threading model.

This is achieved via a combination of:
* Cooperation : Goroutines yield to runtime scheduler upon encountering function calls
* Preemption : For misbehaving goroutines, timer signals preempt the core back to the scheduler runtime

Therefore several 100s or even 1000s of goroutines can be submitted to the golang runtime (apparently Go devs are notorious for this), while only really
having very few logical cores for their execution. Since user space context switching is significantly cheaper than kernel space switching, Goroutines are
performant.

The scheduler runtime contains an abstraction of core (kernel thread) of execution called Machine (M). Each machine tries to acquire a Processor (P).
The processor (P) is what really contains the list of runnable goroutines (R). This system is enabled via a Global Routine Queue (GRQ) and Local Routine Queues
at each processor (P). M is where a routine really runs, which it requests from a P, which in turn pulls routines from the GRQ.

Furthermore it is possible for idle machines to **steal** routines from other machines with heavy routine queues. This is therefore a two level scheduling
subsystem. It is designed this way, among other reasons, to allow for efficient routine execution. A machine that is about to execute a blocking network
or system call, yields its processor (LRQ) for other machines to execute from, so a blocking kernel thread only really blocks the routine requesting it.
Lastly not all IO needs to be blocking in Go, and for IO that can be asynchronous (like some network calls or channel read/writes), Go runtime is very
efficient as it swaps out a waiting routine into the local wait queue, while executing the next runnable routine. Therefore a channel read and write
which really are blocking for the user, do not actually waste much CPU time as they are swapped intelligently. Infact a seemingly blocking read, followed
by a blocking write on a single kernel thread would succeed due to goroutine scheduling.

Goroutines, Channels, Select and other paradigms in Golang together create a very simple yet powerful and fast way to write concurrent efficient code.

## Library design

The library is designed to allow users to write their goroutine functions in an agnostic manner like Go, removing the possibility of cooperative yielding. We
therefore leverage Unix timer signals (SIGRTMIN) to launch a preemption of running coroutines and execution of scheduler coroutine across all the running
threads. The coroutines themselves are implemented using ucontext_t, each with a separate stack which can be configured with **GOSTACKSIZE**.
The coroutines are preempted by signals or exit to execute the scheduler coroutine and run across all library threads in a many to many fashion. New coroutines
are submitted to the current Processor's LRQ (if applicable) or to the global queue from the main thread. Idle machine threads try to steal coroutines from the
global queue or other threads' LRQs.

This design adds opportunity to utilize CPU time more effectively than blocking on an operation (lock or network), and a coroutine can be switched out when its waiting
freeing up the CPU for some other runnable routine. This allows for implementation of Go like Channels in this library, that are blocking in the coroutine for the user
but never block the underlying CPU/thread.