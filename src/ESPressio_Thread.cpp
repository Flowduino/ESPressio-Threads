#include "ESPressio_Thread.hpp"
#include "ESPressio_ThreadManager.hpp"

#include "ESPressio_ThreadGarbageCollector.hpp"

namespace ESPressio {

    namespace Threads {

        // Define the Constructor and Destructor of `Thread` here
        Thread::Thread() : _threadID(0) {
            SetFreeOnTerminate(freeOnTerminate);
            _threadID = ThreadManager::GetInstance()->GetThreadCount();
            SetCoreID(ThreadManager::GetInstance()->AddThread(this));
        }

        Thread::~Thread() {
            SetThreadState(ThreadState::Destroyed);
            ThreadManager::GetInstance()->RemoveThread(this);
            if (_taskHandle != nullptr) { vTaskDelete(_taskHandle); }
        }

        // Define the Terminate method of `Thread` here
        void Thread::Terminate() {
            SetThreadState(ThreadState::Terminated);
            if (GetFreeOnTerminate()) { ThreadGarbageCollector::GetInstance()->CleanUp(); } // Automatically trigger the Garbage Collector
        }

    }

}