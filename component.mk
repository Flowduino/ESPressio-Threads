COMPONENT_ADD_INCLUDEDIRS := include
COMPONENT_SRCDIRS := src
CXXFLAGS += -fno-rtti
CXXFLAGS += -DESPRESSIO_THREADS
CXXFLAGS += -DESPRESSO_THREADS_VERSION_MAJOR=0
CXXFLAGS += -DESPRESSO_THREADS_VERSION_MINOR=0
CXXFLAGS += -DESPRESSO_THREADS_VERSION_PATCH=1
CXXFLAGS += -DESPRESSO_THREADS_VERSION_STRING=\"0.0.1\"