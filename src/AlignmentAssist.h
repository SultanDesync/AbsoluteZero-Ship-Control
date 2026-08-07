#pragma once

namespace AlignmentAssist {
/// Start the synchronized alignment worker. Repeated calls are harmless.
void Start();

/// Stop and join the worker. Safe during plugin/process teardown.
void Stop();
} // namespace AlignmentAssist
