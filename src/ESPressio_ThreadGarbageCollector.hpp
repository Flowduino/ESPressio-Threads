#pragma once

#include "ESPressio_Thread.hpp"
#include "ESPressio_ThreadManager.hpp"

namespace ESPressio {

    class ThreadGarbageCollector : Thread {
        private:
            // A semaphore for our loop to wait on (so that it doesn't consume CPU cycles when there's nothing to do).
            SemaphoreHandle_t _semaphore = xSemaphoreCreateBinary();
        protected:
            // Constructor
            ThreadGarbageCollector() : Thread() {
                // SetStackSize(2000);
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