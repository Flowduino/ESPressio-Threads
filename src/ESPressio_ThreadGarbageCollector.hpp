#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <exception>
#include <memory>
#include <mutex>

#include "ESPressio_ThreadManager.hpp"
#include "ESPressio_IThreadGarbageCollector.hpp"
#include "ESPressio_ThreadSafeObservable.hpp"

#ifndef ESPRESSIO_THREAD_GARBAGE_COLLECTOR_STACK_SIZE
    #define ESPRESSIO_THREAD_GARBAGE_COLLECTOR_STACK_SIZE 2000
#endif

#ifndef ESPRESSIO_THREAD_GARBAGE_COLLECTOR_PRIORITY
    #define ESPRESSIO_THREAD_GARBAGE_COLLECTOR_PRIORITY 2
#endif

namespace ESPressio {
namespace Threads {

    class ThreadGarbageCollector :
        public IThreadGarbageCollector {

    private:
        class GarbageCollectorObservable final :
            public Observable::ThreadSafeObservable {

        private:
            template <typename TCallback>
            void NotifyObservers(
                TCallback callback
            ) {
                ExecuteNotification(
                    [&](NotificationContext& notification) {
                        notification.WithObservers<
                            IThreadGarbageCollectorObserver
                        >(
                            [&](IThreadGarbageCollectorObserver* observer) {
                                try {
                                    callback(observer);
                                } catch (...) {
                                    // Infrastructure observers are diagnostic.
                                }
                            }
                        );
                    }
                );
            }

        public:
            void Initialized(
                bool available
            ) {
                NotifyObservers(
                    [&](IThreadGarbageCollectorObserver* observer) {
                        observer->
                            OnThreadGarbageCollectorInitialized(
                                available
                            );
                    }
                );
            }

            void InitializationFailed() {
                NotifyObservers(
                    [&](IThreadGarbageCollectorObserver* observer) {
                        observer->
                            OnThreadGarbageCollectorInitializationFailed();
                    }
                );
            }

            void Requested(
                ThreadGarbageCollectionExecutionMode mode
            ) {
                NotifyObservers(
                    [&](IThreadGarbageCollectorObserver* observer) {
                        observer->
                            OnThreadGarbageCollectionRequested(
                                mode
                            );
                    }
                );
            }

            void Queued(
                const ThreadGarbageCollectionResult& result
            ) {
                NotifyObservers(
                    [&](IThreadGarbageCollectorObserver* observer) {
                        observer->
                            OnThreadGarbageCollectionQueued(
                                result
                            );
                    }
                );
            }

            void Coalesced(
                const ThreadGarbageCollectionResult& result
            ) {
                NotifyObservers(
                    [&](IThreadGarbageCollectorObserver* observer) {
                        observer->
                            OnThreadGarbageCollectionRequestCoalesced(
                                result
                            );
                    }
                );
            }

            void Started(
                const ThreadGarbageCollectionResult& result
            ) {
                NotifyObservers(
                    [&](IThreadGarbageCollectorObserver* observer) {
                        observer->
                            OnThreadGarbageCollectionStarted(
                                result
                            );
                    }
                );
            }

            void FallbackStarted(
                const ThreadGarbageCollectionResult& result
            ) {
                NotifyObservers(
                    [&](IThreadGarbageCollectorObserver* observer) {
                        observer->
                            OnThreadGarbageCollectionFallbackStarted(
                                result
                            );
                    }
                );
            }

            void Completed(
                const ThreadGarbageCollectionResult& result
            ) {
                NotifyObservers(
                    [&](IThreadGarbageCollectorObserver* observer) {
                        observer->
                            OnThreadGarbageCollectionCompleted(
                                result
                            );
                    }
                );
            }

            void Failed(
                const ThreadGarbageCollectionResult& result,
                std::exception_ptr cause
            ) {
                NotifyObservers(
                    [&](IThreadGarbageCollectorObserver* observer) {
                        observer->
                            OnThreadGarbageCollectionFailed(
                                result,
                                cause
                            );
                    }
                );
            }
        };


        SemaphoreHandle_t _semaphore = nullptr;
        TaskHandle_t _taskHandle = nullptr;

        mutable std::mutex
            _initializationMutex;

        std::shared_ptr<
            GarbageCollectorObservable
        > _observable =
            std::make_shared<
                GarbageCollectorObservable
            >();


        ThreadGarbageCollector() {
            const bool available =
                _initialize();

            if (available) {
                _observable->Initialized(true);
            } else {
                _observable->InitializationFailed();
            }
        }


        bool _initialize() {
            if (
                _semaphore != nullptr &&
                _taskHandle != nullptr
            ) {
                return true;
            }

            _semaphore =
                xSemaphoreCreateBinary();

            if (_semaphore == nullptr) {
                return false;
            }

            const BaseType_t result =
                xTaskCreate(
                    _taskEntry,
                    "threadGarbageCollector",
                    ESPRESSIO_THREAD_GARBAGE_COLLECTOR_STACK_SIZE,
                    this,
                    ESPRESSIO_THREAD_GARBAGE_COLLECTOR_PRIORITY,
                    &_taskHandle
                );

            if (result != pdPASS) {
                vSemaphoreDelete(
                    _semaphore
                );

                _semaphore = nullptr;
                _taskHandle = nullptr;

                return false;
            }

            return true;
        }


        static void _taskEntry(
            void* parameter
        ) {
            ThreadGarbageCollector* collector =
                static_cast<
                    ThreadGarbageCollector*
                >(parameter);

            if (collector != nullptr) {
                collector->_loop();
            }

            vTaskDelete(nullptr);
        }


        void _loop() {
            for (;;) {
                if (
                    xSemaphoreTake(
                        _semaphore,
                        portMAX_DELAY
                    ) != pdTRUE
                ) {
                    continue;
                }

                ThreadGarbageCollectionResult result;

                result.ExecutionMode =
                    ThreadGarbageCollectionExecutionMode::
                        AsynchronousWorker;

                result.InfrastructureAvailable =
                    true;

                result.RequestQueued =
                    true;

                _observable->Started(
                    result
                );

                try {
                    result.ManagerResult =
                        ThreadManager::
                            GetInstance()->
                            CleanUpWithResult();

                    result.Completed =
                        !result.ManagerResult.WasDeferred;

                    _observable->Completed(
                        result
                    );
                } catch (...) {
                    result.Failed = true;

                    _observable->Failed(
                        result,
                        std::current_exception()
                    );

                    /*
                     * A custom IThread failure must not terminate the
                     * infrastructure worker. A later request may succeed.
                     */
                }
            }
        }


    public:
        static ThreadGarbageCollector*
        GetInstance() {
            static ThreadGarbageCollector
                instance;

            return &instance;
        }


        ThreadGarbageCollector(
            const ThreadGarbageCollector&
        ) = delete;

        ThreadGarbageCollector&
        operator=(
            const ThreadGarbageCollector&
        ) = delete;


        bool IsAvailable()
            const override {
            std::lock_guard<
                std::mutex
            > lock(
                _initializationMutex
            );

            return
                _semaphore != nullptr &&
                _taskHandle != nullptr;
        }


        void CleanUp() override {
            SemaphoreHandle_t semaphore =
                nullptr;

            bool infrastructureAvailable =
                false;

            {
                std::lock_guard<
                    std::mutex
                > lock(
                    _initializationMutex
                );

                const bool wasAvailable =
                    _semaphore != nullptr &&
                    _taskHandle != nullptr;

                infrastructureAvailable =
                    _initialize();

                if (
                    !wasAvailable &&
                    infrastructureAvailable
                ) {
                    _observable->Initialized(
                        true
                    );
                }

                if (infrastructureAvailable) {
                    semaphore =
                        _semaphore;
                }
            }

            if (semaphore != nullptr) {
                _observable->Requested(
                    ThreadGarbageCollectionExecutionMode::
                        AsynchronousWorker
                );

                ThreadGarbageCollectionResult result;

                result.ExecutionMode =
                    ThreadGarbageCollectionExecutionMode::
                        AsynchronousWorker;

                result.InfrastructureAvailable =
                    true;

                result.RequestQueued =
                    xSemaphoreGive(
                        semaphore
                    ) == pdTRUE;

                if (result.RequestQueued) {
                    _observable->Queued(
                        result
                    );
                } else {
                    /*
                     * The binary semaphore is already pending. This request
                     * has been coalesced into the existing cleanup request.
                     */
                    _observable->Coalesced(
                        result
                    );
                }

                return;
            }

            _observable->Requested(
                ThreadGarbageCollectionExecutionMode::
                    SynchronousFallback
            );

            ThreadGarbageCollectionResult result;

            result.ExecutionMode =
                ThreadGarbageCollectionExecutionMode::
                    SynchronousFallback;

            result.InfrastructureAvailable =
                false;

            _observable->FallbackStarted(
                result
            );

            try {
                result.ManagerResult =
                    ThreadManager::
                        GetInstance()->
                        CleanUpWithResult();

                result.Completed =
                    !result.ManagerResult.WasDeferred;

                _observable->Completed(
                    result
                );
            } catch (...) {
                result.Failed = true;

                _observable->Failed(
                    result,
                    std::current_exception()
                );

                /*
                 * Preserve the existing cleanup contract: infrastructure
                 * failures do not propagate from a cleanup request.
                 */
            }
        }


        Observable::ObserverHandlePtr
        RegisterObserver(
            IThreadGarbageCollectorObserver* observer
        ) override {
            auto handle =
                _observable->RegisterObserver(
                    observer
                );

            if (observer != nullptr) {
                try {
                    if (IsAvailable()) {
                        observer->
                            OnThreadGarbageCollectorInitialized(
                                true
                            );
                    } else {
                        observer->
                            OnThreadGarbageCollectorInitializationFailed();
                    }
                } catch (...) {
                }
            }

            return handle;
        }


        void UnregisterObserver(
            IThreadGarbageCollectorObserver* observer
        ) override {
            _observable->UnregisterObserver(
                observer
            );
        }
    };

}
}
