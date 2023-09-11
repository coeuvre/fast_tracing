import loadFastTracingWasm from "./fast_tracing.js";
const FastTracingWasm = await loadFastTracingWasm();

/** @type {WebAssembly.Memory} */
const wasmMemory = FastTracingWasm.wasmMemory;

const app_new = FastTracingWasm.cwrap("app_new", "number", []);
const app_is_loading = FastTracingWasm.cwrap("app_is_loading", "bool", [
  "number",
]);
const app_start_loading = FastTracingWasm.cwrap("app_start_loading", null, [
  "number",
]);
const app_lock_input = FastTracingWasm.cwrap("app_lock_input", "number", [
  "number",
  "number",
]);
const app_unlock_input = FastTracingWasm.cwrap("app_unlock_input", null, [
  "number",
]);
const app_on_chunk_done = FastTracingWasm.cwrap("app_on_chunk_done", null, [
  "number",
]);

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
  app_start_loading(app);
  while (app_is_loading(app)) {
    const { done, value } = await reader.read();
    if (done) {
      break;
    }
    if (value.length > 0) {
      const offset = app_lock_input(app, value.length);
      if (offset > 0) {
        const buffer = new Uint8Array(wasmMemory.buffer, offset, value.length);
        buffer.set(value);
      }
      app_unlock_input(app);
    }
  }
  if (app_is_loading(app)) {
    app_on_chunk_done(app);
  }

  const end = performance.now();
  const duration_s = (end - start) / 1000;
  console.log("Time: " + duration_s);
  console.log("Speed: " + file.size / duration_s / 1024 / 1024);
});

canvas.addEventListener("dragover", (event) => {
  event.preventDefault();
});
