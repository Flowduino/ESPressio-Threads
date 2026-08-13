#include <ESPressio_Thread.hpp>
#include <ESPressio_ThreadManager.hpp>

using namespace ESPressio;

class CountingThread final : public Threads::Thread {
    private:
        uint8_t _count = 0;

    protected:
        void OnLoop() override {
            Serial.printf("iteration %u\n", ++_count);
            if (_count == 3) {
                Terminate();
            }
            vTaskDelay(pdMS_TO_TICKS(250));
        }
};

class ThreadLifecycleLogger final : public Threads::IThreadObserver {
    public:
        void OnThreadStateChanged(
            Threads::IThread* thread,
            Threads::ThreadState oldState,
            Threads::ThreadState newState
        ) override {
            Serial.printf(
                "thread %u state %u -> %u\n",
                thread->GetThreadID(),
                static_cast<unsigned int>(oldState),
                static_cast<unsigned int>(newState)
            );
        }

        void OnThreadInitializationFailed(
            Threads::IThread* thread,
            Threads::ThreadInitializationStatus status
        ) override {
            Serial.printf(
                "thread %u initialization failed: %u\n",
                thread->GetThreadID(),
                static_cast<unsigned int>(status)
            );
        }

        void OnThreadTaskExited(Threads::IThread* thread) override {
            Serial.printf(
                "thread %u FreeRTOS task exited\n",
                thread->GetThreadID()
            );
        }
};

ThreadLifecycleLogger lifecycleLogger;
CountingThread countingThread;
Observable::ObserverHandlePtr lifecycleObserverHandle;

void setup() {
    Serial.begin(115200);
    lifecycleObserverHandle = countingThread.RegisterThreadObserver(
        &lifecycleLogger
    );
    Threads::ThreadManager::GetInstance()->Initialize();
}

void loop() {
    delay(1000);
}
