#pragma once

// define CORE_THREADING_DEBUG in your project to enable debugging!

// System Includes
#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <mutex>
#include <vector>

// Library Includes
#include "ESPressio_ThreadSafe.hpp"
#include "ESPressio_IThread.hpp"

namespace ESPressio {

    namespace Threads {

        struct ThreadInitializationResult {
            uint8_t threadID;
            ThreadInitializationStatus status;
        };

        /*
            `ThreadManager` is a Singleton class that manages all instances implementing `IThread` in the system.
            Call `ThreadManager::GetInstance()` to obtain the a Pointer to the Singleton Instance.
            All methods in `ThreadManager` are Thread-Safe.
        */
        class ThreadManager {
            private:
                struct ThreadRecord {
                    uint8_t id;
                    IThread* thread;
                    int coreID;

                    bool operator==(const ThreadRecord& other) const {
                        return id == other.id &&
                               thread == other.thread &&
                               coreID == other.coreID;
                    }
                };

                // Members
                ReadWriteMutex<std::vector<ThreadRecord>> _threads;
                ReadWriteMutex<int> _nextCoreID = ReadWriteMutex<int>(0);
                std::recursive_mutex _iterationMutex;
                std::size_t _activeIterations = 0;
                bool _cleanupPending = false;

                void _beginIteration() {
                    std::lock_guard<std::recursive_mutex> lock(
                        _iterationMutex
                    );
                    ++_activeIterations;
                }

