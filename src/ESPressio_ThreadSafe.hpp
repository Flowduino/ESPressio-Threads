#pragma once

// define CORE_THREADING_DEBUG in your project to enable debugging!

// System Includes
#define max max // Fixes bugs with mutex and std::max
#define min min // Fixes bugs with mutex and std::min
#include <mutex>
#include <shared_mutex>

// Library Includes

namespace ESPressio {

    namespace Threads {

        /*
            `IThreadSafe` is a common Interface for all Thread-Safe Object Wrappers provided by this library.
            You can use it to reference any Thread-Safe Object without knowing the actual type.
        */
        template <typename T>
        class IThreadSafe {
            public:
            // Methods
                virtual T Get() = 0;
                virtual std::pair<bool, T> TryGet(T defaultValue) = 0;
                virtual void Set(T value) = 0;
                virtual bool TrySet(T value) = 0;
                virtual bool IsLockedRead() = 0;
                virtual bool IsLockedWrite() = 0;
                virtual void WithReadLock(std::function<void(T&)> callback) = 0;
                virtual void WithWriteLock(std::function<void(T&)> callback) = 0;
                virtual bool TryWithReadLock(std::function<void(T&)> callback) = 0;
                virtual bool TryWithWriteLock(std::function<void(T&)> callback) = 0;
                virtual void ReleaseLock() = 0;
        };

        /*
            `Mutex` is a class that represents a Thread-Safe Object Wrapper for a given type.
            It is a wrapper around the system's Mutex API, designed to make them much easier to use.
            Mutexes are Exclusive-Read, Exclusive-Write, so only one thread can access the value at a time regardless of the operation.
        */
        template <typename T>
        class Mutex : public IThreadSafe<T> {
            private:
            // Members
                T _value;
                std::mutex _mutex;
                std::function<void(T,T)> _onChange = nullptr;
                std::function<bool(T,T)> _onCompare = [&](T a, T b) -> bool { return a == b; };
            public:
            // Constructor/Destructor
                Mutex(T value, std::function<void(T,T)> onChange = nullptr, std::function<bool(T,T)> onCompare = nullptr) : _value(value), _onChange((onChange)) {
                    if (onCompare) != nullptr { _onCompare = onCompare; }
                }
            // Methods
                T Get() {
                    std::lock_guard<std::mutex> lock(_mutex);
                    return _value;
                } 

                /// Returns a union consisting of a Boolean to denote if the value was able to be obtained (due to thread-safe locking) and the value itself (or the given `defaultValue` if the lock was not obtained).
                std::pair<bool, T> TryGet(T defaultValue) {
                    if (!_mutex.try_lock()) {
                        return std::make_pair(false, defaultValue);
                    }
                    T value = _value;
                    _mutex.unlock();
                    return std::make_pair(true, value);
                }

                /// Returns the OnChange Callback for the `Mutex` object.
                std::function<void(T,T)> GetOnChange() {
                    std::lock_guard<std::mutex> lock(_mutex);
                    return _onChange;
                }

                void Set(T value) {
                    std::lock_guard<std::mutex> lock(_mutex);
                    T oldValue = _value;
                    if (_onCompare(oldValue, value)) { return; }
                    _value = value;
                    if (_onChange != nullptr) { (_onChange)(oldValue, value); }
                }

                /// Returns a boolean notifying you if the value was set successfully (assuming that the thread-safe lock was available at the point of request).
                bool TrySet(T value) {
                    if (!_mutex.try_lock()) {
                        return false;
                    }
                    _value = value;
                    _mutex.unlock();
                    return true;
                }

                /// Returns `true` if the `Mutex` object is locked, `false` otherwise.
                /// If `false` is returned, you have the lock.
                bool IsLockedRead() {
                    return !_mutex.try_lock();
                }

                /// Returns `true` if the `Mutex` object is locked, `false` otherwise.
                /// If `false` is returned, you have the lock.
                bool IsLockedWrite() {
                    return IsLockedRead();
                }

                /// Invokes the provided `callback` with the `Mutex` object locked.
                void WithReadLock(std::function<void(T&)> callback) {
                    _mutex.lock();
                    callback(_value);
                    _mutex.unlock();
                }

                void WithWriteLock(std::function<void(T&)> callback) {
                    _mutex.lock();
                    callback(_value);
                    _mutex.unlock();
                }

                /// Invokes the provided `callback` with the `Mutex` object locked, returning `false` if the thread-safe lock was not obtained.
                bool TryWithReadLock(std::function<void(T&)> callback) {
                    if (!_mutex.try_lock()) {
                        return false;
                    }
                    callback(_value);
                    _mutex.unlock();
                    return true;
                }

                bool TryWithWriteLock(std::function<void(T&)> callback) {
                    return TryWithReadLock(callback);
                }

