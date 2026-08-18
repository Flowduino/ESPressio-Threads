#pragma once

#include "ESPressio_PrecisionThread.hpp"

#include <ESPressio_Frequency_Serializable.hpp>
#include <ESPressio_Time_Serializable.hpp>

namespace ESPressio {

    namespace Threads {

        /*
         * When TTime is a Serializable ESPressio Time wrapper, keep every
         * Unit-valued PrecisionThread result serializable as well.
         *
         * Threads itself remains serialization-agnostic because this
         * specialization exists only in this explicitly opt-in header.
         */
        template<
            typename TValue,
            Units::UnitOrderOfMagnitude TMagnitude
        >
        struct PrecisionThreadTraits<
            Units::Internal::SerializableUnitType<
                Units::Time<
                    TValue,
                    TMagnitude
                >
            >
        > {
            using IterationTime =
                Units::SerializableTime<
                    TValue,
                    TMagnitude
                >;

            using SignedIterationTime =
                Units::SerializableTime<
                    int64_t,
                    Units::Nano
                >;

            using IterationFrequency =
                Units::SerializableFrequency<
                    double
                >;


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
