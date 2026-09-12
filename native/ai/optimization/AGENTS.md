# Coach Optimization Instructions

`native/ai/optimization/` may reduce provider input size or avoid unnecessary model work, but it must never remove authoritative validation or change engine truth.

Prefer deterministic cheap gates before local/remote model calls. Provider evidence trimming must keep the original full evidence set available to native validation.