                /// Releases the Lock on the `Mutex` object.
                /// You should only call this if you have previously called `IsLocked` and it returned `false`, or if you need to release the lock prior to the end of scope.
                void ReleaseLock() {
                    _mutex.unlock();
                }
        };

        /*
            `ReadWriteMutex` is a class that represents a Thread-Safe Object Wrapper for a given type.
            It is a wrapper around the system's Mutex API, designed to make them much easier to use.
            ReadWriteMutexes are Shared-Read, Exclusive-Write, so multiple threads can access the value at a time for reading, but only one thread can access the value at a time for writing.
        */
        template <typename T>
        class ReadWriteMutex : public IThreadSafe<T> {
            private:
            // Members
                T _value;
                std::shared_mutex _mutex;
                std::function<void(T,T)> _onChange = nullptr;
                std::function<bool(T,T)> _onCompare = [&](T a, T b) -> bool { return a == b; };
            public:
            // Constructor/Destructor
                ReadWriteMutex(T value, std::function<void(T,T)> onChange = nullptr, std::function<bool(T,T)> onCompare = nullptr) : _value(value), _onChange((onChange)) {
                    if (onCompare) != nullptr { _onCompare = onCompare; }
                }
            // Methods
                T Get() {
                    std::shared_lock<std::shared_mutex> lock(_mutex);
                    return _value;
                } 

                /// Returns a union consisting of a Boolean to denote if the value was able to be obtained (due to thread-safe locking) and the value itself (or the given `defaultValue` if the lock was not obtained).
                std::pair<bool, T> TryGet(T defaultValue) {
                    if (!_mutex.try_lock_shared()) {
                        return std::make_pair(false, defaultValue);
                    }
                    T value = _value;
                    _mutex.unlock();
                    return std::make_pair(true, value);
                }

                /// Returns the OnChange Callback for the `ReadWriteMutex` object.
                std::function<void(T,T)> GetOnChange() {
                    std::shared_lock<std::shared_mutex> lock(_mutex);
                    return _onChange;
                }

                void Set(T value) {
                    std::unique_lock<std::shared_mutex> lock(_mutex);
                    T oldValue = _value;
                    if (_onCompare(oldValue, value)) { return; }
                    _value = value;
                    if (_onChange != nullptr) { (_onChange)(oldValue, value); }
                }

                /// Returns a boolean notifying you if the value was set successfully (assuming that the thread-safe lock was available at the point of request).
                bool TrySet(T value) {
                    if (!_mutex.try_lock()) {
                        return false;
                    }
                    _value = value;
                    _mutex.unlock();
                    return true;
                }

                /// Returns `true` if the `ReadWriteMutex` object is locked for read, `false` otherwise.
                /// If `false` is returned, you have the lock.
                bool IsLockedRead() {
                    return !_mutex.try_lock_shared();
                }

                /// Returns `true` if the `ReadWriteMutex` object is locked for writing, `false` otherwise.
                /// If `false` is returned, you have the lock.
                bool IsLockedWrite() {
                    return !_mutex.try_lock();
                }

                /// Invokes the provided `callback` with the `ReadWriteMutex` object locked.
                void WithReadLock(std::function<void(T&)> callback) {
                    std::lock_guard<std::shared_mutex>
                    lock(_mutex);
                    callback(_value);
                }

                /// Invokes the provided `callback` with the `ReadWriteMutex` object locked, returning `false` if the thread-safe lock was not obtained.
                bool TryWithReadLock(std::function<void(T&)> callback) {
                    if (!_mutex.try_lock_shared()) {
                        return false;
                    }
                    callback(_value);
                    _mutex.unlock();
                    return true;
                }

                /// Releases the Lock on the `ReadWriteMutex` object.
                /// You should only call this if you have previously called `IsLocked` and it returned `false`, or if you need to release the lock prior to the end of scope.
                void ReleaseLock() {
                    _mutex.unlock();
                }

                /// Invokes the provided `callback` with the `ReadWriteMutex` object locked for writing.
                void WithWriteLock(std::function<void(T&)> callback) {
                    std::unique_lock<std::shared_mutex> lock(_mutex);
                    callback(_value);
                }

                /// Invokes the provided `callback` with the `ReadWriteMutex` object locked for writing, returning `false` if the thread-safe lock was not obtained.
                bool TryWithWriteLock(std::function<void(T&)> callback) {
                    if (!_mutex.try_lock()) {
                        return false;
                    }
                    callback(_value);
                    _mutex.unlock();
                    return true;
                }

                /// Releases the Lock on the `ReadWriteMutex` object for writing.
                /// You should only call this if you have previously called `IsLocked` and it returned `false`, or if you need to release the lock prior to the end of scope.
                void ReleaseWriteLock() {
                    _mutex.unlock();
                }
        };

    }
}