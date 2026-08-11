#pragma once

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

#include "ESPressio_IThread.hpp"
#include "ESPressio_ThreadSafe.hpp"

#ifndef ESPRESSIO_THREAD_DEFAULT_STACK_SIZE
    #define ESPRESSIO_THREAD_DEFAULT_STACK_SIZE 4000
#endif

#ifndef ESPRESSIO_THREAD_TLS_INDEX
    #define ESPRESSIO_THREAD_TLS_INDEX 0
#endif

#if defined(configNUM_THREAD_LOCAL_STORAGE_POINTERS)
    static_assert(
        ESPRESSIO_THREAD_TLS_INDEX >= 0 &&
        ESPRESSIO_THREAD_TLS_INDEX < configNUM_THREAD_LOCAL_STORAGE_POINTERS,
        "ESPRESSIO_THREAD_TLS_INDEX is outside the configured FreeRTOS TLS range"
    );
#endif

namespace ESPressio {

    namespace Threads {

        // Forward Declaration for `ThreadGarbageCollector`
        class ThreadGarbageCollector;
        class ThreadTerminationDispatcher;

        /*
            `Thread` is a class that represents a "standard" Thread in the system.
            It is a wrapper around the system's Thread API, designed to make them much easier to use.
        */
        class Thread : public IThread {
            private:
                enum class CleanupClaim : uint8_t {
                    Available,
                    Manual,
                    Automatic
                };

            // Type Definitions
                
                /// `TOnThreadEvent` is a function type that can be used to handle Thread events.
                using TOnThreadEvent = std::function<void(IThread*)>;
                /// `TOnThreadStateChangeEvent` is a function type that can be used to handle Thread state change events.
                using TOnThreadStateChangeEvent = std::function<void(IThread*, ThreadState, ThreadState)>;
                using TOnThreadInitializationFailedEvent = std::function<void(
                    IThread*,
                    ThreadInitializationStatus
                )>;
                using TOnThreadExecutionFailedEvent = std::function<void(
                    IThread*,
                    std::exception_ptr
                )>;

            // Members
                uint8_t _threadID; // This is idempotent so doesn't need a `Mutex` wrapper.
                ReadWriteMutex<ThreadState> _threadState = ReadWriteMutex<ThreadState>(ThreadState::Uninitialized);
                ReadWriteMutex<bool> _freeOnTerminate = ReadWriteMutex<bool>(false);
                ReadWriteMutex<bool> _startOnInitialize = ReadWriteMutex<bool>(true);
                std::atomic<TaskHandle_t> _taskHandle{nullptr};
                std::atomic<TaskHandle_t> _initializingTaskHandle{nullptr};
                std::atomic<bool> _initializationInProgress{false};
                std::atomic<bool> _terminationDispatchPending{false};
                std::atomic<CleanupClaim> _cleanupClaim{
                    CleanupClaim::Available
                };
                SemaphoreHandle_t _taskExited = xSemaphoreCreateBinary();
                mutable std::mutex _taskConfigurationMutex;
                mutable std::recursive_mutex _stateTransitionMutex;
                ReadWriteMutex<uint32_t> _stackSize = ReadWriteMutex<uint32_t>(ESPRESSIO_THREAD_DEFAULT_STACK_SIZE);
                ReadWriteMutex<unsigned int> _priority = ReadWriteMutex<unsigned int>(2);
                ReadWriteMutex<int> _coreID = ReadWriteMutex<int>(0);
            // Callbacks
                mutable std::mutex _callbackMutex;
                TOnThreadEvent _onDestroy = nullptr;
                TOnThreadEvent _onInitialize = nullptr;
                TOnThreadEvent _onStart = nullptr;
                TOnThreadEvent _onPause = nullptr;
                TOnThreadEvent _onTerminate = nullptr;
                TOnThreadEvent _onTerminated = nullptr;
                TOnThreadInitializationFailedEvent _onInitializationFailed =
                    nullptr;
                TOnThreadExecutionFailedEvent _onExecutionFailed = nullptr;
                TOnThreadStateChangeEvent _onStateChange = nullptr;

