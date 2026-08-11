#pragma once

#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <stdexcept>
#include <utility>

namespace ESPressio {


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

        enum class ThreadInitializationStatus : uint8_t {
            Success,
            AlreadyInitialized,
            InvalidState,
            ExitSignalUnavailable,
            TerminationDispatcherUnavailable,
            TerminationDispatchPending,
            TaskCreationFailed,
            ConcurrentInitializationLost,
            TerminatedDuringInitialization,
            InitializationException
        };

        class ThreadException : public std::runtime_error {
            public:
                explicit ThreadException(const char* message)
                    : std::runtime_error(message) {}
        };

        class ThreadRegistrationException : public ThreadException {
            public:
                explicit ThreadRegistrationException(const char* message)
                    : ThreadException(message) {}
        };

        class ThreadLimitExceededException :
            public ThreadRegistrationException {
            public:
                ThreadLimitExceededException()
                    : ThreadRegistrationException(
                        "ESPressio Threads supports at most 256 registered Threads"
                    ) {}
        };

        class ThreadExecutionException : public ThreadException {
            private:
                std::exception_ptr _cause;

            public:
                explicit ThreadExecutionException(
                    std::exception_ptr cause
                ) : ThreadException("Thread OnLoop execution failed"),
                    _cause(std::move(cause)) {}

                const std::exception_ptr& GetCause() const noexcept {
                    return _cause;
                }

                void RethrowCause() const {
                    if (_cause != nullptr) {
                        std::rethrow_exception(_cause);
                    }
                }
        };

        /*
            `IThread` is a common Interface for all Thread Types provided by this library.
            You can use it to reference any Thread Type without knowing the actual type.
        */
        class IThread  {
            private:
                std::atomic<bool> _automaticCleanupClaimed{false};

            public:
            // Type Defs

                typedef std::function<void(IThread*)> ThreadCallback;
                typedef std::function<void(IThread*, ThreadState, ThreadState)> ThreadStateChangeCallback;
                typedef std::function<void(
                    IThread*,
                    ThreadInitializationStatus
                )> ThreadInitializationFailedCallback;
                typedef std::function<void(
                    IThread*,
                    std::exception_ptr
                )> ThreadExecutionFailedCallback;

            // Destructor

                IThread() = default;
                IThread(const IThread&) = delete;
                IThread& operator=(const IThread&) = delete;
                IThread(IThread&&) = delete;
                IThread& operator=(IThread&&) = delete;
                virtual ~IThread() {}            

            // Methods

                /// `Initialize` is invoked automatically for all Threads when the `ThreadManager` is initialized in your `main()` (or `setup()` for MCU projects) function.
                virtual ThreadInitializationStatus Initialize() = 0;

                /// `Terminate` is invoked automatically for all Threads when the `ThreadManager` is terminated in your `main()` (or `loop()` for MCU projects) function.
                /// You can, however, invoke it manually to terminate a Thread at any time!
                virtual void Terminate() = 0;

                /// `Start` will start the Thread loop if it is not already running.
                /// It will also Resume the thread if it is `Paused`.
                /// Its return value exposes initialization failures when a new
                /// task must be created.
                virtual ThreadInitializationStatus Start() = 0;

                /// `Pause` will pause the Thread loop if it is running.
                virtual void Pause() = 0;

                /// Atomically claims this object for manager-driven cleanup.
                /// The default preserves compatibility for custom IThread
                /// implementations that rely only on FreeOnTerminate.
                virtual bool TryClaimAutomaticCleanup() {
                    if (!GetFreeOnTerminate()) {
                        return false;
                    }

                    bool expected = false;
                    return _automaticCleanupClaimed.compare_exchange_strong(
                        expected,
                        true,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire
                    );
                }

            // Getters

                /// `GetCoreID` returns the ID of the Core the Thread is running on.
                virtual int GetCoreID() = 0;

                /// `GetStackSize` returns the size of the Stack the Thread is using.
                virtual uint32_t GetStackSize() = 0;

                /// `GetPriority` returns the priority of the Thread.
                virtual unsigned int GetPriority() = 0;

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

