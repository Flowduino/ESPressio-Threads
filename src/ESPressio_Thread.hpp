#pragma once

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <atomic>
#include <functional>
#include <string>

#include "ESPressio_IThread.hpp"
#include "ESPressio_ThreadSafe.hpp"

#ifndef ESPRESSIO_THREAD_DEFAULT_STACK_SIZE
    #define ESPRESSIO_THREAD_DEFAULT_STACK_SIZE 4000
#endif

namespace ESPressio {

    namespace Threads {

        // Forward Declaration for `ThreadGarbageCollector`
        class ThreadGarbageCollector;

        /*
            `Thread` is a class that represents a "standard" Thread in the system.
            It is a wrapper around the system's Thread API, designed to make them much easier to use.
        */
        class Thread : public IThread {
            private:
            // Type Definitions
                
                /// `TOnThreadEvent` is a function type that can be used to handle Thread events.
                using TOnThreadEvent = std::function<void(IThread*)>;
                /// `TOnThreadStateChangeEvent` is a function type that can be used to handle Thread state change events.
                using TOnThreadStateChangeEvent = std::function<void(IThread*, ThreadState, ThreadState)>;

            // Members
                uint8_t _threadID; // This is idempotent so doesn't need a `Mutex` wrapper.
                ReadWriteMutex<ThreadState> _threadState = ReadWriteMutex<ThreadState>(ThreadState::Uninitialized);
                ReadWriteMutex<bool> _freeOnTerminate = ReadWriteMutex<bool>(false);
                ReadWriteMutex<bool> _startOnInitialize = ReadWriteMutex<bool>(true);
                std::atomic<TaskHandle_t> _taskHandle{nullptr};
                ReadWriteMutex<uint32_t> _stackSize = ReadWriteMutex<uint32_t>(ESPRESSIO_THREAD_DEFAULT_STACK_SIZE);
                ReadWriteMutex<unsigned int> _priority = ReadWriteMutex<unsigned int>(2);
                ReadWriteMutex<int> _coreID = ReadWriteMutex<int>(0);
            // Callbacks
                TOnThreadEvent _onDestroy = nullptr;
                TOnThreadEvent _onInitialize = nullptr;
                TOnThreadEvent _onStart = nullptr;
                TOnThreadEvent _onPause = nullptr;
                TOnThreadEvent _onTerminate = nullptr;
                TOnThreadStateChangeEvent _onStateChange = nullptr;

            // Methods
                void _deleteTask() {
                    TaskHandle_t handle =
                        _taskHandle.exchange(nullptr, std::memory_order_acq_rel);

                    if (handle != nullptr) {
                        vTaskDelete(handle);
                    }
                }
                
                void _loop() {
                    for (;;) {
                        switch (_threadState.Get()) {
                            case ThreadState::Paused:
                            case ThreadState::Initialized:
                            case ThreadState::Uninitialized:
                                vTaskDelay(pdMS_TO_TICKS(1));
                                break;
                            case ThreadState::Running:
                                OnLoop();
                                break;
                            case ThreadState::Terminating:
                            case ThreadState::Terminated:
                            case ThreadState::Destroyed:
                                return;
                        }
                    }
                }
            protected:
            // Methods

                /// Override `OnLoop` to provide the main loop for the Thread.
                virtual void OnLoop() {}

                /// Override `OnInitialization` to perform any setup required for the Thread before the Loop begins.
                virtual void OnInitialization() {}

            // Getters (Internal)

            

            // Setters (Internal)
                
                void SetThreadState(ThreadState state) {
                    ThreadState oldState = _threadState.Get();
                    if (oldState == state) { return; }
                    _threadState.Set(state);
                    if (_onStateChange != nullptr) { _onStateChange(this, oldState, state); }
                    switch (state) {
                        case ThreadState::Terminated:
                            GarbageCollect();
                            break;
                        case ThreadState::Terminating:
                            if (_onTerminate != nullptr) { _onTerminate(this); }
                            break;
                        case ThreadState::Paused:
                            if (_onPause != nullptr) { _onPause(this); }
                            break;
                        case ThreadState::Running:
                            if (_onStart != nullptr) { _onStart(this); }
                            break;
                        case ThreadState::Initialized:
                            if (_onInitialize != nullptr) { _onInitialize(this); }
                            break;
                        case ThreadState::Destroyed:
                        case ThreadState::Uninitialized:
                            // Do nothing (yet)
                            break;
                    
                    }
                }
            public:


            // Constructor/Destructor
                Thread();

                Thread(bool freeOnTerminate) : Thread() {
                    SetFreeOnTerminate(freeOnTerminate);
                }

                virtual ~Thread();

            // Methods
                void GarbageCollect();

