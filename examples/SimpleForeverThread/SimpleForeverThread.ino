/*
    An extremely simple example of an ESPressio Thread loop that runs on either core of the ESP32.
*/

#define ESPRESSIO_THREAD_DEFAULT_STACK_SIZE = 1600 // This sets the default Stack Size for all Threads in the system.

#include <Arduino.h>

#include "ESPressio_Thread.hpp"
#include "ESPressio_ThreadManager.hpp"

using namespace ESPressio::Threads;

class DemoThread : public Thread {
    private:
        uint32_t _counter = 0;
    protected:
        void OnLoop() {
            Serial.printf("DemoThread: %u\n", _counter++);
            delay(1000);
        }
};

DemoThread thread;

void setup() {
    Serial.begin(115200);

    thread.SetStartOnInitialize(true);

    ThreadManager::Initialize(); // This will initialize ALL Thread instances in your code!
}