#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#ifndef ESPRESSIO_THREAD_TERMINATION_DISPATCHER_STACK_SIZE
    #define ESPRESSIO_THREAD_TERMINATION_DISPATCHER_STACK_SIZE 2000
#endif

#ifndef ESPRESSIO_THREAD_TERMINATION_DISPATCHER_PRIORITY
    #define ESPRESSIO_THREAD_TERMINATION_DISPATCHER_PRIORITY 2
#endif
#ifndef ESPRESSIO_THREAD_TERMINATION_QUEUE_LENGTH
    #define ESPRESSIO_THREAD_TERMINATION_QUEUE_LENGTH 256
#endif

namespace ESPressio {

    namespace Threads {

        class Thread;

        class ThreadTerminationDispatcher {
            private:
                QueueHandle_t _queue = nullptr;
                TaskHandle_t _taskHandle = nullptr;

                ThreadTerminationDispatcher();

                static void _taskEntry(void* parameter);
                void _loop();
            public:
                ThreadTerminationDispatcher(
                    const ThreadTerminationDispatcher&
                ) = delete;
                ThreadTerminationDispatcher& operator=(
                    const ThreadTerminationDispatcher&
                ) = delete;

                static ThreadTerminationDispatcher* GetInstance();

                bool IsAvailable() const;
                bool IsCurrentTask() const;
                bool Dispatch(Thread* thread);
        };

    }
}
