#pragma once

// define CORE_THREADING_DEBUG in your project to enable debugging!

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <mutex>
#include <vector>

#include "ESPressio_ThreadSafe.hpp"
#include "ESPressio_IThread.hpp"

namespace ESPressio {

    namespace Threads {

        struct ThreadInitializationResult {
            uint8_t threadID;
            ThreadInitializationStatus status;
        };


        class ThreadManager {
            private:
                struct ThreadRecord {
                    uint8_t id;
                    IThread* thread;
                    int coreID;

                    bool operator==(
                        const ThreadRecord& other
                    ) const {
                        return
                            id == other.id &&
                            thread == other.thread &&
                            coreID == other.coreID;
                    }
                };

                ReadWriteMutex<
                    std::vector<ThreadRecord>
                > _threads;

                ReadWriteMutex<int>
                    _nextCoreID =
                        ReadWriteMutex<int>(0);

                std::recursive_mutex _iterationMutex;
                std::size_t _activeIterations = 0;
                bool _cleanupPending = false;


                void _beginIteration() {
                    std::lock_guard<
                        std::recursive_mutex
                    > lock(_iterationMutex);

                    ++_activeIterations;
                }


                void _endIteration() {
                    bool runDeferredCleanup = false;

                    {
                        std::lock_guard<
                            std::recursive_mutex
                        > lock(_iterationMutex);

                        if (_activeIterations > 0) {
                            --_activeIterations;
                        }

                        if (
                            _activeIterations == 0 &&
                            _cleanupPending
                        ) {
                            _cleanupPending = false;
                            runDeferredCleanup = true;
                        }
                    }

                    if (runDeferredCleanup) {
                        CleanUp();
                    }
                }


                class IterationGuard {
                    private:
                        ThreadManager& _manager;

                    public:
                        explicit IterationGuard(
                            ThreadManager& manager
                        ) :
                            _manager(manager) {

                            _manager._beginIteration();
                        }

                        ~IterationGuard() {
                            _manager._endIteration();
                        }

                        IterationGuard(
                            const IterationGuard&
                        ) = delete;

                        IterationGuard& operator=(
                            const IterationGuard&
                        ) = delete;
                };


                static int _getCoreCount() {
                    #if defined(portNUM_PROCESSORS)
                        return
                            portNUM_PROCESSORS > 0
                                ? portNUM_PROCESSORS
                                : 1;
                    #elif defined(configNUMBER_OF_CORES)
                        return
                            configNUMBER_OF_CORES > 0
                                ? configNUMBER_OF_CORES
                                : 1;
                    #else
                        return 1;
                    #endif
                }


            protected:
                ThreadManager() :
                    _threads(
                        std::vector<
                            ThreadRecord
                        >()
                    ) {
                }


            public:
                static ThreadManager*
                GetInstance() {
                    // Process-lifetime by design: destroying the manager
                    // during static teardown could race static Thread objects
                    // and already-stopped FreeRTOS infrastructure.
                    static ThreadManager* instance =
                        new ThreadManager();

                    return instance;
                }


