# ESPressio Threads
Threading Components of the Flowduino ESPressio Development Platform.

Light-weight and easy-to-use Threading for your Microcontroller development work.

## Latest Stable Version
There is currently no stable released version.

## ESPressio Development Platform
The **ESPressio** Development Platform is a collection of discrete (sometimes intra-connected) Component Libraries developed with a particular development ethos in mind.

The key objectives of the ESPressio Development Platform are:
- **Light-weight** - The Components should always strive to optimize memory consumption and operational overhead as much as possible, but not to the detriment of...
- **Ease of Use** - Many of our components serve as Developer-Friendly Abstractions of existing procedural code libraries.
- **Object-Oriented** - A `type` for everything, and everything in a `type`!
- **SOLID**:
- -  > **S**ingle Responsibility Principle (SRP)
    Break your code into smaller, focused components.
- - > **O**pen/Closed Principle (OCP)
    Be open for extension but closed for modification.
- - > **L**iskov Substitution Principle (LSP)
    Be substitutable for the base type without altering correctness.
- - > **I**nterface Segregation Principle (ISP)
    Break interfaces into specific, client-focused ones.
- - > **D**ependency Inversion Principle (DIP)
    Be dependent on abstractions, not concretions.

To the maximum extent possible within the limitations/restrictons/constraints of the C++ langauge, the Arduino platform, and Microcontroller Programming itself, all Component Libraries of the **ESPressio** Development Platform must strive to honour the **SOLID** principles.

