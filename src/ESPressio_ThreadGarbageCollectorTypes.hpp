#pragma once

#include <cstddef>

#include "ESPressio_ThreadManagerTypes.hpp"

namespace ESPressio {
namespace Threads {

    enum class ThreadGarbageCollectionExecutionMode {
        AsynchronousWorker,
        SynchronousFallback
    };


    struct ThreadGarbageCollectionResult {
        ThreadGarbageCollectionExecutionMode ExecutionMode =
            ThreadGarbageCollectionExecutionMode::AsynchronousWorker;

        bool InfrastructureAvailable = false;
        bool RequestQueued = false;
        bool Completed = false;
        bool Failed = false;

        ThreadManagerCleanupResult ManagerResult;
    };

}
}
