#pragma once

#include <ESPressio_IObserver.hpp>

#include "ESPressio_ThreadManagerTypes.hpp"

namespace ESPressio {
namespace Threads {

    class IThreadTerminationDispatcherObserver :
        public virtual Observable::IObserver {

    public:
        virtual ~IThreadTerminationDispatcherObserver() = default;

        virtual void OnThreadTerminationDispatcherInitialized(
            bool
        ) {}

        virtual void OnThreadTerminationDispatchQueued(
            const ThreadManagerThreadSnapshot&
        ) {}

        virtual void OnThreadTerminationDispatchQueueFailed(
            const ThreadManagerThreadSnapshot&
        ) {}

        virtual void OnThreadTerminationDispatchStarted(
            const ThreadManagerThreadSnapshot&
        ) {}

        virtual void OnThreadTerminationDispatchCompleted(
            const ThreadManagerThreadSnapshot&
        ) {}
    };

}
}
