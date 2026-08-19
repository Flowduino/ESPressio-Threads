#pragma once

#include <ESPressio_IObservable.hpp>

#include "ESPressio_IThreadGarbageCollectorObserver.hpp"

namespace ESPressio {
namespace Threads {

    class IThreadGarbageCollector {
    public:
        virtual ~IThreadGarbageCollector() = default;

        virtual void CleanUp() = 0;

        virtual bool IsAvailable() const = 0;

        virtual Observable::ObserverHandlePtr
        RegisterObserver(
            IThreadGarbageCollectorObserver* observer
        ) = 0;

        virtual void UnregisterObserver(
            IThreadGarbageCollectorObserver* observer
        ) = 0;
    };

}
}
