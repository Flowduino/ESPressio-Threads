#pragma once

#include "ESPressio_Thread.hpp"
#include "ESPressio_ThreadManager.hpp"

#ifndef ESPRESSIO_THREAD_GARBAGE_COLLECTOR_STACK_SIZE
    #define ESPRESSIO_THREAD_GARBAGE_COLLECTOR_STACK_SIZE 2000 // You are encouraged to determine the appropriate stack size for your application, and define it in your platformio.ini file.
#endif

namespace ESPressio {

    namespace Threads {

        class IThreadGarbageCollector {
            public:             
                virtual void CleanUp() = 0;
        };

    }
}