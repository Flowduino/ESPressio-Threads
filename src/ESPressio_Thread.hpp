#pragma once

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
        class Thread : public IThread, public Object<Thread> {
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
                TaskHandle_t _taskHandle = NULL; // SHOULD be Atomic!
                ReadWriteMutex<uint32_t> _stackSize = ReadWriteMutex<uint32_t>(ESPRESSIO_THREAD_DEFAULT_STACK_SIZE);
                ReadWriteMutex<UBaseType_t> _priority = ReadWriteMutex<UBaseType_t>(2);
                ReadWriteMutex<BaseType_t> _coreID = ReadWriteMutex<BaseType_t>(0);
            // Callbacks
                TOnThreadEvent _onInitialize = nullptr;
                TOnThreadEvent _onStart = nullptr;
                TOnThreadEvent _onPause = nullptr;
                TOnThreadEvent _onTerminate = nullptr;
                TOnThreadStateChangeEvent _onStateChange = nullptr;

            // Methods
                void _deleteTask() {
                    if (_taskHandle != NULL) {
                        TaskHandle_t handle = _taskHandle;
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

                virtual ~Thread() override;

            // Methods
                void GarbageCollect();

                void Initialize() {
                    if (_taskHandle != NULL) { vTaskResume(_taskHandle); } // Resume existing Task if it exists...
                    else { // ... or Create a new Task if it doesn't!
                        String threadIDStr = "thread" + String(GetThreadID());
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

                void Terminate() {
                    SetThreadState(ThreadState::Terminating);
                }

                void Start() {
                    if (GetThreadState() == ThreadState::Terminated) {
                        Initialize();
                    }
                    SetThreadState(ThreadState::Running);
                    if (_onStart != nullptr) { _onStart(this); }
                }

                void Pause() {
                    SetThreadState(ThreadState::Paused);
                    if (_onPause != nullptr) { _onPause(this); }
                }

            // Getters

                BaseType_t GetCoreID() {
                    return _coreID.Get();
                }

                uint32_t GetStackSize() {
                    return _stackSize.Get();
                }

                UBaseType_t GetPriority() {
                    return _priority.Get();
                }

                uint8_t GetThreadID() {
                    return _threadID;
                }

                ThreadState GetThreadState() {
                    return _threadState.Get();
                }

                bool GetFreeOnTerminate() {
                    return _freeOnTerminate.Get();
                }

                bool GetStartOnInitialize() {
                    return _startOnInitialize.Get();
                }

            // Callback Getters

                std::function<void(IThread*)> GetOnInitialize() {
                    return _onInitialize;
                }

                std::function<void(IThread*)> GetOnStart() {
                    return _onStart;
                }

                std::function<void(IThread*)> GetOnPause() {
                    return _onPause;
                }

                std::function<void(IThread*)> GetOnTerminate() {
                    return _onTerminate;
                }

                std::function<void(IThread*, ThreadState, ThreadState)> GetOnStateChange() {
                    return _onStateChange;
                }

            // Setters

                void SetCoreID(BaseType_t value) {
                    _coreID.Set(value);
                }

                void SetStackSize(uint32_t value) {
                    _stackSize.Set(value);
                }

                void SetPriority(UBaseType_t value) {
                    _priority.Set(value);
                }

                void SetFreeOnTerminate(bool value) {
                    _freeOnTerminate.Set(value);
                }

                void SetStartOnInitialize(bool value) {
                    _startOnInitialize.Set(value);
                }

            // Callback Setters

                void SetOnInitialize(std::function<void(IThread*)> value) {
                    _onInitialize = value;
                }

                void SetOnStart(std::function<void(IThread*)> value) {
                    _onStart = value;
                }

                void SetOnPause(std::function<void(IThread*)> value) {
                    _onPause = value;
                }

                void SetOnTerminate(std::function<void(IThread*)> value) {
                    _onTerminate = value;
                }

                void SetOnStateChange(std::function<void(IThread*, ThreadState, ThreadState)> value) {
                    _onStateChange = value;
                }
        };

    }
}