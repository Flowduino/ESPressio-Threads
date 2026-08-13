#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <type_traits>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "ESPressio_Frequency.hpp"
#include "ESPressio_IPrecisionThreadObserver.hpp"
#include "ESPressio_ISystemClock.hpp"
#include "ESPressio_SystemClock.hpp"
#include "ESPressio_Thread.hpp"
#include "ESPressio_ThreadSafeObservable.hpp"

namespace ESPressio {
    namespace Threads {

        enum class IterationDeltaMode : uint8_t {
            StartToStart,
            EndToStart
        };

        class PrecisionThread : public Thread {
            public:
                using IterationTime = Timing::ClockTime;
                using IterationFrequency = Units::Frequency<double>;
                using SignedIterationTime =
                    Units::Time<int64_t, Units::Nano>;

            private:
                class IterationObservable final :
                    public Observable::ThreadSafeObservable {
                    public:
                        void Notify(
                            PrecisionThread* thread,
                            IterationTime delta,
                            IterationTime startTime,
                            SkippedIterationCount skippedIterations
                        ) {
                            ExecuteNotification([&](
                                NotificationContext& notification
                            ) {
                                notification.WithObservers<
                                    IPrecisionThreadObserver
                                >([&](IPrecisionThreadObserver* observer) {
                                    observer->OnPrecisionThreadIteration(
                                        thread,
                                        delta,
                                        startTime,
                                        skippedIterations
                                    );
                                });
                            });
                        }
                };

                Timing::ISystemClock* _clock;
                std::shared_ptr<IterationObservable> _iterationObservable =
                    std::make_shared<IterationObservable>();
                SemaphoreHandle_t _scheduleSignal = xSemaphoreCreateBinary();
                mutable std::mutex _timingMutex;
                IterationDeltaMode _deltaMode =
                    IterationDeltaMode::StartToStart;
                uint64_t _iterationPeriodNanoseconds = 0;
                uint64_t _desiredIterationPeriodNanoseconds = 0;
                uint32_t _iterationSampleCount = 10;
                std::deque<uint64_t> _iterationSamples;
                double _iterationFrequency = 0.0;
                double _averageIterationFrequency = 0.0;
                bool _scheduleInitialized = false;
                bool _hasPreviousIteration = false;
                uint64_t _previousStartNanoseconds = 0;
                uint64_t _previousEndNanoseconds = 0;
                uint64_t _nextIterationNanoseconds = 0;
                uint64_t _activeIterationStartNanoseconds = 0;
                uint64_t _measurementGeneration = 0;
                std::atomic<bool> _workWakeRequested{false};

                static uint64_t _addSaturated(uint64_t left, uint64_t right) {
                    const uint64_t maximum =
                        std::numeric_limits<uint64_t>::max();
                    return right > maximum - left ? maximum : left + right;
                }

                static uint64_t _toNanoseconds(const IterationTime& time) {
                    return Timing::ClockBase::GetNanoseconds(time);
                }

                IterationTime _fromNanoseconds(uint64_t nanoseconds) const {
                    uint64_t resolution = _toNanoseconds(
                        _clock->GetResolution()
                    );
                    if (resolution == 0) {
                        resolution = 1;
                    }
                    return Timing::ClockBase::CreateClockTime(
                        nanoseconds, resolution
                    );
                }

                uint64_t _getNowNanoseconds() const {
                    return _toNanoseconds(_clock->GetTime());
                }

                void _signalScheduler() {
                    if (_scheduleSignal != nullptr) {
                        xSemaphoreGive(_scheduleSignal);
                    }
                }

                void _resetMeasurementsLocked() {
                    ++_measurementGeneration;
                    _hasPreviousIteration = false;
                    _previousStartNanoseconds = 0;
                    _previousEndNanoseconds = 0;
                    _activeIterationStartNanoseconds = 0;
                    _iterationSamples.clear();
                    _iterationFrequency = 0.0;
                    _averageIterationFrequency = 0.0;
                }

                void _resetSchedule(bool clearMeasurements = true) {
                    {
                        std::lock_guard<std::mutex> lock(_timingMutex);
                        _scheduleInitialized = false;
                        _nextIterationNanoseconds = 0;
                        if (clearMeasurements) {
                            _resetMeasurementsLocked();
                        }
                    }
                    _signalScheduler();
                }

