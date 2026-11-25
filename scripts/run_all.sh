#!/usr/bin/env bash
# End-to-end reproduction script for the Hot Dispatch Replication experiment.
#
# It builds the workloads, collects exact profiles with BOLT's instrumentation,
# produces three variants per workload (original / plain BOLT / BOLT+HDR) and
# measures them with an interleaved, order-randomised driver.
#
# Usage:  ./run_all.sh [workdir]
set -euo pipefail

BOLT=${BOLT:-/opt/bolt/bin/llvm-bolt}
BENCH=${BENCH:-/opt/bench}
WORK=${1:-${WORK:-/work}}
REPS=${REPS:-15}
CC=${CC:-clang-18}

mkdir -p "${WORK}"
cd "${WORK}"

# Flags required for BOLT to see relocations and to keep the jump table in a
# form BOLT can analyse: -Wl,-q keeps relocations, -fno-pie/-no-pie turns the
# PIC jump table into an absolute one, -fcf-protection=none avoids the
# `notrack` prefix on the indirect jump.
CFLAGS_COMMON="-O2 -g -fno-pie -no-pie -fcf-protection=none -Wl,-q"

echo "==> building workloads"
$CC $CFLAGS_COMMON -o vm_dispatch "${BENCH}/vm_dispatch.c"
$CC $CFLAGS_COMMON -o flat_kernel "${BENCH}/flat_kernel.c"
$CC $CFLAGS_COMMON -DSQLITE_THREADSAFE=1 -o sqlite3 \
    "${BENCH}/sqlite3/sqlite3.c" "${BENCH}/sqlite3/shell.c" -lpthread -ldl -lm

echo "==> preparing the SQLite database"
rm -f bench.db
./sqlite3 bench.db < "${BENCH}/sqlite_create.sql"

# ---------------------------------------------------------------------------
# profile(): instrument, run, collect the .fdata
# ---------------------------------------------------------------------------
profile() {
    local bin=$1; shift
    local name=$1; shift
    local runcmd=("$@")
    ${BOLT} "${bin}" -o "${name}.inst" -instrument >/dev/null
    rm -f /tmp/prof.fdata
    "${runcmd[@]}" >/dev/null
    cp /tmp/prof.fdata "${name}.fdata"
}

# ---------------------------------------------------------------------------
# build_variants(): plain BOLT and BOLT+HDR from the same profile
# ---------------------------------------------------------------------------
build_variants() {
    local bin=$1 profile=$2 tag=$3
    ${BOLT} "${bin}" -o "${tag}_bolt" -data="${profile}" \
        -reorder-blocks=ext-tsp -reorder-functions=hfsort -split-functions \
        > "${tag}_bolt.log" 2>&1
    ${BOLT} "${bin}" -o "${tag}_hdr" -data="${profile}" \
        -reorder-blocks=ext-tsp -reorder-functions=hfsort -split-functions \
        -replicate-hot-dispatch \
        > "${tag}_hdr.log" 2>&1
    grep -h "replicate-hot-dispatch:" "${tag}_hdr.log" || true
}

echo "==> vm_dispatch"
cp vm_dispatch vm_dispatch.orig
profile vm_dispatch vm_dispatch sh -c "./vm_dispatch.inst 200000"
build_variants vm_dispatch vm_dispatch.fdata vm_dispatch

echo "==> sqlite (scan mix, VDBE bound)"
profile sqlite3 sqlite sh -c "./sqlite.inst bench.db < ${BENCH}/scan.sql"
build_variants sqlite3 sqlite.fdata sqlite

echo "==> flat kernel (control: no dispatch, pass must not fire)"
cp flat_kernel flat_kernel.orig
profile flat_kernel flat_kernel sh -c "./flat_kernel.inst 2000"
build_variants flat_kernel flat_kernel.fdata flat_kernel

# ---------------------------------------------------------------------------
# measure(): interleaved, order-randomised
# ---------------------------------------------------------------------------
cat > run_vm.sh   <<'EOF'
#!/bin/sh
exec ./vm_dispatch.orig 200000
EOF
cat > run_vm_bolt.sh <<'EOF'
#!/bin/sh
exec ./vm_dispatch_bolt 200000
EOF
cat > run_vm_hdr.sh <<'EOF'
#!/bin/sh
exec ./vm_dispatch_hdr 200000
EOF
cat > run_sql.sh <<'EOF'
#!/bin/sh
cd "$(dirname "$0")"
for i in 1 2 3 4 5 6 7 8; do ./sqlite.orig bench.db < scan.sql >/dev/null || exit 1; done
EOF
cat > run_sql_bolt.sh <<'EOF'
#!/bin/sh
cd "$(dirname "$0")"
for i in 1 2 3 4 5 6 7 8; do ./sqlite_bolt bench.db < scan.sql >/dev/null || exit 1; done
EOF
cat > run_sql_hdr.sh <<'EOF'
#!/bin/sh
cd "$(dirname "$0")"
for i in 1 2 3 4 5 6 7 8; do ./sqlite_hdr bench.db < scan.sql >/dev/null || exit 1; done
EOF
chmod +x run_*.sh

echo "==> measuring"
python3 /opt/scripts/compare.py --reps "${REPS}" --shuffle --json vm.json \
    --run "orig::./run_vm.sh" --run "bolt::./run_vm_bolt.sh" \
    --run "bolt+hdr::./run_vm_hdr.sh"
python3 /opt/scripts/compare.py --reps "${REPS}" --shuffle --json sqlite.json \
    --run "orig::./run_sql.sh" --run "bolt::./run_sql_bolt.sh" \
    --run "bolt+hdr::./run_sql_hdr.sh"
python3 /opt/scripts/compare.py --reps "${REPS}" --shuffle --json flat.json \
    --run "orig::./flat_kernel.orig 2000" --run "bolt::./flat_kernel_bolt 2000" \
    --run "bolt+hdr::./flat_kernel_hdr 2000"

echo "==> done; results are in ${WORK}/*.json"
