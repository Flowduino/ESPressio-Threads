#pragma once

#include <exception>

#include <ESPressio_IObserver.hpp>

#include "ESPressio_IThread.hpp"
#include "ESPressio_ThreadManagerTypes.hpp"

namespace ESPressio {
namespace Threads {

    class IThreadManagerObserver :
        public virtual Observable::IObserver {

    public:
        virtual ~IThreadManagerObserver() = default;

        virtual void OnThreadRegistered(
            IThread*,
            const ThreadManagerThreadSnapshot&
        ) {}

        virtual void OnThreadRegistrationFailed(
            IThread*,
            std::exception_ptr
        ) {}

        virtual void OnThreadRemoved(
            const ThreadManagerThreadSnapshot&
        ) {}

        virtual void OnThreadCleanupClaimed(
            IThread*,
            const ThreadManagerThreadSnapshot&
        ) {}

        virtual void OnThreadCleanupDeferred(
            const ThreadManagerCleanupResult&
        ) {}

        virtual void OnThreadCleanupStarted(
            const ThreadManagerCleanupResult&
        ) {}

        virtual void OnThreadCleanupCompleted(
            const ThreadManagerCleanupResult&
        ) {}

        virtual void OnThreadCleanupFailed(
            const ThreadManagerCleanupResult&,
            std::exception_ptr
        ) {}

        virtual void OnThreadManagerInitializationCompleted(
            const ThreadManagerInitializationResult&
        ) {}
    };

}
}