            // Methods
                bool _isValidThreadStateTransition(
                    ThreadState oldState,
                    ThreadState newState
                ) {
                    if (oldState == newState) {
                        return false;
                    }

                    if (newState == ThreadState::Destroyed) {
                        return oldState != ThreadState::Destroyed;
                    }

                    switch (oldState) {
                        case ThreadState::Uninitialized:
                            return newState == ThreadState::Initialized ||
                                   newState == ThreadState::Terminating;
                        case ThreadState::Initialized:
                            return newState == ThreadState::Running ||
                                   newState == ThreadState::Terminating;
                        case ThreadState::Running:
                            return newState == ThreadState::Paused ||
                                   newState == ThreadState::Terminating;
                        case ThreadState::Paused:
                            return newState == ThreadState::Running ||
                                   newState == ThreadState::Terminating;
                        case ThreadState::Terminating:
                            return newState == ThreadState::Terminated;
                        case ThreadState::Terminated:
                            return newState == ThreadState::Uninitialized;
                        case ThreadState::Destroyed:
                            return false;
                    }

                    return false;
                }

                void _deleteTask() {
                    TaskHandle_t handle =
                        _taskHandle.exchange(nullptr, std::memory_order_acq_rel);

                    if (handle != nullptr) {
                        vTaskDelete(handle);
                    }
                }

                static void _requestGarbageCollection();
                static bool _isTerminationDispatcherAvailable();
                static bool _isCurrentTerminationDispatcherTask();
                static bool _queueTerminationDispatch(Thread* thread);
                void _dispatchTermination();

                void _dispatchExecutionFailed(
                    std::exception_ptr cause
                ) noexcept {
                    try {
                        TOnThreadExecutionFailedEvent onExecutionFailed =
                            GetOnExecutionFailed();

                        if (onExecutionFailed != nullptr) {
                            onExecutionFailed(
                                this,
                                std::make_exception_ptr(
                                    ThreadExecutionException(
                                        std::move(cause)
                                    )
                                )
                            );
                        }
                    } catch (...) {
                        // Failure reporting must not unwind through the
                        // FreeRTOS task entry point.
                    }
                }

                void _waitForTerminationDispatch() {
                    if (!_terminationDispatchPending.load(
                        std::memory_order_acquire
                    )) {
                        return;
                    }

                    if (_isCurrentTerminationDispatcherTask()) {
                        return;
                    }

                    while (_terminationDispatchPending.load(
                        std::memory_order_acquire
                    )) {
                        const auto delayTicks = pdMS_TO_TICKS(1);
                        vTaskDelay(delayTicks > 0 ? delayTicks : 1);
                    }
                }
                
