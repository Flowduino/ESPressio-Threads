#include "ESPressio_ThreadTerminationDispatcher.hpp"
#include "ESPressio_Thread.hpp"

namespace ESPressio {

    namespace Threads {

        ThreadTerminationDispatcher::ThreadTerminationDispatcher() {
            _queue = xQueueCreate(
                ESPRESSIO_THREAD_TERMINATION_QUEUE_LENGTH,
                sizeof(Thread*)
            );

            if (_queue == nullptr) {
                return;
            }
            const BaseType_t result = xTaskCreate(
                _taskEntry,
                "threadTerminationDispatcher",
                ESPRESSIO_THREAD_TERMINATION_DISPATCHER_STACK_SIZE,
                this,
                ESPRESSIO_THREAD_TERMINATION_DISPATCHER_PRIORITY,
                &_taskHandle
            );

            if (result != pdPASS) {
                vQueueDelete(_queue);
                _queue = nullptr;
                _taskHandle = nullptr;
            }
        }
        void ThreadTerminationDispatcher::_taskEntry(void* parameter) {
            ThreadTerminationDispatcher* dispatcher =
                static_cast<ThreadTerminationDispatcher*>(parameter);

            if (dispatcher != nullptr) {
                dispatcher->_loop();
            }

            vTaskDelete(nullptr);
        }

        void ThreadTerminationDispatcher::_loop() {
            for (;;) {
                Thread* thread = nullptr;
                if (xQueueReceive(_queue, &thread, portMAX_DELAY) == pdTRUE &&
                    thread != nullptr) {
                    thread->_dispatchTermination();
                }
            }
        }

        ThreadTerminationDispatcher*
        ThreadTerminationDispatcher::GetInstance() {
            static ThreadTerminationDispatcher instance;
            return &instance;
        }
        bool ThreadTerminationDispatcher::IsAvailable() const {
            return _queue != nullptr && _taskHandle != nullptr;
        }

        bool ThreadTerminationDispatcher::IsCurrentTask() const {
            return _taskHandle != nullptr &&
                   xTaskGetCurrentTaskHandle() == _taskHandle;
        }

        bool ThreadTerminationDispatcher::Dispatch(Thread* thread) {
            if (!IsAvailable() || thread == nullptr) {
                return false;
            }
            // Dispatch is called from FreeRTOS task-deletion cleanup, where
            // waiting for queue capacity is unsafe.
            return xQueueSend(_queue, &thread, 0) == pdTRUE;
        }

    }
}
