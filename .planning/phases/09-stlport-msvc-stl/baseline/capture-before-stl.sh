#!/usr/bin/env bash
# Phase 9 Wave 0 — pre-migration STLPort marker capture.
# Source: 09-RESEARCH.md §Validation Architecture §1 (verbatim).
# Run from repo root. Output committed at baseline/BEFORE-STL.txt.
set -e
OUT=".planning/phases/09-stlport-msvc-stl/baseline/BEFORE-STL.txt"
{
  echo "=== _STL:: namespace use ==="
  grep -rn "_STL::" src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453 || true
  echo
  echo "=== _STLP_* macros ==="
  grep -rn "_STLP_" src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453 || true
  echo
  echo "=== hash_map / hash_set refs ==="
  grep -rn "hash_map\|hash_set" src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453 || true
  echo
  echo "=== auto_ptr ==="
  grep -rn "auto_ptr" src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453 || true
  echo
  echo "=== binders / functor adaptors ==="
  grep -rn "binder1st\|binder2nd\|ptr_fun\|mem_fun" src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453 || true
  echo
  echo "=== <hash_map> / <hash_set> includes ==="
  grep -rn '#include\s*<hash_map>\|#include\s*<hash_set>' src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453 || true
  echo
  echo "=== <sgi/ refs ==="
  grep -rn "<sgi/" src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453 || true
  echo
  echo "=== SUMMARY ==="
  echo "_STLP_*    : $(grep -rln "_STLP_" src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453 | wc -l) files"
  echo "hash_map   : $(grep -rln "hash_map" src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453 | wc -l) files"
  echo "hash_set   : $(grep -rln "hash_set" src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453 | wc -l) files"
  echo "<hash_map> : $(grep -rln '#include\s*<hash_map>' src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453 | wc -l) files"
  echo "<hash_set> : $(grep -rln '#include\s*<hash_set>' src/ --include="*.cpp" --include="*.h" --exclude-dir=stlport453 | wc -l) files"
} > "$OUT"
echo "Wrote $OUT"
