#pragma once

#include "ESPressio_IObserver.hpp"
#include "ESPressio_IClock.hpp"

namespace ESPressio {
    namespace Threads {
        class PrecisionThread;

        class IPrecisionThreadObserver :
            public virtual Observable::IObserver {
            public:
                virtual ~IPrecisionThreadObserver() = default;
                virtual void OnPrecisionThreadIteration(
                    PrecisionThread* thread,
                    Timing::ClockTime delta,
                    Timing::ClockTime startTime,
                    bool isLate
                ) { }
        };
    }
}
