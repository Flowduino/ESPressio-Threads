#include <ESPressio_PrecisionThread.hpp>
#include <ESPressio_ThreadManager.hpp>

using namespace ESPressio;

class HeartbeatThread final : public Threads::PrecisionThread {
    protected:
        void Iterate(
            IterationTime delta,
            IterationTime startTime,
            Threads::SkippedIterationCount skippedIterations
        ) override {
            (void)delta;
            (void)startTime;
            (void)skippedIterations;
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
            (void)delta;
            (void)startTime;
            (void)skippedIterations;
        }
};

HeartbeatThread heartbeat;
HeartbeatObserver heartbeatObserver;
Observable::IObserverHandle* heartbeatObserverHandle = nullptr;

void setup() {
    heartbeat.SetIterationPeriod(
        Units::MilliSeconds<uint64_t>(10)
    );
    heartbeat.SetIterationSampleCount(10);
    heartbeatObserverHandle = heartbeat.RegisterIterationObserver(
        &heartbeatObserver
    );
    Threads::ThreadManager::GetInstance()->Initialize();
}

void loop() {
    delay(1000);
}
