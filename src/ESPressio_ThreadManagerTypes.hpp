#pragma once

#include <cstddef>
#include <cstdint>

#include "ESPressio_IThread.hpp"

namespace ESPressio {
namespace Threads {

    struct ThreadManagerThreadSnapshot {
        uint8_t ThreadID = 0;
        int CoreID = 0;
        ThreadState State = ThreadState::Uninitialized;
        bool FreeOnTerminate = false;
        bool StartOnInitialize = true;
    };


    struct ThreadManagerCleanupResult {
        std::size_t ThreadsExamined = 0;
        std::size_t ThreadsClaimed = 0;
        std::size_t ThreadsRemoved = 0;
        std::size_t ThreadsDeleted = 0;

        bool WasDeferred = false;
        std::size_t ActiveIterationCount = 0;

        std::size_t ThreadCountBefore = 0;
        std::size_t ThreadCountAfter = 0;
    };


    struct ThreadManagerInitializationResult {
        std::size_t ThreadsExamined = 0;
        std::size_t ThreadsInitializedSuccessfully = 0;
        std::size_t ThreadsInitializationFailed = 0;
    };

}
}
