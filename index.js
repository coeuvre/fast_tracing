import loadFastTracingWasm from "./fast_tracing.js";
const FastTracingWasm = await loadFastTracingWasm();

/** @type {WebAssembly.Memory} */
const wasmMemory = FastTracingWasm.wasmMemory;

const app_new = FastTracingWasm.cwrap("app_new", "number", []);
const app_is_loading = FastTracingWasm.cwrap("app_is_loading", "bool", [
  "number",
]);
const app_begin_load = FastTracingWasm.cwrap("app_begin_load", null, [
  "number",
]);
const app_get_input_buffer = FastTracingWasm.cwrap(
  "app_get_input_buffer",
  "number",
  ["number", "number"],
);
const app_submit_input = FastTracingWasm.cwrap("app_submit_input", null, [
  "number",
]);
const app_end_load = FastTracingWasm.cwrap("app_end_load", null, ["number"]);

const app = app_new();

const canvas = document.getElementById("canvas");
canvas.width = window.innerWidth;
canvas.height = window.innerHeight;

window.onresize = () => {
  canvas.width = window.innerWidth;
  canvas.height = window.innerHeight;
};

async function decodeJson(file) {
  const stream = file.stream();
  const reader = stream.getReader();
  const decoder = new TextDecoder();
  const chunks = [];
  while (true) {
    const { done, value } = await reader.read();
    if (done) {
      break;
    }
    const chunk = decoder.decode(value, { stream: true });
    chunks.push(chunk);
  }

  const content = chunks.join("");

  JSON.parse(content);
}

canvas.addEventListener("drop", async (event) => {
  event.preventDefault();

  if (app_is_loading(app)) {
    return;
  }

  const file = event.dataTransfer.files[0];
  const start = performance.now();

  // await decodeJson(file);
  const stream = file.stream();
  const reader = stream.getReader();
  app_begin_load(app);
  while (app_is_loading(app)) {
    const { done, value } = await reader.read();
    if (done) {
      break;
    }
    const offset = app_get_input_buffer(app, value.length);
    const buffer = new Uint8Array(wasmMemory.buffer, offset, value.length);
    buffer.set(value);
    app_submit_input(app);
  }
  app_end_load(app);

  const end = performance.now();
  const duration_s = (end - start) / 1000;
  console.log("Time: " + duration_s);
  console.log("Speed: " + file.size / duration_s / 1024 / 1024);
});

canvas.addEventListener("dragover", (event) => {
  event.preventDefault();
});