                /// `GetOnDestroy` returns the callback to be invoked when the Thread is destroyed.
                virtual ThreadCallback GetOnDestroy() = 0;

                /// `GetOnInitialized` returns the callback to be invoked when the Thread is initialized.
                virtual ThreadCallback GetOnInitialize() = 0;

                /// `GetOnStarted` returns the callback to be invoked when the Thread is started.
                virtual ThreadCallback GetOnStart() = 0;

                /// `GetOnPaused` returns the callback to be invoked when the Thread is paused.
                virtual ThreadCallback GetOnPause() = 0;

                /// `GetOnTerminate` returns the callback invoked when the Thread loop enters the Terminated state.
                virtual ThreadCallback GetOnTerminate() = 0;

                /// `GetOnTerminated` returns the callback invoked after the underlying task has completed termination.
                /// The default implementation preserves compatibility with existing IThread implementations.
                virtual ThreadCallback GetOnTerminated() { return nullptr; }

                /// Returns the callback invoked when Initialize() returns an
                /// outcome other than Success.
                virtual ThreadInitializationFailedCallback
                GetOnInitializationFailed() { return nullptr; }

                /// Returns the callback invoked when OnLoop() throws. The
                /// exception_ptr contains a ThreadExecutionException.
                virtual ThreadExecutionFailedCallback
                GetOnExecutionFailed() { return nullptr; }

                /// `GetOnStateChange` returns the callback to be invoked when the Thread's state changes.
                virtual ThreadStateChangeCallback GetOnStateChange() = 0;

            // Setters

                /// `SetCoreID` sets the ID of the Core the Thread should run on.
                virtual void SetCoreID(int value) = 0;

                /// `SetStackSize` sets the size of the Stack the Thread should use.
                virtual void SetStackSize(uint32_t value) = 0;

                /// `SetPriority` sets the priority of the Thread.
                virtual void SetPriority(unsigned int value) = 0;

                /// `SetFreeOnTerminate` defines whether this Thread should be freed from memory when it is terminated. 
                virtual void SetFreeOnTerminate(bool value) = 0;

                /// `SetStartOnInitialize` defines whether this Thread should start running when it is initialized.
                virtual void SetStartOnInitialize(bool value) = 0;

            // Callback Setters

                /// `SetOnDestroy` sets the callback to be invoked when the Thread is destroyed.
                /// The callback function takes `IThread*` and ideally named `sender`.
                virtual void SetOnDestroy(ThreadCallback) = 0;

                /// `SetOnInitialized` sets the callback to be invoked when the Thread is initialized.
                /// The callback function takes `IThread*` and ideally named `sender`.
                virtual void SetOnInitialize(ThreadCallback) = 0;

                /// `SetOnStarted` sets the callback to be invoked when the Thread is started.
                /// The callback function takes `IThread*` and ideally named `sender`.
                virtual void SetOnStart(ThreadCallback) = 0;

                /// `SetOnPaused` sets the callback to be invoked when the Thread is paused.
                /// The callback function takes `IThread*` and ideally named `sender`.
                virtual void SetOnPause(ThreadCallback) = 0;

                /// `SetOnTerminate` sets the callback invoked when the Thread loop enters the Terminated state.
                /// The callback function takes `IThread*` and ideally named `sender`.
                virtual void SetOnTerminate(ThreadCallback) = 0;

                /// `SetOnTerminated` sets the callback invoked after the underlying task has completed termination.
                /// The default implementation preserves compatibility with existing IThread implementations.
                virtual void SetOnTerminated(ThreadCallback) {}

                /// Sets the callback invoked when Initialize() returns an
                /// outcome other than Success.
                virtual void SetOnInitializationFailed(
                    ThreadInitializationFailedCallback
                ) {}

                /// Sets the callback invoked when OnLoop() throws.
                virtual void SetOnExecutionFailed(
                    ThreadExecutionFailedCallback
                ) {}

                /// `SetOnStateChange` sets the callback to be invoked when the Thread's state changes.
                /// The callback function takes `IThread*` and ideally named `sender`, `ThreadState` for the previous state and `ThreadState` for the new state.
                virtual void SetOnStateChange(ThreadStateChangeCallback) = 0;
        };

    }
}
