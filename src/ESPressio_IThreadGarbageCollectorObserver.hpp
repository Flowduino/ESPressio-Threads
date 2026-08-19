#pragma once

#include <exception>

#include <ESPressio_IObserver.hpp>

#include "ESPressio_ThreadGarbageCollectorTypes.hpp"

namespace ESPressio {
namespace Threads {

    class IThreadGarbageCollectorObserver :
        public virtual Observable::IObserver {

    public:
        virtual ~IThreadGarbageCollectorObserver() = default;

        virtual void OnThreadGarbageCollectorInitialized(
            bool
        ) {}

        virtual void OnThreadGarbageCollectorInitializationFailed() {}

        virtual void OnThreadGarbageCollectionRequested(
            ThreadGarbageCollectionExecutionMode
        ) {}

        virtual void OnThreadGarbageCollectionQueued(
            const ThreadGarbageCollectionResult&
        ) {}

        virtual void OnThreadGarbageCollectionRequestCoalesced(
            const ThreadGarbageCollectionResult&
        ) {}

        virtual void OnThreadGarbageCollectionStarted(
            const ThreadGarbageCollectionResult&
        ) {}

        virtual void OnThreadGarbageCollectionCompleted(
            const ThreadGarbageCollectionResult&
        ) {}

        virtual void OnThreadGarbageCollectionFailed(
            const ThreadGarbageCollectionResult&,
            std::exception_ptr
        ) {}

        virtual void OnThreadGarbageCollectionFallbackStarted(
            const ThreadGarbageCollectionResult&
        ) {}
    };

}
}
