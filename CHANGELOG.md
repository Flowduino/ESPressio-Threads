# Changelog

All notable changes to this project are documented in this file.

The structure follows the principles of [Keep a
Changelog](https://keepachangelog.com/en/1.1.0/) and [Semantic
Versioning](https://semver.org/).

> **Historical note:** This changelog was reconstructed retrospectively
> from published GitHub Releases, tags, release notes, repository
> history, and the documented public API. Where an historical release
> had little or no release-note detail, the entry is intentionally terse
> rather than inferring unsupported intent.

## \[3.1.2\] - 2026-08-20

### Changed

-   Raised the minimum ESPressio Timing dependency to 2.2.2, carrying forward the Observable 3.0.1 baseline from Timing's dependency-refresh patch.
-   Raised the direct ESPressio Observable dependency floor from 3.0.0 to 3.0.1.
-   Updated package and ESP-IDF component version metadata for Threads 3.1.2.
-   No public Threads interfaces or runtime semantics changed.

## \[3.1.1\] - 2026-08-19

### Changed

-   Updated the required ESPressio Timing baseline to 2.2.1, matching the dependency-refresh release generation.
-   Bounded ESPressio Timing compatibility to the current 2.x major line (`>=2.2.1 <3.0.0`).
-   Bounded ESPressio Observable compatibility to the current 3.x major line (`>=3.0.0 <4.0.0`).
-   Updated package metadata and current installation/dependency guidance for Threads 3.1.1.

## \[3.1.0\] - 2026-08-19

### Added

-   Added `IThreadManagerObserver`.
-   Added `IThreadGarbageCollectorObserver`.
-   Added `IThreadTerminationDispatcherObserver`.
-   Added immutable Thread Manager thread snapshots.
-   Added garbage-collection result snapshots and cleanup-result
    reporting.
-   Added Thread Manager initialization summaries.
-   Added observation of thread registration/removal, automatic-cleanup
    claiming, deferred cleanup, and other relevant
    singleton-infrastructure lifecycle operations.

### Changed

-   Extended the existing Observer architecture from individual `Thread`
    and `PrecisionThread` instances to process-wide singleton
    infrastructure.
-   Kept the notification layer synchronous and independent of ESPressio
    Event so Event bridges can remain opt-in.

## \[3.0.0\] - 2026-08-18

### Added

-   Added generic `PrecisionThread<TTime, Traits>` timing representation
    support.
-   Added support for ordinary ESPressio Units, Serializable Units, and
    future Timing-compatible representations.
-   Added/updated examples for basic Threads, lifecycle observation,
    automatic garbage collection, Precision Threads, and Serializable
    Precision Threads.

### Changed

-   Migrated precision scheduling to ESPressio Timing 2.x.
-   Separated internal nanosecond scheduling from public Unit
    representation.
-   Updated the dependency baseline to Timing 2.x and Observable 3.x.

### Fixed

-   Corrected integration issues found while validating the 3.0 codebase
    against the newest ESPressio dependencies.

## \[2.0.0\] - 2026-08-13

### Changed

-   Changed Thread and PrecisionThread Observer registration APIs to
    return owning `Observable::ObserverHandlePtr` handles.
-   Adopted ESPressio Observable 3.0 ownership semantics.

### Fixed

-   Removed raw Observer-handle ownership/leak ambiguity.
-   Released retained PrecisionThread sample storage when the sampling
    window is changed or disabled.
-   Clarified process-lifetime Thread infrastructure ownership.

## \[1.4.1\]

### Fixed

-   Maintenance corrections to the 1.4 series.

## \[1.3.0\]

### Added

-   Expanded the Thread/PrecisionThread feature set and lifecycle
    infrastructure.

## \[1.2.0\]

### Added

-   Continued development of precision-threading and
    lifecycle-management capabilities.

## \[1.1.0\]

### Added

-   Extended the initial Thread abstraction with additional
    lifecycle/scheduling functionality.

## \[1.0.0\]

### Added

-   Initial public release of ESPressio Threads and its object-oriented
    ESP32/FreeRTOS Thread foundation.

> Releases 1.0.0 through 1.4.1 are preserved here as terse historical
> entries because their detailed release-note text is not fully exposed
> in the current reconstructed source set.
