set(COMPONENT_SRCDIRS
    "src"
)

set(COMPONENT_ADD_INCLUDEDIRS
    "src"
)

set(COMPONENT_REQUIRES
    "ESPressio_Timing"
    "ESPressio_Observable"
)

register_component()

target_compile_definitions(${COMPONENT_TARGET} PUBLIC -DESP32)