                int AddThread(
                    IThread* thread,
                    uint8_t* assignedThreadID =
                        nullptr
                ) {
                    if (thread == nullptr) {
                        throw
                            ThreadInvalidRegistrationException();
                    }

                    ThreadRecord existingRecord{
                        0,
                        nullptr,
                        0
                    };

                    _threads.WithSharedReadLock(
                        [
                            thread,
                            &existingRecord
                        ](
                            const std::vector<
                                ThreadRecord
                            >& threads
                        ) {
                            const auto existing =
                                std::find_if(
                                    threads.begin(),
                                    threads.end(),
                                    [thread](
                                        const ThreadRecord&
                                            record
                                    ) {
                                        return
                                            record.thread ==
                                            thread;
                                    }
                                );

                            if (
                                existing !=
                                threads.end()
                            ) {
                                existingRecord =
                                    *existing;
                            }
                        }
                    );

                    if (
                        existingRecord.thread !=
                        nullptr
                    ) {
                        if (
                            assignedThreadID !=
                            nullptr
                        ) {
                            *assignedThreadID =
                                existingRecord.id;
                        }

                        return
                            existingRecord.coreID;
                    }

                    // Evaluate custom IDs before taking the list write lock;
                    // a custom implementation may re-enter the manager from
                    // GetThreadID().
                    const uint8_t requestedThreadID =
                        assignedThreadID == nullptr
                            ? thread->GetThreadID()
                            : 0;

                    int useCore = 0;

                    uint8_t resolvedThreadID =
                        requestedThreadID;

                    _threads.WithWriteLock(
                        [&](
                            std::vector<
                                ThreadRecord
                            >& threads
                        ) {
                            const auto existing =
                                std::find_if(
                                    threads.begin(),
                                    threads.end(),
                                    [thread](
                                        const ThreadRecord&
                                            record
                                    ) {
                                        return
                                            record.thread ==
                                            thread;
                                    }
                                );

                            if (
                                existing !=
                                threads.end()
                            ) {
                                resolvedThreadID =
                                    existing->id;

                                useCore =
                                    existing->coreID;

                                return;
                            }

                            uint8_t recordID =
                                requestedThreadID;

                            if (
                                assignedThreadID !=
                                nullptr
                            ) {
                                bool assigned =
                                    false;

                                for (
                                    unsigned int candidate =
                                        1;
                                    candidate <=
                                        std::numeric_limits<
                                            uint8_t
                                        >::max();
                                    ++candidate
                                ) {
                                    const uint8_t
                                        candidateID =
                                            static_cast<
                                                uint8_t
                                            >(
                                                candidate
                                            );

                                    const bool inUse =
                                        std::any_of(
                                            threads.begin(),
                                            threads.end(),
                                            [candidateID](
                                                const ThreadRecord&
                                                    record
                                            ) {
                                                return
                                                    record.id ==
                                                    candidateID;
                                            }
                                        );

                                    if (!inUse) {
                                        recordID =
                                            candidateID;

                                        assigned =
                                            true;

                                        break;
                                    }
                                }

                                // Preserve one-based IDs until all 255 are
                                // occupied, then use zero as the last distinct
                                // uint8_t value.
                                if (!assigned) {
                                    const bool zeroInUse =
                                        std::any_of(
                                            threads.begin(),
                                            threads.end(),
                                            [](
                                                const ThreadRecord&
                                                    record
                                            ) {
                                                return
                                                    record.id ==
                                                    0;
                                            }
                                        );

                                    if (!zeroInUse) {
                                        assigned =
                                            true;
                                    }
                                }

                                if (!assigned) {
                                    throw
                                        ThreadLimitExceededException();
                                }
                            } else {
                                const bool idInUse =
                                    std::any_of(
                                        threads.begin(),
                                        threads.end(),
                                        [recordID](
                                            const ThreadRecord&
                                                record
                                        ) {
                                            return
                                                record.id ==
                                                recordID;
                                        }
                                    );

                                if (idInUse) {
                                    throw
                                        ThreadDuplicateIDException(
                                            recordID
                                        );
                                }
                            }

                            // Keep the counter locked until insertion commits
                            // so failed vector allocation does not advance the
                            // round-robin state.
                            _nextCoreID.WithWriteLock(
                                [&](int& nextCoreID) {
                                    const int coreCount =
                                        _getCoreCount();

                                    useCore =
                                        nextCoreID %
                                        coreCount;

                                    threads.push_back({
                                        recordID,
                                        thread,
                                        useCore
                                    });

                                    nextCoreID =
                                        (useCore + 1) %
                                        coreCount;
                                }
                            );

                            resolvedThreadID =
                                recordID;
                        }
                    );

                    if (
                        assignedThreadID !=
                        nullptr
                    ) {
                        *assignedThreadID =
                            resolvedThreadID;
                    }

                    return useCore;
                }


                void RemoveThread(
                    IThread* thread
                ) {
                    _threads.WithWriteLock(
                        [thread](
                            std::vector<
                                ThreadRecord
                            >& threads
                        ) {
                            threads.erase(
                                std::remove_if(
                                    threads.begin(),
                                    threads.end(),
                                    [thread](
                                        const ThreadRecord&
                                            record
                                    ) {
                                        return
                                            record.thread ==
                                            thread;
                                    }
                                ),
                                threads.end()
                            );
                        }
                    );
                }


                void RemoveThread(
                    uint8_t threadID
                ) {
                    _threads.WithWriteLock(
                        [threadID](
                            std::vector<
                                ThreadRecord
                            >& threads
                        ) {
                            const auto matching =
                                std::find_if(
                                    threads.begin(),
                                    threads.end(),
                                    [threadID](
                                        const ThreadRecord&
                                            record
                                    ) {
                                        return
                                            record.id ==
                                            threadID;
                                    }
                                );

                            if (
                                matching !=
                                threads.end()
                            ) {
                                threads.erase(
                                    matching
                                );
                            }
                        }
                    );
                }


                void ForEachThread(
                    std::function<
                        void(IThread*)
                    > callback
                ) {
                    IterationGuard iteration(
                        *this
                    );

                    std::vector<
                        ThreadRecord
                    > snapshot;

                    _threads.WithSharedReadLock(
                        [&snapshot](
                            const std::vector<
                                ThreadRecord
                            >& threads
                        ) {
                            snapshot = threads;
                        }
                    );

                    for (
                        const ThreadRecord&
                            record :
                        snapshot
                    ) {
                        callback(
                            record.thread
                        );
                    }
                }