## License
ESPressio (and its component libraries, including this one) are subject to the *Apache License 2.0*
Please see the [![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](LICENSE) accompanying this library for full details.

## Namespace
Every type/variable/constant/etc. related to *ESPressio* Threads are located within the `Threads` submaspace of the `ESPressio` parent namespace.

The namespace provides the following (*click on any declaration to navigate to more info*):
- [`ESPressio::Threads::IThread`](#ithread)
- [`ESPressio::Threads::Thread`](#thread)
- [`ESPressio::Threads::Manager`](#threadmanager)
- [`ESPressio::Threads::GarbageCollector`](#garbagecollector)
- [`ESPressio::Threads::IThreadSafe`](#ithreadsafe)
- [`ESPressio::Threads::Mutex`](#mutex)
- [`ESPressio::Threads::ReadWriteMutex`](#readwritemutex)

## Platformio.ini
You can quickly and easily add this library to your project in PlatformIO by simply including the following in your `platformio.ini` file:

```ini
lib_deps = 
    https://github.com/Flowduino/ESPressio-Base.git
	https://github.com/Flowduino/ESPressio-Threads.git
```

Please note that this will use the very latest commits pushed into the repository, so volatility is possible.
This will of course be resolved when the first release version is tagged and published.
This section of the README will be updated concurrently with each release.

## Understanding Threads
Threads enable us to perform concurrent and/or parallel processing on our microcontroller devices.
In the case of multi-core microcontrollers, such as the ESP32, we can achieve true concurrent execution by using the components provided here in the *ESPressio* Thread Library.

By default, when an instance of a [`Thread`](#thread) descendant is created, presuming that you do not modify by calling `SetCoreID()` prior to initializing the instance, the Thread [`Manager`](#threadmanager) will automatically allocate the Thread to the next CPU Core.

For example, by default, your first Thread Instance will occupy *CPU 0*, your second will occupy *CPU 1*, your third will co-occupy *CPU 0*.

However, as hinted previously (and as you'll see later in this document) you can very easily define explicitly which CPU Core you want your `Thread` to run on.

Now, when your Microcontroller doesn't have multiple CPU Cores, or when you have multiple threads co-tenanting the same CPU Cores, Threads will operate on the princpals of *Time Slicing*. This is where `Thread`s are executed in *Parallel* (not the same as *Concurrent*), and they each get slices of time within which to continue execution.

In this way, multiple distinct contexts can be progressed without having to wait for each of them to complete in turn.

## Thread Safety
Those of you familiar with multi-threading will already be aware of the need to enforce careful *thread-safety* when working with multiple `Thread`s.

*ESPressio* Threads makes it easy, providing multiple choices of *Thread-Safe Locks* for you to easily use.

You'll see an example later in this document.

## Basic Usage
ESPressio Threads have been designed with ease of use in mind.

Ultimately, they are a carefully *Managed Encapsulation* of `Task`s, abstracted to operate and interface more alike a true `Thread` in modern desktop and mobile development.

Let's take a look at a really simple implementation:

### Includes...
Before we define our `Thread`, we need to include the required header:
```cpp
    #include <ESPressio_Thread.hpp>
```

### Namespaces...
Given that *ESPressio* Threads uses multi-tier Namespacing throughout, let's declare our Namespace so that we can reference the necessary type identifiers with less code:
```cpp
    using namespace ESPressio::Threads;
```

### A `Thread` type...
With the required header linked, and the namespace defined, we can now define a simple `Thread` type, which we shall call `MyFirstThread`:
```cpp
class MyFirstThread : public Thread {
    protected:
        void OnInitialization() override {
            // Anything we need to do here prior to the Thread's Loop sstarting
        }

        void OnLoop() override {
            // Whatever we want to do within the Loop
        }
};
```
>NOTE: It is not necessary to override `OnInitialization` unless you have a reason. It is virtual, not abstract.

We shall be building from this basic example `class` throughout the rest of this documentation!

So, the above class declaration doesn't really do anything... let's build upon it to illustrate how multiple `Thread`s work:
```cpp
class MyFirstThread : public Thread {
    private:
        int _counter = 0;
    protected:
        void OnInitialization() override {
            // Anything we need to do here prior to the Thread's Loop sstarting
        }

        void OnLoop() override {
            _counter++; // Increment the counter

            // Let's display some information about our Thread...
            Serial.printf("MyFirstThread::OnLoop() - Thread #%d - On CPU %d, Counter = %d", GetThreadID(), xPortGetCoreID(), _counter);

            delay(1000); // Let's let this Thread wait for 1 second before it loops around again
        }
};
```
With the above changes, any instance of `MyFirstThread` will execute its `OnLoop()` method every one second, and each time it does, it'll increment a *counter*, then print out the following information in the `Serial` console:
- The Thread ID
- Which CPU the Thread is running on
- The value of the *Counter*

Admittedly, this isn't the most practical use of a `Thread`, however, it is an *illustrative* one.

### The `setup()` method...
Let's quickly assemble a program to use `MyFirstThread`:
```cpp
MyFirstThread thread1;

void setup() {
    Serial.begin(115200);

    delay(500); // Small delay just so that the thread doesn't start before the Serial Monitor is ready

    thread1.Initialize();
}
```
That's all there is to it! If you push this program to your (compatible) microcontroller, it will immediately start printing the following into your Serial console (once per second):
```
MyFirstThread::OnLoop() - Thread 1 - On CPU 0, Counter = 0
MyFirstThread::OnLoop() - Thread 1 - On CPU 0, Counter = 1
MyFirstThread::OnLoop() - Thread 1 - On CPU 0, Counter = 2
MyFirstThread::OnLoop() - Thread 1 - On CPU 0, Counter = 3
```
### What about the `loop()` method?
Your existing `loop()` method will continue to operate exactly as it always has. On the ESP32, the default `loop()` method executes on CPU 1, while you will notice that your instance of `MyFirstThread` (`thread1` in the above sample code) is running on CPU 0.

### The sample code so far...
To make it easier to refer up and down, let's combine all of the code together now:
```cpp
#include <ESPressio_Thread.hpp>

using namespace ESPressio::Threads;

class MyFirstThread : public Thread {
    private:
        int _counter = 0;
    protected:
        void OnInitialization() override {
            // Anything we need to do here prior to the Thread's Loop sstarting
        }

        void OnLoop() override {
            _counter++; // Increment the counter

            // Let's display some information about our Thread...
            Serial.printf("MyFirstThread::OnLoop() - Thread #%d - On CPU %d, Counter = %d", GetThreadID(), xPortGetCoreID(), _counter);

            delay(1000); // Let's let this Thread wait for 1 second before it loops around again
        }
};

MyFirstThread thread1;

void setup() {
    Serial.begin(115200);

    delay(500); // Small delay just so that the thread doesn't start before the Serial Monitor is ready

    thread1.Initialize();
}
```

### Multiple Threads? No Problem!
So we've created one separate thread (ideally to execute on a separate CPU Core from the default application thread)... but what if we want more threads?

That's really not a problem.

Let's modify the previous example to create multiple Threads:
```cpp
MyFirstThread thread1, thread2, thread3;

void setup() {
    Serial.begin(115200);

    delay(500); // Small delay just so that the thread doesn't start before the Serial Monitor is ready

    thread1.Initialize();
    thread2.Initialize();
    thread3.Initialize();
}
```

We've now added two additional threads, so our output will look something like this:
```
MyFirstThread::OnLoop() - Thread 1 - On CPU 0, Counter = 1
MyFirstThread::OnLoop() - Thread 2 - On CPU 1, Counter = 1
MyFirstThread::OnLoop() - Thread 3 - On CPU 0, Counter = 1
MyFirstThread::OnLoop() - Thread 1 - On CPU 0, Counter = 2
MyFirstThread::OnLoop() - Thread 3 - On CPU 0, Counter = 2
MyFirstThread::OnLoop() - Thread 2 - On CPU 1, Counter = 2
```

The explicit maximum number of *ESPressio* `Thread`s supported by the library is 256, however the *practical limit* depends entirely on the specifications of your microcontroller. It's almost certainly going to be considerably lower than 256!

### The Thread Manager
In the previous example, you'll see that we manually called `Initialize()` on each instance of `MyFirstThread`.

Well, *ESPressio* Threads provides a central Thread `Manager`, and all of your `Thread` instances automatically register themselves with this `Manager`.

This neans we can `Initialize()` all of our `Thread` Instances in a single command!

First we need to make sure we include the `Thread` `Manager`'s header in our program:
```cpp
#include <ESPressio_ThreadManager.hpp>
```
Now we can modify the previous code example accordingly:
```cpp
MyFirstThread thread1, thread2, thread3;

void setup() {
    Serial.begin(115200);

    delay(500); // Small delay just so that the thread doesn't start before the Serial Monitor is ready

    Manager::Initialize();
}
```
Now, all three of our `MyFirstThread` instances will start exactly as they did before, but we didn't have to explicitly `Initialize()` each of them separately.

### Automated Garbage Collection
It is quite common to have `Thread`s with non-permanent lifetimes, such as *Worker Threads* (less common with microcontrollers, but not unheard of).

*ESPressio* Threads provides a means of leveraging fully-automatic *Garbage Collection* for your `Thread`s once they've `Terminated`.

Let's modify our previous example to take advantage of it, and let's add some *finality* to `MyFirstThread` so that it will automatically `Terminate` when it has done its "work":
```cpp
class MyFirstThread : public Thread {
    private:
        int _counter = 0;
    protected:
        void OnInitialization() override {
            // Anything we need to do here prior to the Thread's Loop sstarting
        }

        void OnLoop() override {
            _counter++; // Increment the counter

            // Let's display some information about our Thread...
            Serial.printf("MyFirstThread::OnLoop() - Thread #%d - On CPU %d, Counter = %d", GetThreadID(), xPortGetCoreID(), _counter);

            if (_counter == 10) {
                Termiante(); // This will Terminate the Thread
            }

            delay(1000); // Let's let this Thread wait for 1 second before it loops around again
        }
    public:
        MyFirstThread(bool freeOnTerminate) : Thread(freeOnTerminate) {}
};
```
Okay, so our `MyFirstThread` class has been updated so that it will automatically `Terminate` when the `_counter` reaches `10`.

I've also added a public *Constructor* to expose the overloaded constructor on `Thread`, which provides the optional `freeOnTerminate` parameter we shall be using in a moment.

Let's modify the way we define the *Instances* of `MyFirstThread` so that we can leverage Automatic Garbage Collection:
```cpp
MyFirstThread* thread1, thread2, thread3;

void setup() {
    Serial.begin(115200);

    // Create our Threads (passing `true` to the constructor for "FreeOnTerminate")
    thread1 = new MyFirstThread(true);
    thread2 = new MyFirstThread(true);
    thread3 = new MyFirstThread(true);

    delay(500); // Small delay just so that the thread doesn't start before the Serial Monitor is ready

    Manager::Initialize();
}
```
Now, when you run this program, each of the three instances of `MyFirstThread` will loop precisely 10 times, output their entries to the Serial console, then each of them will automatically `Terminate()`.

At that moment, the *Automatic Garbage Collector* will be awoken, and will take responsibility for purging the unwanted instances from our device's active memory.

>It is important to understand when it's appropriate to take advantage of Automatic Garbage Collection, and when you should manually manage the memory of your `Thread`s.

It's also good to know that the *Automatic Garbage Collector* is a "good citizen" and doesn't take up undue memory or clock cycles when it doesn't have any garbage to collect.