import loadFastTracingWasm from "./bazel-bin/src/fast_tracing.js";

const FastTracingWasm = await loadFastTracingWasm();

const getImGuiVersion = FastTracingWasm.cwrap("GetImGuiVersion", "string", []);

console.log(getImGuiVersion());