                void _endIteration() {
                    bool runDeferredCleanup = false;

                    {
                        std::lock_guard<std::recursive_mutex> lock(
                            _iterationMutex
                        );
                        if (_activeIterations > 0) {
                            --_activeIterations;
                        }

                        if (_activeIterations == 0 && _cleanupPending) {
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
                        explicit IterationGuard(ThreadManager& manager)
                            : _manager(manager) {
                            _manager._beginIteration();
                        }

                        ~IterationGuard() {
                            _manager._endIteration();
                        }

                        IterationGuard(const IterationGuard&) = delete;
                        IterationGuard& operator=(const IterationGuard&) = delete;
                };

                static int _getCoreCount() {
                    #if defined(portNUM_PROCESSORS)
                        return portNUM_PROCESSORS > 0 ? portNUM_PROCESSORS : 1;
                    #elif defined(configNUMBER_OF_CORES)
                        return configNUMBER_OF_CORES > 0 ? configNUMBER_OF_CORES : 1;
                    #else
                        return 1;
                    #endif
                }
                
            protected:
                ThreadManager() : _threads(std::vector<ThreadRecord>()) {

                }
            public:
                /// Returns a Pointer to the Singleton Instance of `ThreadManager`.
                static ThreadManager* GetInstance() {
                    static ThreadManager* instance = new ThreadManager();
                    return instance;
                }

                /// Adds a Thread to the `ThreadManager` for management.
                int AddThread(
                    IThread* thread,
                    uint8_t* assignedThreadID = nullptr
                ) {
                    if (thread == nullptr) {
                        throw ThreadInvalidRegistrationException();
                    }

                    // Custom implementations may re-enter ThreadManager from
                    // GetThreadID(), so evaluate it before taking the list
                    // lock. Thread itself requests manager-assigned IDs.
                    const uint8_t requestedThreadID =
                        assignedThreadID == nullptr ?
                            thread->GetThreadID() : 0;
                    int useCore = 0;

                    _threads.WithWriteLock([&](std::vector<ThreadRecord>& threads) {
                        const auto existing = std::find_if(
                            threads.begin(),
                            threads.end(),
                            [thread](const ThreadRecord& record) {
                                return record.thread == thread;
                            }
                        );

                        if (existing != threads.end()) {
                            if (assignedThreadID != nullptr) {
                                *assignedThreadID = existing->id;
                            }
                            useCore = existing->coreID;
                            return;
                        }

                        uint8_t recordID = requestedThreadID;

                        if (assignedThreadID != nullptr) {
                            *assignedThreadID = 0;
                            bool assigned = false;

                            for (unsigned int candidate = 1;
                                 candidate <= std::numeric_limits<uint8_t>::max();
                                 ++candidate) {
                                const uint8_t candidateID =
                                    static_cast<uint8_t>(candidate);
                                const bool inUse = std::any_of(
                                    threads.begin(),
                                    threads.end(),
                                    [candidateID](const ThreadRecord& record) {
                                        return record.id == candidateID;
                                    }
                                );

                                if (!inUse) {
                                    *assignedThreadID = candidateID;
                                    assigned = true;
                                    break;
                                }
                            }

                            // Preserve the existing one-based IDs until all
                            // 255 are occupied, then use zero as the final
                            // distinct uint8_t value.
                            if (!assigned) {
                                const bool zeroInUse = std::any_of(
                                    threads.begin(),
                                    threads.end(),
                                    [](const ThreadRecord& record) {
                                        return record.id == 0;
                                    }
                                );

                                if (!zeroInUse) {
                                    assigned = true;
                                }
                            }

                            if (!assigned) {
                                throw ThreadLimitExceededException();
                            }

                            recordID = *assignedThreadID;
                        } else {
                            const bool idInUse = std::any_of(
                                threads.begin(),
                                threads.end(),
                                [recordID](const ThreadRecord& record) {
                                    return record.id == recordID;
                                }
                            );

                            if (idInUse) {
                                throw ThreadDuplicateIDException(recordID);
                            }
                        }

                        // Hold the core counter lock until insertion succeeds,
                        // so a failed vector allocation neither registers a
                        // partial record nor advances round-robin state.
                        _nextCoreID.WithWriteLock([&](int& nextCoreID) {
                            const int coreCount = _getCoreCount();
                            useCore = nextCoreID % coreCount;
                            threads.push_back({recordID, thread, useCore});
                            nextCoreID = (useCore + 1) % coreCount;
                        });
                    });
                    return useCore;
                }

                /// Removes a Thread from the `ThreadManager` for management.
                void RemoveThread(IThread* thread) {
                    _threads.WithWriteLock([thread](std::vector<ThreadRecord>& threads) {
                        threads.erase(
                            std::remove_if(
                                threads.begin(),
                                threads.end(),
                                [thread](const ThreadRecord& record) {
                                    return record.thread == thread;
                                }
                            ),
                            threads.end()
                        );
                    });
                }

                /// Removes a Thread from the `ThreadManager` using its ID.
                void RemoveThread(uint8_t threadID) {
                    _threads.WithWriteLock([threadID](std::vector<ThreadRecord>& threads) {
                        const auto matching = std::find_if(
                            threads.begin(),
                            threads.end(),
                            [threadID](const ThreadRecord& record) {
                                return record.id == threadID;
                            }
                        );

                        if (matching != threads.end()) {
                            threads.erase(matching);
                        }
                    });
                }

                /// Iterates through all Threads in the `ThreadManager`.
                void ForEachThread(std::function<void(IThread*)> callback) {
                    IterationGuard iteration(*this);
                    std::vector<ThreadRecord> snapshot;

                    _threads.WithSharedReadLock([&snapshot](const std::vector<ThreadRecord>& threads) {
                        snapshot = threads;
                    });

                    for (const ThreadRecord& record : snapshot) {
                        callback(record.thread);
                    }
                }

                /// Invokes a callback with the requested Thread pinned against
                /// manager-driven garbage collection for the callback's
                /// duration. Returns false when the ID is not registered.
                bool WithThread(
                    uint8_t threadID,
                    std::function<void(IThread*)> callback
                ) {
                    IterationGuard iteration(*this);
                    ThreadRecord result{0, nullptr, 0};

                    _threads.WithSharedReadLock(
                        [threadID, &result](const std::vector<ThreadRecord>& threads) {
                            for (const ThreadRecord& record : threads) {
                                if (record.id == threadID) {
                                    result = record;
                                    break;
                                }
                            }
                        }
                    );

                    if (result.thread == nullptr) {
                        return false;
                    }

                    callback(result.thread);
                    return true;
                }

                /// Returns a non-owning Thread pointer by ID. Prefer
                /// WithThread() when garbage collection may run concurrently.
                IThread* GetThread(uint8_t threadID) {
                    IThread* result = nullptr;
                    _threads.WithSharedReadLock([threadID, &result](const std::vector<ThreadRecord>& threads) {
                        for (const ThreadRecord& record : threads) {
                            if (record.id == threadID) {
                                result = record.thread;
                                break;
                            }
                        }
                    });
                    return result;
                }

                /*
                    Iterates through all Threads and destroys any that are Terminated AND have FreeOnTerminate() set to `true`.
                    This method is Thread-Safe, but you need to be 100% certain you only set `FreeOnTerminate` to `true` when you are managing the memory of the `Thread` yourself.
                */
                void CleanUp() {
                    std::vector<IThread*> deleteThreads;
                    std::vector<ThreadRecord> snapshot;
                    std::vector<ThreadRecord> claimedRecords;

                    // The recursive gate prevents another task from starting
                    // a manager iteration while allowing a custom IThread to
                    // re-enter ThreadManager from a virtual method below.
                    std::unique_lock<std::recursive_mutex> iterationLock(
                        _iterationMutex
                    );
                    if (_activeIterations > 0) {
                        _cleanupPending = true;
                        return;
                    }

                    _threads.WithSharedReadLock(
                        [&snapshot](const std::vector<ThreadRecord>& threads) {
                            snapshot = threads;
                        }
                    );

                    // No manager list lock is held while invoking virtual
                    // methods supplied by Thread or custom IThread types.
                    for (const ThreadRecord& record : snapshot) {
                        if (record.thread != nullptr &&
                            record.thread->GetThreadState() ==
                                ThreadState::Terminated &&
                            record.thread->TryClaimAutomaticCleanup()) {
                            claimedRecords.push_back(record);
                        }
                    }

                    _threads.WithWriteLock(
                        [&claimedRecords, &deleteThreads](
                            std::vector<ThreadRecord>& threads
                        ) {
                            for (const ThreadRecord& claimed : claimedRecords) {
                                const auto current = std::find_if(
                                    threads.begin(),
                                    threads.end(),
                                    [&claimed](const ThreadRecord& record) {
                                        return record.id == claimed.id &&
                                               record.thread == claimed.thread;
                                    }
                                );

                                if (current != threads.end()) {
                                    deleteThreads.push_back(current->thread);
                                    threads.erase(current);
                                }
                            }
                        }
                    );

                    iterationLock.unlock();

                    // Destructors and callbacks may access ThreadManager, so
                    // deletion must happen after releasing the manager lock.
                    for (auto thread : deleteThreads) {
                        delete thread;
                    }
                }

                /// Initializes all Threads and returns each initialization
                /// outcome in manager iteration order.
                std::vector<ThreadInitializationResult>
                InitializeWithResults() {
                    IterationGuard iteration(*this);
                    std::vector<ThreadRecord> snapshot;
                    std::vector<ThreadInitializationResult> results;

                    _threads.WithSharedReadLock([&snapshot](const std::vector<ThreadRecord>& threads) {
                        snapshot = threads;
                    });

                    results.reserve(snapshot.size());

                    for (const ThreadRecord& record : snapshot) {
                        ThreadInitializationStatus status =
                            ThreadInitializationStatus::InitializationException;

                        try {
                            status = record.thread->Initialize();
                        } catch (...) {
                            // Custom IThread implementations are not required
                            // to provide Thread's internal exception handling.
                        }

                        results.push_back({record.id, status});
                    }

                    return results;
                }

                /// Initializes all Threads while preserving the original API.
                /// Use InitializeWithResults() when outcomes are required.
                void Initialize() {
                    static_cast<void>(InitializeWithResults());
                }

                std::size_t GetThreadCount() {
                    std::size_t result = 0;
                    _threads.WithSharedReadLock([&result](const std::vector<ThreadRecord>& threads) {
                        result = threads.size();
                    });
                    return result;
                }
        };

    }
}
