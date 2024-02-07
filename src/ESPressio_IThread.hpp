#pragma once

// #define CORE_THREADING_DEBUG // Uncomment this to explicitly enable debugging for the threading module
#include <Arduino.h>
#include <cstdint>
#include <functional>

#include <ESPressio_Object.hpp>

namespace ESPressio {

    using namespace Base;

    namespace Threads {

        enum ThreadState {
            Uninitialized,
            Initialized,
            Running,
            Paused,
            Terminating,
            Terminated,
            Destroyed
        };

        /*
            `IThread` is a common Interface for all Thread Types provided by this library.
            You can use it to reference any Thread Type without knowing the actual type.
        */
        class IThread : public IObject {
            public:
            // Methods

                /// `Initialize` is invoked automatically for all Threads when the `ThreadManager` is initialized in your `main()` (or `setup()` for MCU projects) function.
                virtual void Initialize() = 0;

                /// `Terminate` is invoked automatically for all Threads when the `ThreadManager` is terminated in your `main()` (or `loop()` for MCU projects) function.
                /// You can, however, invoke it manually to terminate a Thread at any time!
                virtual void Terminate() = 0;

                /// `Start` will start the Thread loop if it is not already running.
                /// It will also Resume the thread if it is `Paused`.
                virtual void Start() = 0;

                /// `Pause` will pause the Thread loop if it is running.
                virtual void Pause() = 0;

            // Getters

                /// `GetCoreID` returns the ID of the Core the Thread is running on.
                virtual BaseType_t GetCoreID() = 0;

                /// `GetStackSize` returns the size of the Stack the Thread is using.
                virtual uint32_t GetStackSize() = 0;

                /// `GetPriority` returns the priority of the Thread.
                virtual UBaseType_t GetPriority() = 0;

                /// `GetThreadID` returns the unique ID of the Thread.
                virtual uint8_t GetThreadID() = 0;

                /// `GetThreadState` returns the current state of the Thread.
                virtual ThreadState GetThreadState() = 0;

                /// `GetFreeOnTerminate` returns whether this Thread should be freed from memory when it is terminated.
                virtual bool GetFreeOnTerminate() = 0;

                /// `GetStartOnInitialize` returns whether this Thread should start running when it is initialized.
                virtual bool GetStartOnInitialize() = 0;

            // Utility Getters

                bool IsRunning() { return GetThreadState() == ThreadState::Running; }

                bool IsPaused() { return GetThreadState() == ThreadState::Paused; }

                bool IsTerminating() { return GetThreadState() == ThreadState::Terminating; }

                bool IsTerminated() { return GetThreadState() == ThreadState::Terminated; }

            // Callback Getters

                /// `GetOnInitialized` returns the callback to be invoked when the Thread is initialized.
                virtual std::function<void(IThread*)> GetOnInitialize() = 0;

                /// `GetOnStarted` returns the callback to be invoked when the Thread is started.
                virtual std::function<void(IThread*)> GetOnStart() = 0;

                /// `GetOnPaused` returns the callback to be invoked when the Thread is paused.
                virtual std::function<void(IThread*)> GetOnPause() = 0;

                /// `GetOnTerminated` returns the callback to be invoked when the Thread is terminated.
                virtual std::function<void(IThread*)> GetOnTerminate() = 0;

                /// `GetOnStateChange` returns the callback to be invoked when the Thread's state changes.
                virtual std::function<void(IThread*, ThreadState, ThreadState)> GetOnStateChange() = 0;

            // Setters

                /// `SetCoreID` sets the ID of the Core the Thread should run on.
                virtual void SetCoreID(BaseType_t value) = 0;

                /// `SetStackSize` sets the size of the Stack the Thread should use.
                virtual void SetStackSize(uint32_t value) = 0;

                /// `SetPriority` sets the priority of the Thread.
                virtual void SetPriority(UBaseType_t value) = 0;

                /// `SetFreeOnTerminate` defines whether this Thread should be freed from memory when it is terminated. 
                virtual void SetFreeOnTerminate(bool value) = 0;

            // Callback Setters

                /// `SetOnInitialized` sets the callback to be invoked when the Thread is initialized.
                /// The callback function takes `IThread*` and ideally named `sender`.
                virtual void SetOnInitialize(std::function<void(IThread*)>) = 0;

                /// `SetOnStarted` sets the callback to be invoked when the Thread is started.
                /// The callback function takes `IThread*` and ideally named `sender`.
                virtual void SetOnStart(std::function<void(IThread*)>) = 0;

                /// `SetOnPaused` sets the callback to be invoked when the Thread is paused.
                /// The callback function takes `IThread*` and ideally named `sender`.
                virtual void SetOnPause(std::function<void(IThread*)>) = 0;

                /// `SetOnTerminated` sets the callback to be invoked when the Thread is terminated.
                /// The callback function takes `IThread*` and ideally named `sender`.
                virtual void SetOnTerminate(std::function<void(IThread*)>) = 0;

                /// `SetOnStateChange` sets the callback to be invoked when the Thread's state changes.
                /// The callback function takes `IThread*` and ideally named `sender`, `ThreadState` for the previous state and `ThreadState` for the new state.
                virtual void SetOnStateChange(std::function<void(IThread*, ThreadState, ThreadState)>) = 0;
        };

    }
}