#pragma once

// define CORE_THREADING_DEBUG in your project to enable debugging!

// System Includes
#include <vector>
#include <functional>

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
                ReadWriteMutex<BaseType_t> _nextCoreID = ReadWriteMutex<BaseType_t>(0);
                
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
                BaseType_t AddThread(IThread* thread) {
                    _threads.WithWriteLock([thread](std::vector<IThread*>& threads) {
                        threads.push_back(thread);
                    });
                    BaseType_t useCore = 0;
                    _nextCoreID.WithWriteLock([&useCore](BaseType_t& nextCoreID) {
                        useCore = nextCoreID;
                        nextCoreID = (nextCoreID + 1) % 2;
                    });
                    return useCore;
                }

                /// Removes a Thread from the `ThreadManager` for management.
                void RemoveThread(IThread* thread) {
                    _threads.WithWriteLock([thread](std::vector<IThread*>& threads) {
                        threads.erase(std::remove(threads.begin(), threads.end(), thread), threads.end());
                    });
                }

                /// Iterates through all Threads in the `ThreadManager`.
                void ForEachThread(std::function<void(IThread*)> callback) {
                    _threads.WithWriteLock([callback](std::vector<IThread*>& threads) {
                        for (auto thread : threads) {
                            callback(thread);
                        }
                    });
                }

                /// Returns the requested `Thread` by ID
                IThread* GetThread(uint8_t threadID) {
                    IThread* result = nullptr;
                    _threads.WithReadLock([threadID, &result](std::vector<IThread*>& threads) {
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

                    _threads.WithWriteLock([&deleteThreads](std::vector<IThread*>& threads) {
                        for (auto thread : threads) {
                            if (thread->GetThreadState() == ThreadState::Terminated && thread->GetFreeOnTerminate()) {
                                deleteThreads.push_back(thread);
                            }
                        }
                        // Now iterate deleteThreads and remove them from the threads list
                        for (auto thread : deleteThreads) {
                            free(thread);
                        }
                    });
                }

                /// Initializes all Threads in the `ThreadManager`.
                void Initialize() {
                    _threads.WithReadLock([](std::vector<IThread*>& threads) {
                        for (auto thread : threads) {
                            thread->Initialize();
                        }
                    });
                }

                uint8_t GetThreadCount() {
                    uint8_t result = 0;
                    _threads.WithReadLock([&result](std::vector<IThread*>& threads) {
                        result = threads.size();
                    });
                    return result;
                }
        };

    }
}