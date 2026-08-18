#pragma once

#include <cstdint>

#include "ESPressio_Frequency.hpp"
#include "ESPressio_Time.hpp"

namespace ESPressio {

    namespace Threads {

        /*
         * Public Unit representation policy for PrecisionThread.
         *
         * TTime controls iteration timestamps/deltas. The policy additionally
         * defines the public types used for signed remaining/overrun time and
         * measured iteration frequency.
         *
         * The default policy uses ordinary ESPressio Unit types and therefore
         * introduces no Serializable dependency.
         */
        template<typename TTime>
        struct PrecisionThreadTraits {
            using IterationTime = TTime;

            using SignedIterationTime =
                Units::Time<
                    int64_t,
                    Units::Nano
                >;

            using IterationFrequency =
                Units::Frequency<double>;


            static SignedIterationTime
            CreateSignedIterationTime(
                int64_t nanoseconds
            ) {
                return SignedIterationTime(
                    nanoseconds,
                    Units::Nano
                );
            }


            static IterationFrequency
            CreateIterationFrequency(
                double frequency
            ) {
                return IterationFrequency(
                    frequency
                );
            }
        };

    }

}
