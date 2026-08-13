#pragma once

#include <cstdint>

#include "ESPressio_IObserver.hpp"
#include "ESPressio_IClock.hpp"

namespace ESPressio {
    namespace Threads {
        class PrecisionThread;
        using SkippedIterationCount = uint64_t;

        class IPrecisionThreadObserver :
            public virtual Observable::IObserver {
            public:
                virtual ~IPrecisionThreadObserver() = default;
                virtual void OnPrecisionThreadIteration(
                    PrecisionThread* thread,
                    Timing::ClockTime delta,
                    Timing::ClockTime startTime,
                    SkippedIterationCount skippedIterations
                ) { }
        };
    }
}