                void _loop() {
                    for (;;) {
                        switch (_threadState.Get()) {
                            case ThreadState::Paused:
                            case ThreadState::Initialized:
                            case ThreadState::Uninitialized:
                                {
                                    const auto delayTicks = pdMS_TO_TICKS(1);
                                    vTaskDelay(delayTicks > 0 ? delayTicks : 1);
                                }
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

                void _dispatchThreadStateChange(
                    ThreadState oldState,
                    ThreadState newState
                ) {
                    TOnThreadStateChangeEvent onStateChange;
                    TOnThreadEvent onThreadEvent;
                    bool callbackFailed = false;

                    {
                        std::lock_guard<std::mutex> lock(_callbackMutex);
                        onStateChange = _onStateChange;

                        switch (newState) {
                            case ThreadState::Terminated:
                                onThreadEvent = _onTerminate;
                                break;
                            case ThreadState::Paused:
                                onThreadEvent = _onPause;
                                break;
                            case ThreadState::Running:
                                onThreadEvent = _onStart;
                                break;
                            case ThreadState::Initialized:
                                onThreadEvent = _onInitialize;
                                break;
                            case ThreadState::Uninitialized:
                            case ThreadState::Terminating:
                            case ThreadState::Destroyed:
                                break;
                        }
                    }

                    if (onStateChange != nullptr) {
                        try {
                            onStateChange(this, oldState, newState);
                        } catch (...) {
                            // Lifecycle callbacks must not interrupt a state
                            // transition or prevent subsequent callbacks.
                            callbackFailed = true;
                        }
                    }

                    // A reentrant OnStateChange callback may have moved the
                    // Thread into another state. Do not emit a stale event for
                    // a state that is no longer current.
                    if (onThreadEvent != nullptr &&
                        GetThreadState() == newState) {
                        try {
                            onThreadEvent(this);
                        } catch (...) {
                            // Each callback is isolated so one handler cannot
                            // suppress the remainder of the lifecycle.
                            callbackFailed = true;
                        }
                    }

                    if (callbackFailed &&
                        _initializationInProgress.load(
                            std::memory_order_acquire
                        ) &&
                        _initializingTaskHandle.load(
                            std::memory_order_acquire
                        ) == xTaskGetCurrentTaskHandle()) {
                        // Initialize() converts this internal signal into
                        // InitializationException after cleaning up its gated
                        // worker task.
                        throw std::runtime_error(
                            "Thread initialization lifecycle callback failed"
                        );
                    }
                }
            protected:
            // Methods

                /// Override `OnLoop` to provide the main loop for the Thread.
                virtual void OnLoop() {
                    const auto delayTicks = pdMS_TO_TICKS(1);
                    vTaskDelay(delayTicks > 0 ? delayTicks : 1);
                }

                /// Override `OnInitialization` to perform any setup required for the Thread before the Loop begins.
                virtual void OnInitialization() {}

            // Getters (Internal)


            // Setters (Internal)

                void SetThreadState(ThreadState state) {
                    std::lock_guard<std::recursive_mutex> transitionLock(
                        _stateTransitionMutex
                    );
                    ThreadState oldState = state;
                    bool changed = false;

                    _threadState.WithWriteLock([&](ThreadState& currentState) {
                        if (!_isValidThreadStateTransition(currentState, state)) {
                            return;
                        }

                        oldState = currentState;
                        currentState = state;
                        changed = true;
                    });

                    if (changed) {
                        _dispatchThreadStateChange(oldState, state);
                    }
                }

                bool TrySetThreadState(
                    ThreadState expectedState,
                    ThreadState newState
                ) {
                    std::lock_guard<std::recursive_mutex> transitionLock(
                        _stateTransitionMutex
                    );
                    bool changed = false;

                    _threadState.WithWriteLock([&](ThreadState& currentState) {
                        if (currentState != expectedState ||
                            !_isValidThreadStateTransition(
                                currentState,
                                newState
                            )) {
                            return;
                        }

                        currentState = newState;
                        changed = true;
                    });

                    if (changed) {
                        _dispatchThreadStateChange(expectedState, newState);
                    }

                    return changed;
                }
            public:

                friend class ThreadTerminationDispatcher;


            // Constructor/Destructor
                Thread();

                Thread(bool freeOnTerminate) : Thread() {
                    SetFreeOnTerminate(freeOnTerminate);
                }

                virtual ~Thread();

            // Methods
                void GarbageCollect();

                /// Requests termination and waits until the FreeRTOS task no
                /// longer accesses this object. Derived destructors should call
                /// this before destroying members used by OnLoop().
                void Shutdown() {
                    CleanupClaim expectedClaim = CleanupClaim::Available;
                    _cleanupClaim.compare_exchange_strong(
                        expectedClaim,
                        CleanupClaim::Manual,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire
                    );
                    SetFreeOnTerminate(false);

                    // A caller running inside this Thread cannot wait for its
                    // own task deletion callback.
                    const TaskHandle_t handle =
                        _taskHandle.load(std::memory_order_acquire);
                    const TaskHandle_t currentTask =
                        xTaskGetCurrentTaskHandle();

                    // OnInitialization() must return before Initialize() can
                    // delete the still-gated worker and signal task exit.
                    if (_initializationInProgress.load(std::memory_order_acquire) &&
                        _initializingTaskHandle.load(std::memory_order_acquire) ==
                            currentTask) {
                        Terminate();
                        return;
                    }

                    if (handle == nullptr) {
                        if (GetThreadState() != ThreadState::Terminated &&
                            GetThreadState() != ThreadState::Destroyed) {
                            Terminate();
                            SetThreadState(ThreadState::Terminated);
                        }
                        _waitForTerminationDispatch();
                        return;
                    }

                    if (handle == currentTask) {
                        Terminate();
                        return;
                    }

                    Terminate();
                    xSemaphoreTake(_taskExited, portMAX_DELAY);
                    _waitForTerminationDispatch();
                }

            private:
                ThreadInitializationStatus _initialize() {
                    if (_taskExited == nullptr) {
                        return ThreadInitializationStatus::ExitSignalUnavailable;
                    }

                    if (_taskHandle.load(std::memory_order_acquire) != nullptr) {
                        return ThreadInitializationStatus::AlreadyInitialized;
                    }

                    if (_terminationDispatchPending.load(
                            std::memory_order_acquire)) {
                        return ThreadInitializationStatus::TerminationDispatchPending;
                    }

                    if (!_isTerminationDispatcherAvailable()) {
                        return ThreadInitializationStatus::TerminationDispatcherUnavailable;
                    }

                    const ThreadState initialState = GetThreadState();

                    // A replacement task must not observe Terminated and exit
                    // before Initialize() has finished publishing its handle.
                    if (initialState == ThreadState::Terminated) {
                        if (!TrySetThreadState(
                                ThreadState::Terminated,
                                ThreadState::Uninitialized)) {
                            return ThreadInitializationStatus::InvalidState;
                        }
                    } else if (initialState != ThreadState::Uninitialized) {
                        return ThreadInitializationStatus::InvalidState;
                    }

                    std::unique_lock<std::mutex> configurationLock(
                        _taskConfigurationMutex
                    );

                    // Another initializer may have published a task while this
                    // call waited for the configuration lock.
                    if (_taskHandle.load(std::memory_order_acquire) != nullptr) {
                        return ThreadInitializationStatus::AlreadyInitialized;
                    }

                    std::string threadName =
                        "thread" + std::to_string(GetThreadID());

                    TaskHandle_t createdTask = nullptr;

                    // Remove the completion signal left by a previous task.
                    xSemaphoreTake(_taskExited, 0);

                    const BaseType_t result = xTaskCreatePinnedToCore(
                        [](void* parameter) {
                            Thread* instance = static_cast<Thread*>(parameter);

                            // Do not enter the Thread lifecycle until Initialize()
                            // has published the handle and completed setup.
                            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

                            if (instance != nullptr) {
                                try {
                                    instance->_loop();
                                } catch (...) {
                                    // User code must not unwind through the
                                    // FreeRTOS task entry point. Convert worker
                                    // failures into ordinary termination.
                                    instance->_dispatchExecutionFailed(
                                        std::current_exception()
                                    );
                                    instance->Terminate();
                                }

                                // Prevent restart between publishing task exit
                                // and enqueueing asynchronous termination work.
                                instance->_terminationDispatchPending.store(
                                    true,
                                    std::memory_order_release
                                );

                                const TaskHandle_t currentTask =
                                    xTaskGetCurrentTaskHandle();
                                TaskHandle_t expected = currentTask;

                                instance->_taskHandle.compare_exchange_strong(
                                    expected,
                                    nullptr,
                                    std::memory_order_acq_rel,
                                    std::memory_order_acquire
                                );

                                instance->TrySetThreadState(
                                    ThreadState::Terminating,
                                    ThreadState::Terminated
                                );
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
                        return ThreadInitializationStatus::TaskCreationFailed;
                    }

                    TaskHandle_t expected = nullptr;

                    if (!_taskHandle.compare_exchange_strong(
                            expected,
                            createdTask,
                            std::memory_order_release,
                            std::memory_order_acquire)) {
                        vTaskDelete(createdTask);
                        return ThreadInitializationStatus::ConcurrentInitializationLost;
                    }

                    vTaskSetThreadLocalStoragePointerAndDelCallback(
                        createdTask,
                        ESPRESSIO_THREAD_TLS_INDEX,
                        this,
                        [](int, void* value) {
                            Thread* instance = static_cast<Thread*>(value);

                            if (instance == nullptr) {
                                return;
                            }

                            if (instance->GetThreadState() !=
                                ThreadState::Terminated) {
                                instance->_terminationDispatchPending.store(
                                    false,
                                    std::memory_order_release
                                );
                                if (instance->_taskExited != nullptr) {
                                    xSemaphoreGive(instance->_taskExited);
                                }
                                return;
                            }

                            instance->_terminationDispatchPending.store(
                                true,
                                std::memory_order_release
                            );

                            if (!Thread::_queueTerminationDispatch(instance)) {
                                // Task-deletion callbacks must not block or
                                // lazily create/signal other worker tasks. If
                                // the dispatcher queue is unexpectedly full,
                                // release Shutdown() and leave this terminated
                                // object registered for a later explicit
                                // ThreadManager::CleanUp().
                                if (instance->_taskExited != nullptr) {
                                    xSemaphoreGive(instance->_taskExited);
                                }

                                // Release the lifetime guard only after the
                                // final access to the Thread and its semaphore.
                                instance->_terminationDispatchPending.store(
                                    false,
                                    std::memory_order_release
                                );
                            }
                        }
                    );

                    configurationLock.unlock();

                    struct InitializationContextGuard {
                        std::atomic<TaskHandle_t>& taskHandle;
                        std::atomic<bool>& inProgress;

                        ~InitializationContextGuard() {
                            inProgress.store(false, std::memory_order_release);
                            taskHandle.store(nullptr, std::memory_order_release);
                        }
                    };

                    _initializingTaskHandle.store(
                        xTaskGetCurrentTaskHandle(),
                        std::memory_order_release
                    );
                    _initializationInProgress.store(
                        true,
                        std::memory_order_release
                    );
                    InitializationContextGuard initializationContext{
                        _initializingTaskHandle,
                        _initializationInProgress
                    };
                    try {
                        OnInitialization();

                        const ThreadState stateAfterInitialization =
                            GetThreadState();

                        if (stateAfterInitialization ==
                            ThreadState::Terminating) {
                            SetThreadState(ThreadState::Terminated);
                            _deleteTask();
                            return ThreadInitializationStatus::
                                TerminatedDuringInitialization;
                        }

                        if (stateAfterInitialization ==
                            ThreadState::Terminated) {
                            _deleteTask();
                            return ThreadInitializationStatus::
                                TerminatedDuringInitialization;
                        }

                        SetThreadState(ThreadState::Initialized);

                        const ThreadState stateAfterInitialized =
                            GetThreadState();

                        if (stateAfterInitialized ==
                            ThreadState::Terminating) {
                            SetThreadState(ThreadState::Terminated);
                            _deleteTask();
                            return ThreadInitializationStatus::
                                TerminatedDuringInitialization;
                        }

                        if (stateAfterInitialized ==
                            ThreadState::Terminated) {
                            _deleteTask();
                            return ThreadInitializationStatus::
                                TerminatedDuringInitialization;
                        }

                        if (stateAfterInitialized == ThreadState::Initialized &&
                            GetStartOnInitialize()) {
                            SetThreadState(ThreadState::Running);
                        }
                    } catch (...) {
                        // The worker is still blocked at its startup gate, so
                        // it can be deleted without racing OnLoop(). State
                        // callbacks may also throw; each transition changes
                        // state before dispatching its callbacks, therefore
                        // continuing cleanup remains safe.
                        try {
                            Terminate();
                        } catch (...) {}

                        try {
                            SetThreadState(ThreadState::Terminated);
                        } catch (...) {}

                        _deleteTask();
                        return ThreadInitializationStatus::
                            InitializationException;
                    }

                    xTaskNotifyGive(createdTask);
                    return ThreadInitializationStatus::Success;
                }

            public:
                ThreadInitializationStatus Initialize() override {
                    const ThreadInitializationStatus status = _initialize();

                    if (status != ThreadInitializationStatus::Success) {
                        TOnThreadInitializationFailedEvent
                            onInitializationFailed =
                                GetOnInitializationFailed();

                        if (onInitializationFailed != nullptr) {
                            try {
                                onInitializationFailed(this, status);
                            } catch (...) {
                                // A diagnostic callback must not replace the
                                // initialization outcome it is reporting.
                            }
                        }
                    }

                    return status;
                }

                void Terminate() override {
                    switch (GetThreadState()) {
                        case ThreadState::Uninitialized:
                            SetThreadState(ThreadState::Terminating);
                            SetThreadState(ThreadState::Terminated);
                            return;

                        case ThreadState::Initialized:
                        case ThreadState::Running:
                        case ThreadState::Paused:
                            SetThreadState(ThreadState::Terminating);
                            return;

                        case ThreadState::Terminating:
                        case ThreadState::Terminated:
                        case ThreadState::Destroyed:
                            return;
                    }
                }

                ThreadInitializationStatus Start() override {

                    switch (GetThreadState()) {
                        case ThreadState::Uninitialized:
                        case ThreadState::Terminated:
                            {
                                const ThreadInitializationStatus status =
                                    Initialize();

                                if (status !=
                                    ThreadInitializationStatus::Success) {
                                    return status;
                                }

                                TrySetThreadState(
                                    ThreadState::Initialized,
                                    ThreadState::Running
                                );
                                return status;
                            }

                        case ThreadState::Initialized:
                            TrySetThreadState(
                                ThreadState::Initialized,
                                ThreadState::Running
                            );
                            return ThreadInitializationStatus::AlreadyInitialized;

                        case ThreadState::Paused:
                            TrySetThreadState(
                                ThreadState::Paused,
                                ThreadState::Running
                            );
                            return ThreadInitializationStatus::AlreadyInitialized;

                        case ThreadState::Running:
                            return ThreadInitializationStatus::AlreadyInitialized;

                        case ThreadState::Terminating:
                        case ThreadState::Destroyed:
                            return ThreadInitializationStatus::InvalidState;
                    }

                    return ThreadInitializationStatus::InvalidState;
                }

                void Pause() override {
                    TrySetThreadState(
                        ThreadState::Running,
                        ThreadState::Paused
                    );
                }

                bool TryClaimAutomaticCleanup() override {
                    if (!GetFreeOnTerminate() ||
                        GetThreadState() != ThreadState::Terminated) {
                        return false;
                    }

                    CleanupClaim expected = CleanupClaim::Available;
                    return _cleanupClaim.compare_exchange_strong(
                        expected,
                        CleanupClaim::Automatic,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire
                    );
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
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    return _onDestroy;
                }

                TOnThreadEvent GetOnInitialize() override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    return _onInitialize;
                }

                TOnThreadEvent GetOnStart() override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    return _onStart;
                }

                TOnThreadEvent GetOnPause() override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    return _onPause;
                }

                TOnThreadEvent GetOnTerminate() override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    return _onTerminate;
                }

                TOnThreadEvent GetOnTerminated() override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    return _onTerminated;
                }

