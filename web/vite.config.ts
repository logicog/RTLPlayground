import { defineConfig } from "vite";
import minifyLiterals from "rollup-plugin-html-literals";

export default defineConfig({
  base: "/",
  plugins: [
    minifyLiterals({
      include: ["**/*.ts"],
      failOnError: true,
    }),
  ],
  esbuild: { legalComments: "none" },
  build: {
    target: "es2020",
    minify: "esbuild",
    cssMinify: "esbuild",
    sourcemap: false,
    modulePreload: false,
    cssCodeSplit: false,
    rollupOptions: {
      output: {
        entryFileNames: "app.js",
        chunkFileNames: (chunk) => `${chunk.name.replace(/[^A-Za-z0-9_]/g, "_")}.js`,
        assetFileNames: "style.[ext]",
        manualChunks: (id) => {
          const shared = [
            "/src/api.ts",
            "/src/shared.ts",
            "/src/pages/ports.ts",
            "/src/domain/ports.ts",
            "/src/domain/port-networks.ts",
            "/src/domain/session.ts",
            "/src/domain/entity-colors.ts",
          ];
          if (
            id.includes("node_modules") ||
            id.includes("/src/components/") ||
            shared.some((path) => id.endsWith(path))
          )
            return "core";
        },
      },
    },
  },
  // No production-switch proxy. Development and browser tests use a local mock.
});
