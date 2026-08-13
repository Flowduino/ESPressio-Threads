#pragma once

#include <exception>

#include "ESPressio_IObserver.hpp"
#include "ESPressio_IThread.hpp"

namespace ESPressio {
    namespace Threads {

        class IThreadObserver : public virtual Observable::IObserver {
            public:
                virtual ~IThreadObserver() = default;

                virtual void OnThreadStateChanged(
                    IThread* thread,
                    ThreadState oldState,
                    ThreadState newState
                ) { }

                virtual void OnThreadUninitialized(IThread* thread) { }
                virtual void OnThreadInitialized(IThread* thread) { }
                virtual void OnThreadStarted(IThread* thread) { }
                virtual void OnThreadPaused(IThread* thread) { }
                virtual void OnThreadTerminationRequested(IThread* thread) { }
                virtual void OnThreadTerminated(IThread* thread) { }
                virtual void OnThreadDestroyed(IThread* thread) { }

                virtual void OnThreadTaskExited(IThread* thread) { }

                virtual void OnThreadInitializationFailed(
                    IThread* thread,
                    ThreadInitializationStatus status
                ) { }

                virtual void OnThreadExecutionFailed(
                    IThread* thread,
                    std::exception_ptr cause
                ) { }
        };

    }
}
