#pragma once

namespace gecko::runtime {
class TrackingAllocator;
}

namespace gecko::examples::core_example {

/// One self-contained tour of a piece of the Core API.
/// Implementations live in Demos.cpp.
namespace demos {

/// Allocates, frees, and reports per-label stats from `TrackingAllocator`.
void RunMemory(::gecko::runtime::TrackingAllocator& tracker);

/// Sends events with both Immediate and Queued delivery and shows the
/// difference between Send-time and Dispatch-time delivery.
void RunEvents();

/// Demonstrates `ThisThreadId`, `HardwareThreadCount`, sleep/yield, and
/// `HighResTimeNs`.
void RunThreading();

/// Submits jobs in parallel, builds a 3-stage dependency pipeline, and
/// processes jobs on the main thread.
void RunJobs(::gecko::runtime::TrackingAllocator& tracker);

/// Emits messages at every log level so users can see them in the console
/// sink and the file sink.
void RunLogging();

/// Reads `IProfiler::GetDiagnostics()` and logs the counters.
void RunProfilerDiagnostics();

}  // namespace demos

}  // namespace gecko::examples::core_example
