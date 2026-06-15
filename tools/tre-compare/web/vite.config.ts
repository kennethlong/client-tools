/// <reference types="vitest/config" />
import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import tailwindcss from "@tailwindcss/vite";
import path from "node:path";

// https://vite.dev/config/
export default defineConfig({
  plugins: [react(), tailwindcss()],
  resolve: {
    // shadcn @/* alias -> ./src
    alias: { "@": path.resolve(__dirname, "./src") },
  },
  build: {
    // NEVER "dist" — tools/tre-compare/.gitignore ignores dist/ as the Python build
    // artifact (name collision). FastAPI static-mounts web/build/ (D-02).
    outDir: "build",
    emptyOutDir: true,
  },
  server: {
    // Dev-only: proxy the four API route prefixes to the FastAPI localhost port so the
    // SPA uses same-origin relative fetches in BOTH dev (proxied) and prod (same origin).
    proxy: {
      "/compare": "http://127.0.0.1:8765",
      "/file": "http://127.0.0.1:8765",
      "/installs": "http://127.0.0.1:8765",
    },
  },
  test: {
    // Vitest reads this config (Wave 0 frontend seam). jsdom for the eventual RTL
    // component tests; globals so `describe`/`it`/`expect` need no import.
    environment: "jsdom",
    globals: true,
  },
});