                void _recordSampleLocked(uint64_t startToStartDelta) {
                    if (_iterationSampleCount == 0 ||
                        startToStartDelta == 0) {
                        return;
                    }
                    _iterationFrequency =
                        static_cast<double>(Timing::NanosecondsPerSecond) /
                        static_cast<double>(startToStartDelta);
                    _iterationSamples.push_back(startToStartDelta);
                    while (_iterationSamples.size() > _iterationSampleCount) {
                        _iterationSamples.pop_front();
                    }
                    long double total = 0.0L;
                    for (uint64_t sample : _iterationSamples) {
                        total += static_cast<long double>(sample);
                    }
                    _averageIterationFrequency = total > 0.0L
                        ? static_cast<double>(
                            (static_cast<long double>(
                                _iterationSamples.size()
                            ) * Timing::NanosecondsPerSecond) / total
                        )
                        : 0.0;
                }

                TickType_t _getWaitTicks(
                    uint64_t remainingNanoseconds
                ) const {
                    const uint64_t milliseconds =
                        remainingNanoseconds /
                        Timing::NanosecondsPerMillisecond;
                    if (milliseconds == 0) {
                        return 0;
                    }
                    const uint64_t bounded = std::min<uint64_t>(
                        milliseconds,
                        static_cast<uint64_t>(
                            std::numeric_limits<TickType_t>::max()
                        )
                    );
                    return pdMS_TO_TICKS(static_cast<uint32_t>(bounded));
                }

            protected:
                virtual void OnWorkWake() { }

                void WakeForWork() {
                    _workWakeRequested.store(true);
                    _signalScheduler();
                }

                virtual void Iterate(
                    IterationTime delta,
                    IterationTime startTime,
                    SkippedIterationCount skippedIterations
                ) = 0;

                void OnLoop() final override {
                    if (_workWakeRequested.exchange(false)) {
                        OnWorkWake();
                        return;
                    }

                    const uint64_t now = _getNowNanoseconds();
                    uint64_t period = 0;
                    uint64_t deltaNanoseconds = 0;
                    uint64_t remainingNanoseconds = 0;
                    uint64_t measurementGeneration = 0;
                    bool shouldWait = false;
                    SkippedIterationCount skippedIterations = 0;

                    {
                        std::lock_guard<std::mutex> lock(_timingMutex);
                        period = _iterationPeriodNanoseconds;
                        if (!_scheduleInitialized) {
                            _scheduleInitialized = true;
                            _nextIterationNanoseconds = now;
                        }
                        if (period > 0 && now < _nextIterationNanoseconds) {
                            remainingNanoseconds =
                                _nextIterationNanoseconds - now;
                            shouldWait = true;
                        } else {
                            if (_hasPreviousIteration) {
                                const uint64_t baseline =
                                    _deltaMode ==
                                        IterationDeltaMode::StartToStart
                                        ? _previousStartNanoseconds
                                        : _previousEndNanoseconds;
                                deltaNanoseconds = now >= baseline
                                    ? now - baseline
                                    : 0;
                                if (now >= _previousStartNanoseconds) {
                                    _recordSampleLocked(
                                        now - _previousStartNanoseconds
                                    );
                                }
                            }
                            if (period > 0) {
                                const uint64_t behind =
                                    now - _nextIterationNanoseconds;
                                const uint64_t elapsedPeriods =
                                    behind / period;
                                skippedIterations = elapsedPeriods;
                                const uint64_t periodsToAdvance =
                                    elapsedPeriods ==
                                        std::numeric_limits<uint64_t>::max()
                                        ? elapsedPeriods
                                        : elapsedPeriods + 1;
                                const uint64_t advance =
                                    periodsToAdvance >
                                        std::numeric_limits<uint64_t>::max() /
                                            period
                                        ? std::numeric_limits<uint64_t>::max()
                                        : periodsToAdvance * period;
                                _nextIterationNanoseconds = _addSaturated(
                                    _nextIterationNanoseconds, advance
                                );
                            }
                            _activeIterationStartNanoseconds = now;
                            measurementGeneration = _measurementGeneration;
                        }
                    }

                    if (shouldWait) {
                        const TickType_t waitTicks =
                            _getWaitTicks(remainingNanoseconds);
                        if (waitTicks > 0) {
                            xSemaphoreTake(_scheduleSignal, waitTicks);
                        } else {
                            taskYIELD();
                        }
                        return;
                    }

                    const IterationTime delta =
                        _fromNanoseconds(deltaNanoseconds);
                    const IterationTime startTime = _fromNanoseconds(now);
                    Iterate(delta, startTime, skippedIterations);
                    const uint64_t end = _getNowNanoseconds();

                    {
                        std::lock_guard<std::mutex> lock(_timingMutex);
                        if (measurementGeneration ==
                            _measurementGeneration) {
                            _previousStartNanoseconds = now;
                            _previousEndNanoseconds = end;
                            _hasPreviousIteration = true;
                        }
                    }

                    _iterationObservable->Notify(
                        this, delta, startTime, skippedIterations
                    );
                    if (period == 0) {
                        taskYIELD();
                    }
                }

