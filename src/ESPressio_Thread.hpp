#pragma once

#ifdef ARDUINO
    // Includes for Arduino environment
    #include <FreeRTOS.h>
    #include <task.h>
#else
    // Includes for ESP-IDF environment
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
#endif

#include <functional>

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
                void* _taskHandle = NULL; // SHOULD be Atomic!
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
                    if (_taskHandle != NULL) {
                        void* handle = _taskHandle;
                        _taskHandle = NULL;
                        vTaskDelete(handle);
                    }
                }
                
                void _loop() {
                    for (;;) {
                        switch (_threadState.Get()) {
                            case ThreadState::Paused:
                            case ThreadState::Initialized:
                            case ThreadState::Uninitialized:
                                delay(1);
                                break;
                            case ThreadState::Running:
                                OnLoop();
                                break;
                            case ThreadState::Terminating:
                            case ThreadState::Terminated:
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
                    if (_taskHandle != NULL) { vTaskResume(_taskHandle); } // Resume existing Task if it exists...
                    else { // ... or Create a new Task if it doesn't!
                        std::string threadIDStr = "thread" + std::to_string(GetThreadID());
                        xTaskCreatePinnedToCore(
                            [](void* parameter) {
                                Thread* instance = static_cast<Thread*>(parameter);
                                if (instance != nullptr) {
                                    instance->_loop();
                                    instance->SetThreadState(ThreadState::Terminated);
                                }
                                vTaskSuspend(NULL);
                            },
                            threadIDStr.c_str(),
                            GetStackSize(),
                            this,          
                            GetPriority(), 
                            &_taskHandle,  
                            GetCoreID()    
                        );
                    }
                    OnInitialization(); // Invoke any custom initialization behaviour before we change the state of the Thread
                    // Check if the state was changed to Terminating or Terminate during the OnInitialization() method
                    if (GetThreadState() == ThreadState::Terminating || GetThreadState() == ThreadState::Terminated) {
                        vTaskDelete(_taskHandle);
                        return;
                    }
                    SetThreadState(GetStartOnInitialize() ? ThreadState::Running : ThreadState::Initialized);
                    if (_onInitialize != nullptr) { _onInitialize(this); }
                }

                void Terminate() override {
                    SetThreadState(ThreadState::Terminating);
                }

                void Start() override {
                    if (GetThreadState() == ThreadState::Terminated) {
                        Initialize();
                    }
                    SetThreadState(ThreadState::Running);
                    if (_onStart != nullptr) { _onStart(this); }
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