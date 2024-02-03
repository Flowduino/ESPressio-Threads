/*
    An example of ESPressio Threads' Garbage Collection system in action.
*/

#define ESPRESSIO_THREAD_DEFAULT_STACK_SIZE = 1600 // This sets the default Stack Size for all Threads in the system.

#include <Arduino.h>

#include "ESPressio_IThread.hpp" // This gives us access to the `IThread` interface.
#include "ESPressio_Thread.hpp" // This gives us access to our `Thread` base class.
#include "ESPressio_ThreadManager.hpp" // This gives us access to the `ThreadManager` class.

using namespace ESPressio::Threads;

class DemoThread : public Thread {
    private:
        uint32_t _counter = 0;
    protected:
        void OnLoop() {
            Serial.printf("DemoThread: %u\n", _counter++);
            delay(1000);
            if (counter == 10) { // When the counter reaches 10...
                Terminate(); // ... terminate the thread.
            }
        }
};

DemoThread* thread;

// This function will be called when the thread is destroyed.
void onThreadDestroyed(IThread* thread) {
    Serial.printf("Thread %u has been destroyed!\n", thread->GetThreadID()); // Print a message to the Serial Monitor.
}

void setup() {
    Serial.begin(115200); // Start the Serial Monitor.

    thread = new DemoThread(true); // Create a new instance of our `DemoThread` class. The `true` parameter tells the Thread to use Garbage Collection when it's Terminated!

    thread.SetStartOnInitialize(true); // This will start the thread as soon as it's initialized. (true is the default, but we're setting it here for clarity.)
    thread.SetOnDestroy(onThreadDestroyed); // This will set the `onThreadDestroyed` function as the callback for when the thread is destroyed.

    ThreadManager::Initialize(); // This will initialize ALL Thread instances in your code!
}

void loop() {
    // You can still use your main loop as normal, if you want to!
}