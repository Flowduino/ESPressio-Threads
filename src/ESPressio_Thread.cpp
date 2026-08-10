#include "ESPressio_Thread.hpp"
#include "ESPressio_ThreadManager.hpp"

#include "ESPressio_ThreadGarbageCollector.hpp"

namespace ESPressio {

    namespace Threads {

        // Define the Constructor and Destructor of `Thread` here
        Thread::Thread() : _threadID(0) {
            SetCoreID(ThreadManager::GetInstance()->AddThread(this, &_threadID));
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

        void Thread::GarbageCollect() {
            if (GetFreeOnTerminate()) {
                _requestGarbageCollection();
            }
        }
    }

}