            public:
                explicit PrecisionThread(
                    Timing::ISystemClock* clock = nullptr
                ) : _clock(
                        clock == nullptr
                            ? Timing::SystemClock::GetInstance()
                            : clock
                    ) { }

                PrecisionThread(
                    bool freeOnTerminate,
                    Timing::ISystemClock* clock = nullptr
                ) : Thread(freeOnTerminate),
                    _clock(
                        clock == nullptr
                            ? Timing::SystemClock::GetInstance()
                            : clock
                    ) { }

                ~PrecisionThread() override {
                    Shutdown();
                    if (_scheduleSignal != nullptr) {
                        vSemaphoreDelete(_scheduleSignal);
                        _scheduleSignal = nullptr;
                    }
                }

                ThreadInitializationStatus Initialize() override {
                    _resetSchedule();
                    return Thread::Initialize();
                }

                ThreadInitializationStatus Start() override {
                    if (GetThreadState() == ThreadState::Paused) {
                        _resetSchedule();
                    }
                    const ThreadInitializationStatus status = Thread::Start();
                    _signalScheduler();
                    return status;
                }

                void Pause() override {
                    Thread::Pause();
                    _resetSchedule();
                }

                void Terminate() override {
                    Thread::Terminate();
                    _signalScheduler();
                }

                void Bump() {
                    const uint64_t now = _getNowNanoseconds();
                    {
                        std::lock_guard<std::mutex> lock(_timingMutex);
                        _nextIterationNanoseconds = now;
                        _scheduleInitialized = true;
                    }
                    _signalScheduler();
                }

                Observable::IObserverHandle* RegisterIterationObserver(
                    IPrecisionThreadObserver* observer
                ) {
                    return _iterationObservable->RegisterObserver(observer);
                }

                void UnregisterIterationObserver(
                    IPrecisionThreadObserver* observer
                ) {
                    _iterationObservable->UnregisterObserver(observer);
                }

                Timing::ISystemClock* GetClock() const { return _clock; }

                IterationDeltaMode GetIterationDeltaMode() const {
                    std::lock_guard<std::mutex> lock(_timingMutex);
                    return _deltaMode;
                }

                void SetIterationDeltaMode(IterationDeltaMode mode) {
                    std::lock_guard<std::mutex> lock(_timingMutex);
                    if (_deltaMode != mode) {
                        _deltaMode = mode;
                        _resetMeasurementsLocked();
                    }
                }

                IterationTime GetIterationPeriod() const {
                    std::lock_guard<std::mutex> lock(_timingMutex);
                    return _fromNanoseconds(_iterationPeriodNanoseconds);
                }

                void SetIterationPeriod(IterationTime period) {
                    const uint64_t nanoseconds = _toNanoseconds(period);
                    {
                        std::lock_guard<std::mutex> lock(_timingMutex);
                        _iterationPeriodNanoseconds = nanoseconds;
                        if (nanoseconds > 0 &&
                            _desiredIterationPeriodNanoseconds > 0 &&
                            _desiredIterationPeriodNanoseconds < nanoseconds) {
                            _desiredIterationPeriodNanoseconds = nanoseconds;
                        }
                        _scheduleInitialized = false;
                    }
                    _signalScheduler();
                }

