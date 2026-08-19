#include <Arduino.h>

#include <ESPressio_Thread.hpp>
#include <ESPressio_ThreadManager.hpp>
#include <ESPressio_ThreadGarbageCollector.hpp>
#include <ESPressio_ThreadTerminationDispatcher.hpp>

#include <ESPressio_IThreadManagerObserver.hpp>
#include <ESPressio_IThreadGarbageCollectorObserver.hpp>
#include <ESPressio_IThreadTerminationDispatcherObserver.hpp>

using namespace ESPressio;

class InfrastructureObserver final :
    public Threads::IThreadManagerObserver,
    public Threads::IThreadGarbageCollectorObserver,
    public Threads::IThreadTerminationDispatcherObserver {

public:
    void OnThreadRegistered(
        Threads::IThread*,
        const Threads::ThreadManagerThreadSnapshot& snapshot
    ) override {
        Serial.printf(
            "Thread registered: id=%u core=%d\n",
            snapshot.ThreadID,
            snapshot.CoreID
        );
    }

    void OnThreadRemoved(
        const Threads::ThreadManagerThreadSnapshot& snapshot
    ) override {
        Serial.printf(
            "Thread removed: id=%u state=%u\n",
            snapshot.ThreadID,
            static_cast<unsigned int>(
                snapshot.State
            )
        );
    }

    void OnThreadCleanupCompleted(
        const Threads::ThreadManagerCleanupResult& result
    ) override {
        Serial.printf(
            "Manager cleanup: examined=%u claimed=%u removed=%u deleted=%u deferred=%u\n",
            static_cast<unsigned int>(result.ThreadsExamined),
            static_cast<unsigned int>(result.ThreadsClaimed),
            static_cast<unsigned int>(result.ThreadsRemoved),
            static_cast<unsigned int>(result.ThreadsDeleted),
            result.WasDeferred ? 1U : 0U
        );
    }

    void OnThreadGarbageCollectionCompleted(
        const Threads::ThreadGarbageCollectionResult& result
    ) override {
        Serial.printf(
            "GC completed: mode=%u deleted=%u\n",
            static_cast<unsigned int>(
                result.ExecutionMode
            ),
            static_cast<unsigned int>(
                result.ManagerResult.ThreadsDeleted
            )
        );
    }

    void OnThreadTerminationDispatchQueued(
        const Threads::ThreadManagerThreadSnapshot& snapshot
    ) override {
        Serial.printf(
            "Termination dispatch queued: id=%u\n",
            snapshot.ThreadID
        );
    }
};


class DemoThread final :
    public Threads::Thread {

private:
    uint8_t _iterations = 0;

protected:
    void OnLoop() override {
        Serial.printf(
            "Demo iteration %u\n",
            ++_iterations
        );

        if (_iterations == 3) {
            Terminate();
        }

        delay(250);
    }
};


InfrastructureObserver infrastructureObserver;

Observable::ObserverHandlePtr
    managerObserverHandle;

Observable::ObserverHandlePtr
    garbageCollectorObserverHandle;

Observable::ObserverHandlePtr
    terminationDispatcherObserverHandle;

DemoThread* demoThread = nullptr;


void setup() {
    Serial.begin(115200);

    managerObserverHandle =
        Threads::ThreadManager::
            GetInstance()->
            RegisterObserver(
                &infrastructureObserver
            );

    garbageCollectorObserverHandle =
        Threads::ThreadGarbageCollector::
            GetInstance()->
            RegisterObserver(
                &infrastructureObserver
            );

    terminationDispatcherObserverHandle =
        Threads::ThreadTerminationDispatcher::
            GetInstance()->
            RegisterObserver(
                &infrastructureObserver
            );

    /*
     * Construct after observer registration so the manager-registration
     * notification is visible in this example.
     */
    demoThread =
        new DemoThread();

    demoThread->SetFreeOnTerminate(
        true
    );

    demoThread->SetStartOnInitialize(
        true
    );

    Threads::ThreadManager::
        GetInstance()->
        Initialize();
}


void loop() {
    delay(1000);
}
