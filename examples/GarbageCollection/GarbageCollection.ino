/*
    An example of ESPressio Threads' Garbage Collection system in action.
*/

#define ESPRESSIO_THREAD_DEFAULT_STACK_SIZE 1600

#include <Arduino.h>
#include <ESPressio_IThread.hpp>
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

            if (_counter == 10) {
                Terminate();
            }
        }
};

DemoThread* thread = nullptr;

void onThreadDestroyed(
    IThread* sender
) {
    Serial.printf(
        "Thread %u has been destroyed!\n",
        sender->GetThreadID()
    );
}

void setup() {
    Serial.begin(115200);

    thread = new DemoThread(true);

    thread->SetStartOnInitialize(
        true
    );

    thread->SetOnDestroy(
        onThreadDestroyed
    );

    ThreadManager::GetInstance()->
        Initialize();
}

void loop() {
}