                bool WithThread(
                    uint8_t threadID,
                    std::function<
                        void(IThread*)
                    > callback
                ) {
                    IterationGuard iteration(
                        *this
                    );

                    ThreadRecord result{
                        0,
                        nullptr,
                        0
                    };

                    _threads.WithSharedReadLock(
                        [
                            threadID,
                            &result
                        ](
                            const std::vector<
                                ThreadRecord
                            >& threads
                        ) {
                            for (
                                const ThreadRecord&
                                    record :
                                threads
                            ) {
                                if (
                                    record.id ==
                                    threadID
                                ) {
                                    result =
                                        record;

                                    break;
                                }
                            }
                        }
                    );

                    if (
                        result.thread ==
                        nullptr
                    ) {
                        return false;
                    }

                    callback(
                        result.thread
                    );

                    return true;
                }


                IThread* GetThread(
                    uint8_t threadID
                ) {
                    IThread* result =
                        nullptr;

                    _threads.WithSharedReadLock(
                        [
                            threadID,
                            &result
                        ](
                            const std::vector<
                                ThreadRecord
                            >& threads
                        ) {
                            for (
                                const ThreadRecord&
                                    record :
                                threads
                            ) {
                                if (
                                    record.id ==
                                    threadID
                                ) {
                                    result =
                                        record.thread;

                                    break;
                                }
                            }
                        }
                    );

                    return result;
                }


                void CleanUp() {
                    std::vector<IThread*>
                        deleteThreads;

                    std::vector<
                        ThreadRecord
                    > snapshot;

                    std::vector<
                        ThreadRecord
                    > claimedRecords;

                    std::unique_lock<
                        std::recursive_mutex
                    > iterationLock(
                        _iterationMutex
                    );

                    if (_activeIterations > 0) {
                        _cleanupPending = true;
                        return;
                    }

                    _threads.WithSharedReadLock(
                        [&snapshot](
                            const std::vector<
                                ThreadRecord
                            >& threads
                        ) {
                            snapshot = threads;
                        }
                    );

                    // No list lock while calling virtual methods.
                    for (
                        const ThreadRecord&
                            record :
                        snapshot
                    ) {
                        if (
                            record.thread !=
                                nullptr &&
                            record.thread->
                                GetThreadState() ==
                                ThreadState::
                                    Terminated &&
                            record.thread->
                                TryClaimAutomaticCleanup()
                        ) {
                            claimedRecords.
                                push_back(
                                    record
                                );
                        }
                    }

                    _threads.WithWriteLock(
                        [
                            &claimedRecords,
                            &deleteThreads
                        ](
                            std::vector<
                                ThreadRecord
                            >& threads
                        ) {
                            for (
                                const ThreadRecord&
                                    claimed :
                                claimedRecords
                            ) {
                                const auto current =
                                    std::find_if(
                                        threads.begin(),
                                        threads.end(),
                                        [&claimed](
                                            const ThreadRecord&
                                                record
                                        ) {
                                            return
                                                record.id ==
                                                    claimed.id &&
                                                record.thread ==
                                                    claimed.thread;
                                        }
                                    );

                                if (
                                    current !=
                                    threads.end()
                                ) {
                                    deleteThreads.
                                        push_back(
                                            current->
                                                thread
                                        );

                                    threads.erase(
                                        current
                                    );
                                }
                            }
                        }
                    );

                    iterationLock.unlock();

                    // Destructors/callbacks may re-enter ThreadManager.
                    for (
                        auto thread :
                        deleteThreads
                    ) {
                        delete thread;
                    }
                }


                std::vector<
                    ThreadInitializationResult
                > InitializeWithResults() {
                    IterationGuard iteration(
                        *this
                    );

                    std::vector<
                        ThreadRecord
                    > snapshot;

                    std::vector<
                        ThreadInitializationResult
                    > results;

                    _threads.WithSharedReadLock(
                        [&snapshot](
                            const std::vector<
                                ThreadRecord
                            >& threads
                        ) {
                            snapshot = threads;
                        }
                    );

                    results.reserve(
                        snapshot.size()
                    );

                    for (
                        const ThreadRecord&
                            record :
                        snapshot
                    ) {
                        ThreadInitializationStatus
                            status =
                                ThreadInitializationStatus::
                                    InitializationException;

                        try {
                            status =
                                record.thread->
                                    Initialize();
                        } catch (...) {
                            // Custom IThread implementations do not have to
                            // provide Thread's internal exception containment.
                        }

                        results.push_back({
                            record.id,
                            status
                        });
                    }

                    return results;
                }


                void Initialize() {
                    static_cast<void>(
                        InitializeWithResults()
                    );
                }


                std::size_t GetThreadCount() {
                    std::size_t result = 0;

                    _threads.WithSharedReadLock(
                        [&result](
                            const std::vector<
                                ThreadRecord
                            >& threads
                        ) {
                            result =
                                threads.size();
                        }
                    );

                    return result;
                }
        };

    }

}
