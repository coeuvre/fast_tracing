import loadFastTracingWasm from "./bazel-bin/src/fast_tracing.js";

const FastTracingWasm = await loadFastTracingWasm();

console.log(FastTracingWasm._add(1, 2));
