#include "ESPressio_ThreadTerminationDispatcher.hpp"
#include "ESPressio_Thread.hpp"

namespace ESPressio {
namespace Threads {

    namespace {

        ThreadManagerThreadSnapshot
        SnapshotThread(
            Thread* thread
        ) {
            ThreadManagerThreadSnapshot snapshot;

            if (thread == nullptr) {
                return snapshot;
            }

            snapshot.ThreadID =
                thread->GetThreadID();

            snapshot.CoreID =
                thread->GetCoreID();

            snapshot.State =
                thread->GetThreadState();

            snapshot.FreeOnTerminate =
                thread->GetFreeOnTerminate();

            snapshot.StartOnInitialize =
                thread->GetStartOnInitialize();

            return snapshot;
        }

    }


    ThreadTerminationDispatcher::
    ThreadTerminationDispatcher() {
        _queue =
            xQueueCreate(
                ESPRESSIO_THREAD_TERMINATION_QUEUE_LENGTH,
                sizeof(DispatchRecord)
            );

        if (_queue == nullptr) {
            _observable->Initialized(false);
            return;
        }

        const BaseType_t result =
            xTaskCreate(
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

            _observable->Initialized(false);
            return;
        }

        _observable->Initialized(true);
    }


    void ThreadTerminationDispatcher::
    _taskEntry(
        void* parameter
    ) {
        ThreadTerminationDispatcher* dispatcher =
            static_cast<
                ThreadTerminationDispatcher*
            >(parameter);

        if (dispatcher != nullptr) {
            dispatcher->_loop();
        }

        vTaskDelete(nullptr);
    }


    void ThreadTerminationDispatcher::
    _loop() {
        for (;;) {
            DispatchRecord record;

            if (
                xQueueReceive(
                    _queue,
                    &record,
                    portMAX_DELAY
                ) != pdTRUE ||
                record.ThreadPointer ==
                    nullptr
            ) {
                continue;
            }

            _observable->Started(
                record.Snapshot
            );

            record.ThreadPointer->
                _dispatchTermination();

            /*
             * Do not dereference the Thread after termination dispatch:
             * automatic GC can now own its eventual destruction.
             */
            _observable->Completed(
                record.Snapshot
            );
        }
    }


    ThreadTerminationDispatcher*
    ThreadTerminationDispatcher::
    GetInstance() {
        static ThreadTerminationDispatcher
            instance;

        return &instance;
    }


    bool ThreadTerminationDispatcher::
    IsAvailable() const {
        return
            _queue != nullptr &&
            _taskHandle != nullptr;
    }


    bool ThreadTerminationDispatcher::
    IsCurrentTask() const {
        return
            _taskHandle != nullptr &&
            xTaskGetCurrentTaskHandle() ==
                _taskHandle;
    }


    bool ThreadTerminationDispatcher::
    Dispatch(
        Thread* thread
    ) {
        if (
            !IsAvailable() ||
            thread == nullptr
        ) {
            return false;
        }

        DispatchRecord record;

        record.ThreadPointer =
            thread;

        record.Snapshot =
            SnapshotThread(thread);

        /*
         * Dispatch is called from FreeRTOS task-deletion cleanup, where
         * waiting for queue capacity is unsafe.
         */
        const bool queued =
            xQueueSend(
                _queue,
                &record,
                0
            ) == pdTRUE;

        if (queued) {
            _observable->Queued(
                record.Snapshot
            );
        } else {
            _observable->QueueFailed(
                record.Snapshot
            );
        }

        return queued;
    }

}
}
