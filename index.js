import loadFastTracingWasm from "./bazel-bin/src/fast_tracing.js";

const FastTracingWasm = await loadFastTracingWasm();

const app_new = FastTracingWasm.cwrap("app_new", "number", []);
const app_is_loading = FastTracingWasm.cwrap("app_is_loading", "bool", [
  "number",
]);
const app_start_loading = FastTracingWasm.cwrap("app_start_loading", null, [
  "number",
]);
const app_on_chunk = FastTracingWasm.cwrap("app_on_chunk", null, [
  "number",
  "array",
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

  const start = performance.now();
  JSON.parse(content);
  const end = performance.now();
  const duration_s = (end - start) / 1000;
  console.log(duration_s);
  console.log(file.size / duration_s / 1024 / 1024);
}

canvas.addEventListener("drop", async (event) => {
  event.preventDefault();

  if (app_is_loading(app)) {
    return;
  }

  const file = event.dataTransfer.files[0];

  app_start_loading(app);

  const stream = file.stream();
  const reader = stream.getReader();

  const start = performance.now();
  while (app_is_loading(app)) {
    const { done, value } = await reader.read();
    if (done) {
      break;
    }
    const chunk_size = 4096;
    let begin = 0;
    while (app_is_loading(app) && begin < value.length) {
      const end = Math.min(begin + chunk_size, value.length);
      const chunk = new Uint8Array(value.buffer.slice(begin, end));
      app_on_chunk(app, chunk, chunk.length);
      begin = end;
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
