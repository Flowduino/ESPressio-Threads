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
            if (_onDestroy() != nullptr) { _onDestroy(); }
            SetThreadState(ThreadState::Destroyed);
            _deleteTask();
        }

        void Thread::GarbageCollect() {
            if (GetFreeOnTerminate()) { ThreadGarbageCollector::GetInstance()->CleanUp(); } // Automatically trigger the Garbage Collector
        }
    }

}