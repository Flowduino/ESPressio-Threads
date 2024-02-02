#pragma once

#include "ESPressio_Thread.hpp"
#include "ESPressio_ThreadManager.hpp"

#ifndef ESPRESSIO_THREAD_GARBAGE_COLLECTOR_STACK_SIZE
    #define ESPRESSIO_THREAD_GARBAGE_COLLECTOR_STACK_SIZE 2000 // You are encouraged to determine the appropriate stack size for your application, and define it in your platformio.ini file.
#endif

namespace ESPressio {

    namsapce Threads {

        class ThreadGarbageCollector : Thread {
            private:
                // A semaphore for our loop to wait on (so that it doesn't consume CPU cycles when there's nothing to do).
                SemaphoreHandle_t _semaphore = xSemaphoreCreateBinary();
            protected:
                // Constructor
                ThreadGarbageCollector() : Thread() {
                    SetStackSize(ESPRESSIO_THREAD_GARBAGE_COLLECTOR_STACK_SIZE);
                    Initialize();
                    Start();
                }

                void OnLoop() {
                    xSemaphoreTake(_semaphore, portMAX_DELAY);
                    // Wait on the semaphore until we're told to clean up.
                    ThreadManager::GetInstance()->CleanUp();
                }
            public:
                static ThreadGarbageCollector* GetInstance() {
                    static ThreadGarbageCollector* instance = new ThreadGarbageCollector();
                    return instance;
                }
                
                void CleanUp() {
                    // Signal the semaphore to wake up the thread.
                    xSemaphoreGive(_semaphore);
                }
        };

    }
}