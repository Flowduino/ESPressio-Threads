#pragma once

#include <cstdint>

#include "ESPressio_IObserver.hpp"
#include "ESPressio_ClockTypes.hpp"
#include "ESPressio_PrecisionThreadTraits.hpp"

namespace ESPressio {

    namespace Threads {

        template<
            typename TTime = Timing::DefaultClockTime,
            typename TRepresentationTraits =
                PrecisionThreadTraits<TTime>
        >
        class PrecisionThread;

        using SkippedIterationCount =
            uint64_t;


        template<
            typename TTime = Timing::DefaultClockTime,
            typename TRepresentationTraits =
                PrecisionThreadTraits<TTime>
        >
        class IPrecisionThreadObserver :
            public virtual Observable::IObserver {

            public:
                using TimeType =
                    TTime;

                using RepresentationTraits =
                    TRepresentationTraits;

                using ThreadType =
                    PrecisionThread<
                        TTime,
                        TRepresentationTraits
                    >;

                virtual ~IPrecisionThreadObserver() =
                    default;


                virtual void
                OnPrecisionThreadIteration(
                    ThreadType*,
                    TTime,
                    TTime,
                    SkippedIterationCount
                ) {
                }
        };

    }

}
