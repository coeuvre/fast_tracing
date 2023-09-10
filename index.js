// import loadFastTracingWasm from "./bazel-bin/src/fast_tracing.js";

// const FastTracingWasm = await loadFastTracingWasm();

// const getImGuiVersion = FastTracingWasm.cwrap("GetImGuiVersion", "string", []);

// console.log(getImGuiVersion());

const canvas = document.getElementById("canvas");
canvas.width = window.innerWidth;
canvas.height = window.innerHeight;

window.onresize = () => {
  canvas.width = window.innerWidth;
  canvas.height = window.innerHeight;
};

canvas.addEventListener("drop", async (event) => {
  event.preventDefault();

  const file = event.dataTransfer.files[0];

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
});

canvas.addEventListener("dragover", (event) => {
  event.preventDefault();
});
