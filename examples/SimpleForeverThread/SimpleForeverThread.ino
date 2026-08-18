/*
    An extremely simple example of an ESPressio Thread loop.
*/

#define ESPRESSIO_THREAD_DEFAULT_STACK_SIZE 1600

#include <Arduino.h>
#include <ESPressio_Thread.hpp>
#include <ESPressio_ThreadManager.hpp>

using namespace ESPressio::Threads;

class DemoThread : public Thread {
    private:
        uint32_t _counter = 0;

    protected:
        void OnLoop() override {
            Serial.printf(
                "DemoThread: %u\n",
                _counter++
            );

            delay(1000);
        }
};

DemoThread thread;

void setup() {
    Serial.begin(115200);

    thread.SetStartOnInitialize(
        true
    );

    ThreadManager::GetInstance()->
        Initialize();
}

void loop() {
}
