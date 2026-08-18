#include <ESPressio_PrecisionThread_Serializable.hpp>
#include <ESPressio_ThreadManager.hpp>

using namespace ESPressio;

using SerializableThreadTime =
    Units::SerializableNanoSeconds<
        uint64_t
    >;

class SerializableWorker final :
    public Threads::PrecisionThread<
        SerializableThreadTime
    > {

    protected:
        void Iterate(
            IterationTime delta,
            IterationTime startTime,
            Threads::SkippedIterationCount skippedIterations
        ) override {
            /*
             * With the optional Serializable policy header:
             *
             *   IterationTime
             *   SignedIterationTime
             *   IterationFrequency
             *
             * are all Serializable Unit types.
             */
            (void)delta;
            (void)startTime;
            (void)skippedIterations;
        }
};

SerializableWorker worker;

void setup() {
    Serial.begin(115200);

    worker.SetIterationPeriod(
        Units::MilliSeconds<
            uint64_t
        >(500)
    );

    Threads::ThreadManager::
        GetInstance()->
        Initialize();
}

void loop() {
    auto available =
        worker.
            GetAvailableIterationTime();

    auto frequency =
        worker.
            GetAverageIterationFrequency();

    (void)available;
    (void)frequency;

    delay(1000);
}
