#include "ESPressio_Thread.hpp"
#include "ESPressio_ThreadManager.hpp"

#include "ESPressio_ThreadGarbageCollector.hpp"
#include "ESPressio_ThreadTerminationDispatcher.hpp"

namespace ESPressio {

    namespace Threads {

        // Define the Constructor and Destructor of `Thread` here
        Thread::Thread() : _threadID(0) {
            try {
                SetCoreID(
                    ThreadManager::GetInstance()->AddThread(this, &_threadID)
                );
            } catch (...) {
                if (_taskExited != nullptr) {
                    vSemaphoreDelete(_taskExited);
                    _taskExited = nullptr;
                }
                throw;
            }
        }

        Thread::~Thread() {
            SetThreadState(ThreadState::Destroyed);
            TOnThreadEvent onDestroy = GetOnDestroy();
            if (onDestroy != nullptr) { onDestroy(this); }
            _deleteTask();
            if (_taskExited != nullptr) {
                vSemaphoreDelete(_taskExited);
                _taskExited = nullptr;
            }
            ThreadManager::GetInstance()->RemoveThread(this);
        }

        void Thread::_requestGarbageCollection() {
            ThreadGarbageCollector::GetInstance()->CleanUp();
        }

        bool Thread::_isTerminationDispatcherAvailable() {
            return ThreadTerminationDispatcher::GetInstance()->IsAvailable();
        }

        bool Thread::_queueTerminationDispatch(Thread* thread) {
            return ThreadTerminationDispatcher::GetInstance()->Dispatch(thread);
        }

        void Thread::_dispatchTermination() {
            const bool terminated =
                GetThreadState() == ThreadState::Terminated;
            const bool shouldGarbageCollect =
                terminated && GetFreeOnTerminate();
            const SemaphoreHandle_t taskExited = _taskExited;
            TOnThreadEvent onTerminated = GetOnTerminated();

            if (terminated && onTerminated != nullptr) {
                try {
                    onTerminated(this);
                } catch (...) {
                    // User callbacks must not terminate the dispatcher task.
                }
            }

            _terminationDispatchPending.store(false, std::memory_order_release);

            if (shouldGarbageCollect) {
                _requestGarbageCollection();
            } else if (taskExited != nullptr) {
                xSemaphoreGive(taskExited);
            }
        }

        void Thread::GarbageCollect() {
            if (GetFreeOnTerminate()) {
                _requestGarbageCollection();
            }
        }
    }

}
