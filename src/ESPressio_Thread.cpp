#include "ESPressio_Thread.hpp"
#include "ESPressio_ThreadManager.hpp"

#include "ESPressio_ThreadGarbageCollector.hpp"

namespace ESPressio {

    namespace Threads {

        // Define the Constructor and Destructor of `Thread` here
        Thread::Thread() : _threadID(0) {
            _threadID = ThreadManager::GetInstance()->GetThreadCount() + 1;
            SetCoreID(ThreadManager::GetInstance()->AddThread(this));
        }

        Thread::~Thread() {
            TOnThreadEvent onDestroy = GetOnDestroy();
            if (onDestroy != nullptr) { onDestroy(this); }
            SetThreadState(ThreadState::Destroyed);
            _deleteTask();
            if (_taskExited != nullptr) {
                vSemaphoreDelete(_taskExited);
                _taskExited = nullptr;
            }
        }

        void Thread::GarbageCollect() {
            if (GetFreeOnTerminate()) { ThreadGarbageCollector::GetInstance()->CleanUp(); } // Automatically trigger the Garbage Collector
        }
    }

}
