"""Index cache optimization (Phase 8.3) — Forsyth-style triangle reorder."""

from __future__ import annotations

DEFAULT_CACHE_SIZE = 16


def _calc_vertex_score(cache_position: int, valence: int, cache_size: int) -> float:
    if cache_position < 0:
        if valence < 3:
            return 0.0
        return 2.0 * (valence ** -0.5)

    score = 0.75 * ((cache_size - cache_position) ** -1.5)
    if valence < 3:
        return score
    return score + 2.0 * (valence ** -0.5)


def optimize_triangle_list(
    indices: list[int],
    *,
    cache_size: int = DEFAULT_CACHE_SIZE,
) -> list[int]:
    """Return a cache-optimized indexed triangle list (same topology)."""
    if len(indices) < 3 or len(indices) % 3 != 0:
        return list(indices)

    tri_count = len(indices) // 3
    if tri_count <= 1:
        return list(indices)

    max_vertex = max(indices) + 1
    vertex_triangles: list[list[int]] = [[] for _ in range(max_vertex)]
    triangle_vertices: list[tuple[int, int, int]] = []
    for tri in range(tri_count):
        i0, i1, i2 = indices[tri * 3 : tri * 3 + 3]
        triangle_vertices.append((i0, i1, i2))
        vertex_triangles[i0].append(tri)
        vertex_triangles[i1].append(tri)
        vertex_triangles[i2].append(tri)

    live_triangles = [True] * tri_count
    emitted = [False] * tri_count
    cache: list[int] = []
    out: list[int] = []

    def cache_position(vertex: int) -> int:
        try:
            return cache.index(vertex)
        except ValueError:
            return -1

    def add_to_cache(vertex: int) -> None:
        nonlocal cache
        pos = cache_position(vertex)
        if pos >= 0:
            cache.pop(pos)
        cache.insert(0, vertex)
        if len(cache) > cache_size:
            cache.pop()

    def best_triangle() -> int | None:
        best_score = -1.0
        best_tri: int | None = None
        for tri, alive in enumerate(live_triangles):
            if not alive or emitted[tri]:
                continue
            score = 0.0
            for vertex in triangle_vertices[tri]:
                valence = sum(1 for t in vertex_triangles[vertex] if live_triangles[t])
                score += _calc_vertex_score(cache_position(vertex), valence, cache_size)
            if score > best_score:
                best_score = score
                best_tri = tri
        return best_tri

    while True:
        tri = best_triangle()
        if tri is None:
            for candidate, alive in enumerate(live_triangles):
                if alive and not emitted[candidate]:
                    tri = candidate
                    break
        if tri is None:
            break

        i0, i1, i2 = triangle_vertices[tri]
        out.extend((i0, i1, i2))
        emitted[tri] = True
        live_triangles[tri] = False
        for vertex in (i0, i1, i2):
            add_to_cache(vertex)

    if len(out) != len(indices):
        return list(indices)
    return out


def triangle_sets_equal(a: list[int], b: list[int]) -> bool:
    if len(a) != len(b) or len(a) % 3 != 0:
        return False

    def as_set(indices: list[int]) -> set[tuple[int, int, int]]:
        tris: set[tuple[int, int, int]] = set()
        for i in range(0, len(indices), 3):
            tris.add(tuple(sorted(indices[i : i + 3])))
        return tris

    return as_set(a) == as_set(b)


def estimate_cache_misses(indices: list[int], *, cache_size: int = DEFAULT_CACHE_SIZE) -> int:
    cache: list[int] = []
    misses = 0
    for index in indices:
        if index not in cache:
            misses += 1
            if index in cache:
                cache.remove(index)
            cache.insert(0, index)
            if len(cache) > cache_size:
                cache.pop()
    return misses