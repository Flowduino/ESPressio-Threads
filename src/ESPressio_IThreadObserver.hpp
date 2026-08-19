#pragma once

#include <exception>

#include <ESPressio_IObserver.hpp>

#include "ESPressio_IThread.hpp"

namespace ESPressio {
namespace Threads {

    class IThreadObserver :
        public virtual Observable::IObserver {

    public:
        virtual ~IThreadObserver() = default;

        virtual void OnThreadStateChanged(
            IThread*,
            ThreadState,
            ThreadState
        ) {}

        virtual void OnThreadUninitialized(IThread*) {}
        virtual void OnThreadInitialized(IThread*) {}
        virtual void OnThreadStarted(IThread*) {}
        virtual void OnThreadPaused(IThread*) {}
        virtual void OnThreadTerminationRequested(IThread*) {}
        virtual void OnThreadTerminated(IThread*) {}
        virtual void OnThreadDestroyed(IThread*) {}
        virtual void OnThreadTaskExited(IThread*) {}

        virtual void OnThreadInitializationFailed(
            IThread*,
            ThreadInitializationStatus
        ) {}

        virtual void OnThreadExecutionFailed(
            IThread*,
            std::exception_ptr
        ) {}
    };

}
}