                void Initialize() override {
                    if (_taskHandle.load(std::memory_order_acquire) != nullptr) {
                        return;
                    }

                    // A replacement task must not observe Terminated and exit
                    // before Initialize() has finished publishing its handle.
                    if (GetThreadState() == ThreadState::Terminated) {
                        _threadState.Set(ThreadState::Uninitialized);
                    }

                    std::string threadName =
                        "thread" + std::to_string(GetThreadID());

                    TaskHandle_t createdTask = nullptr;

                    const BaseType_t result = xTaskCreatePinnedToCore(
                        [](void* parameter) {
                            Thread* instance = static_cast<Thread*>(parameter);

                            // Do not enter the Thread lifecycle until Initialize()
                            // has published the handle and completed setup.
                            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

                            if (instance != nullptr) {
                                instance->_loop();

                                const TaskHandle_t currentTask =
                                    xTaskGetCurrentTaskHandle();
                                TaskHandle_t expected = currentTask;

                                instance->_taskHandle.compare_exchange_strong(
                                    expected,
                                    nullptr,
                                    std::memory_order_acq_rel,
                                    std::memory_order_acquire
                                );

                                instance->SetThreadState(ThreadState::Terminated);
                            }
                            vTaskDelete(nullptr);
                        },
                        threadName.c_str(),
                        GetStackSize(),
                        this,
                        GetPriority(),
                        &createdTask,
                        GetCoreID()
                    );

                    if (result != pdPASS) {
                        return;
                    }

                    TaskHandle_t expected = nullptr;

                    if (!_taskHandle.compare_exchange_strong(
                            expected,
                            createdTask,
                            std::memory_order_release,
                            std::memory_order_acquire)) {
                        vTaskDelete(createdTask);
                        return;
                    }

                    OnInitialization();

                    const ThreadState stateAfterInitialization = GetThreadState();

                    if (stateAfterInitialization == ThreadState::Terminating) {
                        _deleteTask();
                        SetThreadState(ThreadState::Terminated);
                        return;
                    }

                    if (stateAfterInitialization == ThreadState::Terminated) {
                        _deleteTask();
                        return;
                    }

                    SetThreadState(
                        GetStartOnInitialize()
                            ? ThreadState::Running
                            : ThreadState::Initialized
                    );

                    xTaskNotifyGive(createdTask);
                }

                void Terminate() override {
                    SetThreadState(ThreadState::Terminating);
                }

                void Start() override {

                    switch (GetThreadState()) {
                        case ThreadState::Uninitialized:
                        case ThreadState::Terminated:
                            Initialize();

                            if (GetThreadState() == ThreadState::Initialized) {
                                SetThreadState(ThreadState::Running);
                            }
                            return;

                        case ThreadState::Initialized:
                        case ThreadState::Paused:
                            SetThreadState(ThreadState::Running);
                            return;

                        case ThreadState::Running:
                        case ThreadState::Terminating:
                        case ThreadState::Destroyed:
                            return;
                    }
                }

                void Pause() override {
                    SetThreadState(ThreadState::Paused);
                    if (_onPause != nullptr) { _onPause(this); }
                }

            // Getters

                int GetCoreID() override {
                    return _coreID.Get();
                }

                uint32_t GetStackSize() override {
                    return _stackSize.Get();
                }

                unsigned int GetPriority() override {
                    return _priority.Get();
                }

                uint8_t GetThreadID() override {
                    return _threadID;
                }

                ThreadState GetThreadState() override {
                    return _threadState.Get();
                }

                bool GetFreeOnTerminate() override {
                    return _freeOnTerminate.Get();
                }

                bool GetStartOnInitialize() override {
                    return _startOnInitialize.Get();
                }

            // Callback Getters

                TOnThreadEvent GetOnDestroy() override {
                    return _onDestroy;
                }

                TOnThreadEvent GetOnInitialize() override {
                    return _onInitialize;
                }

                TOnThreadEvent GetOnStart() override {
                    return _onStart;
                }

                TOnThreadEvent GetOnPause() override {
                    return _onPause;
                }

                TOnThreadEvent GetOnTerminate() override {
                    return _onTerminate;
                }

                TOnThreadStateChangeEvent GetOnStateChange() override {
                    return _onStateChange;
                }

            // Setters

                void SetCoreID(int value) override {
                    _coreID.Set(value);
                }

                void SetStackSize(uint32_t value) override {
                    _stackSize.Set(value);
                }

                void SetPriority(unsigned int value) override {
                    _priority.Set(value);
                }

                void SetFreeOnTerminate(bool value) override {
                    _freeOnTerminate.Set(value);
                }

                void SetStartOnInitialize(bool value) override {
                    _startOnInitialize.Set(value);
                }

            // Callback Setters

                void SetOnDestroy(TOnThreadEvent value) override {
                    _onDestroy = value;
                }

                void SetOnInitialize(TOnThreadEvent value) override {
                    _onInitialize = value;
                }

                void SetOnStart(TOnThreadEvent value) override {
                    _onStart = value;
                }

                void SetOnPause(TOnThreadEvent value) override {
                    _onPause = value;
                }

                void SetOnTerminate(TOnThreadEvent value) override {
                    _onTerminate = value;
                }

                void SetOnStateChange(TOnThreadStateChangeEvent value) override {
                    _onStateChange = value;
                }
        };

    }
}
