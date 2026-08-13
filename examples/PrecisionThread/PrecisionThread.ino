#include <ESPressio_PrecisionThread.hpp>
#include <ESPressio_ThreadManager.hpp>

using namespace ESPressio;

constexpr uint8_t HeartbeatPin = 2;

class HeartbeatThread final : public Threads::PrecisionThread {
    private:
        bool _ledState = false;

    protected:
        void Iterate(
            IterationTime delta,
            IterationTime startTime,
            Threads::SkippedIterationCount skippedIterations
        ) override {
            (void)delta;
            (void)startTime;
            (void)skippedIterations;

            _ledState = !_ledState;
            digitalWrite(HeartbeatPin, _ledState ? HIGH : LOW);
        }
};

class HeartbeatObserver final :
    public Threads::IPrecisionThreadObserver {
    public:
        void OnPrecisionThreadIteration(
            Threads::PrecisionThread* thread,
            Timing::ClockTime delta,
            Timing::ClockTime startTime,
            Threads::SkippedIterationCount skippedIterations
        ) override {
            (void)thread;

            Serial.printf(
                "iteration at %llu ms; delta=%llu us; skipped=%llu\n",
                static_cast<unsigned long long>(
                    startTime.ToMagnitude<uint64_t>(Units::Milli)
                ),
                static_cast<unsigned long long>(
                    delta.ToMagnitude<uint64_t>(Units::Micro)
                ),
                static_cast<unsigned long long>(skippedIterations)
            );
        }
};

HeartbeatObserver heartbeatObserver;
HeartbeatThread heartbeat;
Observable::IObserverHandle* heartbeatObserverHandle = nullptr;

void setup() {
    Serial.begin(115200);
    pinMode(HeartbeatPin, OUTPUT);

    heartbeat.SetIterationPeriod(
        Units::MilliSeconds<uint64_t>(500)
    );
    heartbeat.SetIterationDeltaMode(
        Threads::IterationDeltaMode::StartToStart
    );
    heartbeat.SetIterationSampleCount(10);
    heartbeatObserverHandle = heartbeat.RegisterIterationObserver(
        &heartbeatObserver
    );
    Threads::ThreadManager::GetInstance()->Initialize();
}

void loop() {
    const double currentFrequency =
        heartbeat.GetIterationFrequency().value;
    const double averageFrequency =
        heartbeat.GetAverageIterationFrequency().value;

    (void)currentFrequency;
    (void)averageFrequency;
    delay(1000);
}
