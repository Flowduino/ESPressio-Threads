#pragma once

#include <cstdint>

#include "ESPressio_IObserver.hpp"
#include "ESPressio_ClockTypes.hpp"

namespace ESPressio {

namespace Threads {

template<
    typename TTime = Timing::DefaultClockTime
>
class PrecisionThread;

using SkippedIterationCount = uint64_t;

template<
    typename TTime = Timing::DefaultClockTime
>
class IPrecisionThreadObserver :
    public virtual Observable::IObserver {

public:

    using TimeType = TTime;
    using ThreadType = PrecisionThread<TTime>;

    virtual ~IPrecisionThreadObserver() = default;

    virtual void OnPrecisionThreadIteration(
        ThreadType* thread,
        TTime delta,
        TTime startTime,
        SkippedIterationCount skippedIterations
    ) { }

};

}

}
