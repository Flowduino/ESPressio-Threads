# ESPressio Threads

Object-oriented threading, precision iteration, lifecycle management,
and thread infrastructure for ESP32-family microcontrollers.

## Latest Stable Version

**3.1.0**

## ESPressio Development Platform

ESPressio is a collection of discrete, composable component libraries
built around a common development ethos:

-   **Light-weight** --- minimise memory consumption and runtime
    overhead without sacrificing correctness.
-   **Ease of use** --- provide strongly typed, developer-friendly
    abstractions over lower-level facilities.
-   **Object-oriented** --- a type for everything, and everything in a
    type.
-   **SOLID** --- favour focused responsibilities, extensibility,
    substitutable abstractions, narrow interfaces, and dependency
    inversion wherever practical on embedded C++ platforms.

## License

Licensed under the **Apache License 2.0**. See [LICENSE](LICENSE).

## ESPressio Library Dependencies

ESPressio is designed as a modular ecosystem of independently useful
libraries, with required dependencies kept explicit and optional
integrations introduced only when the corresponding functionality is
selected.

For a complete overview of required dependencies, opt-in dependencies,
and the overall hierarchy, see:

**[ESPressio Library Dependency Chart](ESPRESSIO_DEPENDENCY_CHART.md)**

-   **Solid relationships** represent required ESPressio dependencies.
-   **Dashed relationships** represent opt-in dependencies introduced
    only by the corresponding feature, integration, type, or header.

### Required ESPressio dependencies

-   **ESPressio Timing \>= 2.0.0**
-   **ESPressio Observable \>= 3.0.0**

## Thread

`Thread` provides an object-oriented abstraction over ESP32/FreeRTOS
task execution and lifecycle management.

## PrecisionThread

`PrecisionThread<TTime, Traits>` provides deterministic/periodic
iteration using ESPressio Timing and its generic time representation
model.

This is also the scheduling foundation used by ESPressio Event's
`PrecisionEventThread`.

## Singleton infrastructure

Threads includes process-lifetime infrastructure for thread management,
garbage collection, and termination coordination.

### Thread Manager

Coordinates thread lifecycle and exposes meaningful lifecycle/state
changes through Observable notifications.

### Garbage Collector

Handles deferred thread cleanup and exposes relevant garbage-collection
lifecycle notifications through Observable rather than relying solely on
bespoke callbacks.

### Termination Dispatcher

Coordinates termination behaviour and likewise exposes logical lifecycle
notifications.

## Observer integration

Version 3.1 systematically applies ESPressio Observable where
synchronous infrastructure notifications make sense:

``` text
Threads singleton operation
    -> Observable callback
        +--> application observer
        +--> optional Event bridge
```

Threads does **not** depend on ESPressio Event.

Event supplies optional bridges such as:

``` cpp
ThreadManagerEventBridge
ThreadGarbageCollectorEventBridge
ThreadTerminationDispatcherEventBridge
```

with Serializable counterparts when selected.

## Relationship with Timing

Threads consumes Timing for scheduling and precision-iteration
contracts. Timing's generic representation model therefore propagates
naturally into precision Thread types.

## Design goals

-   Object-oriented FreeRTOS task management.
-   Explicit lifecycle ownership.
-   Precision periodic execution.
-   Safe singleton infrastructure.
-   Observable lifecycle/state changes.
-   No upward dependency on Event.