                template <
                    typename TValue,
                    Units::UnitOrderOfMagnitude TMagnitude
                >
                void SetIterationPeriod(
                    const Units::Time<TValue, TMagnitude>& period
                ) {
                    static_assert(
                        std::is_integral<TValue>::value &&
                            std::is_unsigned<TValue>::value,
                        "Iteration periods require an unsigned integral "
                        "ESPressio Time value"
                    );
                    SetIterationPeriod(IterationTime(
                        period.template ToMagnitude<uint64_t>(Units::Nano),
                        Units::Nano
                    ));
                }

                IterationTime GetDesiredIterationPeriod() const {
                    std::lock_guard<std::mutex> lock(_timingMutex);
                    return _fromNanoseconds(
                        _desiredIterationPeriodNanoseconds
                    );
                }

                void SetDesiredIterationPeriod(IterationTime period) {
                    uint64_t nanoseconds = _toNanoseconds(period);
                    std::lock_guard<std::mutex> lock(_timingMutex);
                    if (_iterationPeriodNanoseconds > 0 &&
                        nanoseconds > 0 &&
                        nanoseconds < _iterationPeriodNanoseconds) {
                        nanoseconds = _iterationPeriodNanoseconds;
                    }
                    _desiredIterationPeriodNanoseconds = nanoseconds;
                }

                template <
                    typename TValue,
                    Units::UnitOrderOfMagnitude TMagnitude
                >
                void SetDesiredIterationPeriod(
                    const Units::Time<TValue, TMagnitude>& period
                ) {
                    static_assert(
                        std::is_integral<TValue>::value &&
                            std::is_unsigned<TValue>::value,
                        "Desired iteration periods require an unsigned "
                        "integral ESPressio Time value"
                    );
                    SetDesiredIterationPeriod(IterationTime(
                        period.template ToMagnitude<uint64_t>(Units::Nano),
                        Units::Nano
                    ));
                }

                uint32_t GetIterationSampleCount() const {
                    std::lock_guard<std::mutex> lock(_timingMutex);
                    return _iterationSampleCount;
                }

                void SetIterationSampleCount(uint32_t sampleCount) {
                    std::lock_guard<std::mutex> lock(_timingMutex);
                    _iterationSampleCount = sampleCount;
                    _iterationSamples.clear();
                    _iterationFrequency = 0.0;
                    _averageIterationFrequency = 0.0;
                }

                IterationFrequency GetIterationFrequency() const {
                    std::lock_guard<std::mutex> lock(_timingMutex);
                    return IterationFrequency(_iterationFrequency);
                }

                IterationFrequency GetAverageIterationFrequency() const {
                    std::lock_guard<std::mutex> lock(_timingMutex);
                    return IterationFrequency(
                        _averageIterationFrequency
                    );
                }

                SignedIterationTime GetAvailableIterationTime() const {
                    const uint64_t now = _getNowNanoseconds();
                    std::lock_guard<std::mutex> lock(_timingMutex);
                    if (_desiredIterationPeriodNanoseconds == 0 ||
                        _activeIterationStartNanoseconds == 0) {
                        return SignedIterationTime(0);
                    }
                    const uint64_t deadline = _addSaturated(
                        _activeIterationStartNanoseconds,
                        _desiredIterationPeriodNanoseconds
                    );
                    if (deadline >= now) {
                        const uint64_t remaining = deadline - now;
                        return SignedIterationTime(
                            remaining > static_cast<uint64_t>(
                                std::numeric_limits<int64_t>::max()
                            )
                                ? std::numeric_limits<int64_t>::max()
                                : static_cast<int64_t>(remaining)
                        );
                    }
                    const uint64_t overrun = now - deadline;
                    return SignedIterationTime(
                        overrun > static_cast<uint64_t>(
                            std::numeric_limits<int64_t>::max()
                        )
                            ? std::numeric_limits<int64_t>::min()
                            : -static_cast<int64_t>(overrun)
                    );
                }
        };
    }
}
