#include "ESPressio_Thread.hpp"
#include "ESPressio_ThreadManager.hpp"

#include "ESPressio_ThreadGarbageCollector.hpp"

namespace ESPressio {

    namespace Threads {

        // Define the Constructor and Destructor of `Thread` here
        Thread::Thread() : _threadID(0) {
            _threadID = ThreadManager::GetInstance()->GetThreadCount();
            SetCoreID(ThreadManager::GetInstance()->AddThread(this));
        }

        Thread::~Thread() {
            if (_onDestroy != nullptr) { _onDestroy(this); }
            SetThreadState(ThreadState::Destroyed);
            ThreadManager::GetInstance()->RemoveThread(this);
            if (_taskHandle != nullptr) { vTaskDelete(_taskHandle); }
        }

        // Define the Terminate method of `Thread` here
        void Thread::Terminate() {
            SetThreadState(ThreadState::Terminated);
            if (_onTerminate != nullptr) { _onTerminate(this); }
            if (GetFreeOnTerminate()) { ThreadGarbageCollector::GetInstance()->CleanUp(); } // Automatically trigger the Garbage Collector
        }

    }

}