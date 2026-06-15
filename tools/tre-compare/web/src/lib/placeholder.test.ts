import { describe, expect, it } from "vitest";

// Wave-0 placeholder: proves the Vitest runner is wired (vite.config test block +
// `npm run test`). Wave 2 (plan 30-03) replaces this with the real
// tree.test.ts / status.test.ts unit suites.
describe("vitest harness", () => {
  it("runs", () => {
    expect(1 + 1).toBe(2);
  });
});
