set(COMPONENT_SRCDIRS
    "src"
)

set(COMPONENT_ADD_INCLUDEDIRS
    "src"
)

register_component()

target_compile_definitions(${COMPONENT_TARGET} PUBLIC -DESP32)