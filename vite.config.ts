import { defineConfig } from "vite";

export default defineConfig({
  build: {
    target: "esnext",
  },
  server: {
    fs: {
      strict: false,
    },
  },
});
