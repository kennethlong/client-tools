// Typed, relative-path fetch client for the four frozen FastAPI routes.
//
// RELATIVE PATHS ONLY (no hardcoded host) — the Vite dev proxy (web/vite.config.ts) forwards
// /compare /file /installs to the dev FastAPI port, and prod serves the SPA same-origin from
// the same FastAPI process, so one code path works in both (RESEARCH Anti-Patterns; D-02).
//
// On a non-ok response the client throws an `ApiError` carrying the parsed `{detail}` envelope
// so callers can surface the route's error copy:
//   - /file/detail 404 -> { detail: { error: "not found", path } }
//   - /file/detail 400 -> { detail: { error: "rejected path", path } }
//   - bad cfg path  400 -> { detail: { error: "cfg path missing or unreadable", cfg } }

import type {
  DetailResponse,
  ErrorDetail,
  FileDetailReq,
  FilesResponse,
  InstallEntry,
  PairReq,
  SetResponse,
} from "./types";

// Thrown on any non-2xx response. `detail` is the parsed `{error, path?/cfg?}` envelope (or
// the raw body / {} when the response was not JSON).
export class ApiError extends Error {
  readonly status: number;
  readonly detail: ErrorDetail | Record<string, unknown>;

  constructor(status: number, detail: ErrorDetail | Record<string, unknown>) {
    const msg =
      detail && typeof detail === "object" && "error" in detail
        ? String((detail as ErrorDetail).error)
        : `request failed (${status})`;
    super(msg);
    this.name = "ApiError";
    this.status = status;
    this.detail = detail;
  }
}

// Unwrap a FastAPI error body. FastAPI wraps HTTPException detail under a top-level `detail`
// key — surface that inner object so callers see `{error, path/cfg}` directly.
function unwrapDetail(body: unknown): ErrorDetail | Record<string, unknown> {
  if (body && typeof body === "object" && "detail" in body) {
    const d = (body as { detail: unknown }).detail;
    if (d && typeof d === "object") return d as ErrorDetail;
  }
  return (body as Record<string, unknown>) ?? {};
}

async function postJson<T>(url: string, body: PairReq | FileDetailReq): Promise<T> {
  const res = await fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  if (!res.ok) {
    const parsed = await res.json().catch(() => ({}));
    throw new ApiError(res.status, unwrapDetail(parsed));
  }
  return (await res.json()) as T;
}

// GET /installs -> [{ name, cfg_path }]. An empty list is a valid empty picker, not an error.
export async function fetchInstalls(): Promise<InstallEntry[]> {
  const res = await fetch("/installs");
  if (!res.ok) {
    const parsed = await res.json().catch(() => ({}));
    throw new ApiError(res.status, unwrapDetail(parsed));
  }
  return (await res.json()) as InstallEntry[];
}

// POST /compare/set -> { rows } set-level archive diff.
export function compareSet(body: PairReq): Promise<SetResponse> {
  return postJson<SetResponse>("/compare/set", body);
}

// POST /compare/files -> { rows, summary } full file-level diff (no pagination, D-08).
export function compareFiles(body: PairReq): Promise<FilesResponse> {
  return postJson<FilesResponse>("/compare/files", body);
}

// POST /file/detail -> drill-in winner + shadowed + per-side hash + verdict. 404/400 throw.
export function fileDetail(body: FileDetailReq): Promise<DetailResponse> {
  return postJson<DetailResponse>("/file/detail", body);
}