                TOnThreadInitializationFailedEvent
                GetOnInitializationFailed() override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    return _onInitializationFailed;
                }

                TOnThreadExecutionFailedEvent
                GetOnExecutionFailed() override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    return _onExecutionFailed;
                }

                TOnThreadStateChangeEvent GetOnStateChange() override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    return _onStateChange;
                }

            // Setters

                void SetCoreID(int value) override {
                    std::lock_guard<std::mutex> lock(_taskConfigurationMutex);
                    if (_taskHandle.load(std::memory_order_acquire) == nullptr) {
                        _coreID.Set(value);
                    }
                }

                void SetStackSize(uint32_t value) override {
                    std::lock_guard<std::mutex> lock(_taskConfigurationMutex);
                    if (_taskHandle.load(std::memory_order_acquire) == nullptr &&
                        value > 0) {
                        _stackSize.Set(value);
                    }
                }

                void SetPriority(unsigned int value) override {
                    std::lock_guard<std::mutex> lock(_taskConfigurationMutex);
                    if (_taskHandle.load(std::memory_order_acquire) == nullptr) {
                        _priority.Set(value);
                    }
                }

                void SetFreeOnTerminate(bool value) override {
                    _freeOnTerminate.Set(value);

                    if (value) {
                        CleanupClaim expected = CleanupClaim::Manual;
                        _cleanupClaim.compare_exchange_strong(
                            expected,
                            CleanupClaim::Available,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire
                        );
                    } else {
                        CleanupClaim expected = CleanupClaim::Available;
                        _cleanupClaim.compare_exchange_strong(
                            expected,
                            CleanupClaim::Manual,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire
                        );
                    }
                }

                void SetStartOnInitialize(bool value) override {
                    _startOnInitialize.Set(value);
                }

            // Callback Setters

                void SetOnDestroy(TOnThreadEvent value) override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    _onDestroy = value;
                }

                void SetOnInitialize(TOnThreadEvent value) override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    _onInitialize = value;
                }

                void SetOnStart(TOnThreadEvent value) override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    _onStart = value;
                }

                void SetOnPause(TOnThreadEvent value) override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    _onPause = value;
                }

                void SetOnTerminate(TOnThreadEvent value) override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    _onTerminate = value;
                }

                void SetOnTerminated(TOnThreadEvent value) override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    _onTerminated = value;
                }

                void SetOnInitializationFailed(
                    TOnThreadInitializationFailedEvent value
                ) override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    _onInitializationFailed = value;
                }

                void SetOnExecutionFailed(
                    TOnThreadExecutionFailedEvent value
                ) override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    _onExecutionFailed = value;
                }

                void SetOnStateChange(TOnThreadStateChangeEvent value) override {
                    std::lock_guard<std::mutex> lock(_callbackMutex);
                    _onStateChange = value;
                }
        };

    }
}
