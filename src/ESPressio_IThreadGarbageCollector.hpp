#pragma once

namespace ESPressio {

    namespace Threads {

        class IThreadGarbageCollector {
            public:             
                virtual ~IThreadGarbageCollector() = default;
                virtual void CleanUp() = 0;
        };

    }
}
