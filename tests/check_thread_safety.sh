#!/bin/bash
#
# Thread-safety completeness gate for the quickerSnes9x core.
#
# A multi-threaded core is only safe if every writable global is either
# thread-local (TLS) or provably constant-after-init. This script enumerates
# every OBJECT symbol the core defines in a writable section (.data / .bss)
# that is NOT thread-local, and diffs that set against a reviewed allowlist of
# known-safe init-once / read-only globals. Any symbol not on the list fails
# the gate — it is a candidate data race that must be made __thread or proven
# const-after-init and added to the list (with justification).
#
# Usage: check_thread_safety.sh <path-to-quickerSnes9x-build-dir>
#   e.g. check_thread_safety.sh ../build
#
set -euo pipefail

BUILD=${1:-../build}
OBJDIR="$BUILD/quickerSnes9xTester.p"
if [ ! -d "$OBJDIR" ]; then echo "ERROR: core objects not found at $OBJDIR (build first)"; exit 2; fi

# Reviewed allowlist: shared writable globals proven safe to share.
# Each entry must have a documented reason (see tests/README or the audit notes).
#  - M1SNES/M2SNES : const SNES model tables ({1,3,2}/{2,4,3}); never written
#  - Model         : pointer set once at ROM load to &M1SNES/&M2SNES; same ROM across threads
#  - color masks   : idempotent init-write (identical value every thread; rendering disabled)
#  - conf          : function-static in S9xLoadConfigFiles; not on the headless emulation path
ALLOWLIST="$(cat <<'EOF'
M1SNES
M2SNES
Model
ALPHA_BITS_MASK
BLUE_HI_BIT_MASK
BLUE_LOW_BIT_MASK
FIRST_COLOR_MASK
FIRST_THIRD_COLOR_MASK
GREEN_HI_BIT
GREEN_HI_BIT_MASK
GREEN_LOW_BIT_MASK
HIGH_BITS_SHIFTED_TWO_MASK
MAX_BLUE
MAX_GREEN
MAX_RED
RED_HI_BIT_MASK
RED_LOW_BIT_MASK
RGB_HI_BITS_MASK
RGB_HI_BITS_MASKx2
RGB_LOW_BITS_MASK
RGB_REMOVE_LOW_BITS_MASK
SECOND_COLOR_MASK
SPARE_RGB_BIT_MASK
THIRD_COLOR_MASK
TWO_LOW_BITS_MASK
_ZZ18S9xLoadConfigFilesvE4conf
EOF
)"

# Collect every non-TLS OBJECT symbol the core defines in a writable section.
found=""
for o in "$OBJDIR"/source_quickerSnes9x_core_*.o; do
  # writable section indices (.data / .bss; exclude .data.rel.ro = const-after-reloc)
  widx=$(readelf -SW "$o" 2>/dev/null | sed 's/\[ /[/' \
        | awk '/\.bss|\.data[^.]|\.data$/{gsub(/\[|\]/,"",$1); print $1}' \
        | grep -vE "rel.ro" | tr '\n' '|' | sed 's/|$//')
  [ -z "$widx" ] && continue
  syms=$(readelf -sW "$o" 2>/dev/null | awk -v idx="$widx" '
    $4=="OBJECT" && $8!="" { split(idx,a,"|"); for(i in a) if($7==a[i]) print $8 }')
  found="$found$syms"$'\n'
done

# Strip compiler-generated noise (guard variables, vtables-in-bss, etc.)
found=$(echo "$found" | grep -vE '_ZGV|guard|::ms_|test_string|^$' | sort -u)

# Diff against allowlist
violations=$(comm -23 <(echo "$found") <(echo "$ALLOWLIST" | sort -u))

if [ -n "$violations" ]; then
  echo "[FAIL] Unreviewed shared writable global(s) — potential data race:"
  echo "$violations" | sed 's/^/    /'
  echo
  echo "Each must be made __thread, or proven const-after-init and added to the"
  echo "ALLOWLIST in $(basename "$0") with a justification."
  exit 1
fi

echo "[OK] Thread-safety gate passed: every shared writable global is on the reviewed allowlist."
echo "     (all mutable emulation state is thread-local)"
