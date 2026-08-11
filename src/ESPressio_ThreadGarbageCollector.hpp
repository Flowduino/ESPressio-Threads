#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "ESPressio_ThreadManager.hpp"
#include "ESPressio_IThreadGarbageCollector.hpp"

#ifndef ESPRESSIO_THREAD_GARBAGE_COLLECTOR_STACK_SIZE
    #define ESPRESSIO_THREAD_GARBAGE_COLLECTOR_STACK_SIZE 2000 // You are encouraged to determine the appropriate stack size for your application, and define it in your platformio.ini file.
#endif

#ifndef ESPRESSIO_THREAD_GARBAGE_COLLECTOR_PRIORITY
    #define ESPRESSIO_THREAD_GARBAGE_COLLECTOR_PRIORITY 2
#endif

namespace ESPressio {

    namespace Threads {

        class ThreadGarbageCollector : public IThreadGarbageCollector {
            private:
                SemaphoreHandle_t _semaphore = nullptr;
                TaskHandle_t _taskHandle = nullptr;

                ThreadGarbageCollector() {
                    _semaphore = xSemaphoreCreateBinary();

                    if (_semaphore == nullptr) {
                        return;
                    }

                    const BaseType_t result = xTaskCreate(
                        _taskEntry,
                        "threadGarbageCollector",
                        ESPRESSIO_THREAD_GARBAGE_COLLECTOR_STACK_SIZE,
                        this,
                        ESPRESSIO_THREAD_GARBAGE_COLLECTOR_PRIORITY,
                        &_taskHandle
                    );

                    if (result != pdPASS) {
                        vSemaphoreDelete(_semaphore);
                        _semaphore = nullptr;
                        _taskHandle = nullptr;
                    }
                }

                static void _taskEntry(void* parameter) {
                    ThreadGarbageCollector* collector =
                        static_cast<ThreadGarbageCollector*>(parameter);

                    if (collector != nullptr) {
                        collector->_loop();
                    }

                    vTaskDelete(nullptr);
                }

                void _loop() {
                    for (;;) {
                        if (xSemaphoreTake(_semaphore, portMAX_DELAY) ==
                            pdTRUE) {
                            try {
                                ThreadManager::GetInstance()->CleanUp();
                            } catch (...) {
                                // Keep infrastructure alive if cleanup of a
                                // custom IThread unexpectedly throws.
                            }
                        }
                    }
                }

            public:
                static ThreadGarbageCollector* GetInstance() {
                    static ThreadGarbageCollector instance;
                    return &instance;
                }

                ThreadGarbageCollector(const ThreadGarbageCollector&) = delete;
                ThreadGarbageCollector& operator=(
                    const ThreadGarbageCollector&
                ) = delete;
                
                void CleanUp() override {
                    if (_semaphore != nullptr && _taskHandle != nullptr) {
                        xSemaphoreGive(_semaphore);
                    }
                }
        };

    }
}
