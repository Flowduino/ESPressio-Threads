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

        /*
            `ThreadManager` is a Singleton class that manages all instances implementing `IThread` in the system.
            Call `ThreadManager::GetInstance()` to obtain the a Pointer to the Singleton Instance.
            All methods in `ThreadManager` are Thread-Safe.
        */
        class ThreadManager {
            private:
                // Members
                ReadWriteMutex<std::vector<IThread*>> _threads;
                ReadWriteMutex<int> _nextCoreID = ReadWriteMutex<int>(0);
                std::mutex _iterationMutex;
                std::size_t _activeIterations = 0;
                bool _cleanupPending = false;

                void _beginIteration() {
                    std::lock_guard<std::mutex> lock(_iterationMutex);
                    ++_activeIterations;
                }

                void _endIteration() {
                    bool runDeferredCleanup = false;

                    {
                        std::lock_guard<std::mutex> lock(_iterationMutex);
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
                ThreadManager() : _threads(std::vector<IThread*>()) {

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
                    _threads.WithWriteLock([&](std::vector<IThread*>& threads) {
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
                                    [candidateID](IThread* existingThread) {
                                        return existingThread->GetThreadID() == candidateID;
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
                                    [](IThread* existingThread) {
                                        return existingThread->GetThreadID() == 0;
                                    }
                                );

                                if (!zeroInUse) {
                                    assigned = true;
                                }
                            }

                            if (!assigned) {
                                throw ThreadLimitExceededException();
                            }
                        }

                        if (std::find(threads.begin(), threads.end(), thread) ==
                            threads.end()) {
                            threads.push_back(thread);
                        }
                    });

                    int useCore = 0;
                    _nextCoreID.WithWriteLock([&useCore](int& nextCoreID) {
                        const int coreCount = _getCoreCount();
                        useCore = nextCoreID % coreCount;
                        nextCoreID = (useCore + 1) % coreCount;
                    });
                    return useCore;
                }

                /// Removes a Thread from the `ThreadManager` for management.
                void RemoveThread(IThread* thread) {
                    _threads.WithWriteLock([thread](std::vector<IThread*>& threads) {
                        threads.erase(std::remove(threads.begin(), threads.end(), thread), threads.end());
                    });
                }

                /// Removes a Thread from the `ThreadManager` using its ID.
                void RemoveThread(uint8_t threadID) {
                    _threads.WithWriteLock([threadID](std::vector<IThread*>& threads) {
                        for (auto thread : threads) {
                            if (thread->GetThreadID() == threadID) {
                                threads.erase(std::remove(threads.begin(), threads.end(), thread), threads.end());
                                break;
                            }
                        }
                    });
                }

                /// Iterates through all Threads in the `ThreadManager`.
                void ForEachThread(std::function<void(IThread*)> callback) {
                    IterationGuard iteration(*this);
                    std::vector<IThread*> snapshot;

                    _threads.WithSharedReadLock([&snapshot](const std::vector<IThread*>& threads) {
                        snapshot = threads;
                    });

                    for (auto thread : snapshot) {
                        callback(thread);
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
                    IThread* result = nullptr;

                    _threads.WithSharedReadLock(
                        [threadID, &result](const std::vector<IThread*>& threads) {
                            for (auto thread : threads) {
                                if (thread->GetThreadID() == threadID) {
                                    result = thread;
                                    break;
                                }
                            }
                        }
                    );

                    if (result == nullptr) {
                        return false;
                    }

                    callback(result);
                    return true;
                }

                /// Returns a non-owning Thread pointer by ID. Prefer
                /// WithThread() when garbage collection may run concurrently.
                IThread* GetThread(uint8_t threadID) {
                    IThread* result = nullptr;
                    _threads.WithSharedReadLock([threadID, &result](const std::vector<IThread*>& threads) {
                        for (auto thread : threads) {
                            if (thread->GetThreadID() == threadID) {
                                result = thread;
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

                    // Keep the iteration pin while selecting and removing
                    // victims so no new snapshot can start in between.
                    std::unique_lock<std::mutex> iterationLock(_iterationMutex);
                    if (_activeIterations > 0) {
                        _cleanupPending = true;
                        return;
                    }

                    _threads.WithWriteLock([&deleteThreads](std::vector<IThread*>& threads) {
                        for (auto thread : threads) {
                            if (thread->GetThreadState() == ThreadState::Terminated && thread->GetFreeOnTerminate()) {
                                deleteThreads.push_back(thread);
                            }
                        }

                        for (auto thread : deleteThreads) {
                            threads.erase(std::remove(threads.begin(), threads.end(), thread), threads.end());
                        }
                    });

                    iterationLock.unlock();

                    // Destructors and callbacks may access ThreadManager, so
                    // deletion must happen after releasing the manager lock.
                    for (auto thread : deleteThreads) {
                        delete thread;
                    }
                }

                /// Initializes all Threads in the `ThreadManager`.
                void Initialize() {
                    IterationGuard iteration(*this);
                    std::vector<IThread*> snapshot;

                    _threads.WithSharedReadLock([&snapshot](const std::vector<IThread*>& threads) {
                        snapshot = threads;
                    });

                    for (auto thread : snapshot) {
                        thread->Initialize();
                    }
                }

                std::size_t GetThreadCount() {
                    std::size_t result = 0;
                    _threads.WithSharedReadLock([&result](const std::vector<IThread*>& threads) {
                        result = threads.size();
                    });
                    return result;
                }
        };

    }
}
