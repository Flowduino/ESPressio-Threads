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
        class Thread : public IThread {
            private:
            // Type Definitions
                
                /// `TOnThreadEvent` is a function type that can be used to handle Thread events.
                using TOnThreadEvent = std::function<void(IThread*)>;

            // Members
                uint8_t _threadID; // This is idempotent so doesn't need a `Mutex` wrapper.
                ReadWriteMutex<ThreadState> _threadState = ReadWriteMutex<ThreadState>(ThreadState::Uninitialized);
                ReadWriteMutex<bool> _freeOnTerminate = ReadWriteMutex<bool>(false);
                ReadWriteMutex<bool> _startOnInitialize = ReadWriteMutex<bool>(true);
                TaskHandle_t _taskHandle = nullptr; // SHOULD be Atomic!
                ReadWriteMutex<uint32_t> _stackSize = ReadWriteMutex<uint32_t>(ESPRESSIO_THREAD_DEFAULT_STACK_SIZE);
                ReadWriteMutex<UBaseType_t> _priority = ReadWriteMutex<UBaseType_t>(2);
                ReadWriteMutex<BaseType_t> _coreID = ReadWriteMutex<BaseType_t>(0);
            // Callbacks
                TOnThreadEvent _onInitialize = nullptr;
                TOnThreadEvent _onStarte = nullptr;
                TOnThreadEvent _onPause = nullptr;
                TOnThreadEvent _onTerminate = nullptr;
                TOnThreadEvent _onDestroy = nullptr;

            // Methods
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
                                SetThreadState(ThreadState::Terminated);
                            case ThreadState::Terminated:
                                if (_taskHandle != nullptr) { vTaskDelete(_taskHandle); }
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
                    _threadState.Set(state);
                }
            public:


            // Constructor/Destructor
                Thread();

                Thread(bool freeOnTerminate) : Thread() {
                    SetFreeOnTerminate(freeOnTerminate);
                }

                ~Thread();

            // Methods
                void Initialize() {
                    if (_taskHandle != nullptr) { vTaskDelete(_taskHandle); } // Delete any existing task handle if it's there!
                    // Convert value of GetThreadID() to const char* for xTaskCreatePinnedToCore
                    char threadIDStr[3];
                    itoa(GetThreadID(), threadIDStr, 10);
                    
                    xTaskCreatePinnedToCore(
                        [](void* parameter) {
                            Thread* instance = static_cast<Thread*>(parameter);
                            instance->_loop();
                        },
                        threadIDStr,                /* Name of the task. */
                        GetStackSize(),                   /* Stack size of the task. */
                        this,                    /* Parameter of the task (class instance). */
                        GetPriority(),                       /* Priority of the task. */
                        &_taskHandle,            /* Task handle to keep track of the created task. */
                        GetCoreID()              /* Pin task to core 0. */
                    );
                    OnInitialization(); // Invoke any custom initialization behaviour before we change the state of the Thread
                    // Check if the state was changed to Terminating or Terminate during the OnInitialization() method
                    if (GetThreadState() == ThreadState::Terminating || GetThreadState() == ThreadState::Terminated) {
                        vTaskDelete(_taskHandle);
                        return;
                    }
                    SetThreadState(GetStartOnInitialize() ? ThreadState::Running : ThreadState::Initialized);
                    if (_onInitialize != nullptr) { _onInitialize(this); }
                }

                void Terminate();

                void Start() {
                    if (GetThreadState() == ThreadState::Terminated) {
                        Initialize();
                    }
                    SetThreadState(ThreadState::Running);
                    if (_onStarte != nullptr) { _onStarte(this); }
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

                std::function<void(IThread*)> GetOnInitialized() {
                    return _onInitialize;
                }

                std::function<void(IThread*)> GetOnStarted() {
                    return _onStarte;
                }

                std::function<void(IThread*)> GetOnPaused() {
                    return _onPause;
                }

                std::function<void(IThread*)> GetOnTerminated() {
                    return _onTerminate;
                }

                std::function<void(IThread*)> GetOnDestroying() {
                    return _onDestroy;
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

                void SetOnInitialized(std::function<void(IThread*)> value) {
                    _onInitialize = value;
                }

                void SetOnStarted(std::function<void(IThread*)> value) {
                    _onStarte = value;
                }

                void SetOnPaused(std::function<void(IThread*)> value) {
                    _onPause = value;
                }

                void SetOnTerminated(std::function<void(IThread*)> value) {
                    _onTerminate = value;
                }

                void SetOnDestroying(std::function<void(IThread*)> value) {
                    _onDestroy = value;
                }
        };

    }
}