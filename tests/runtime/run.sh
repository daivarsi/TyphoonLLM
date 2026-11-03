#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
bench_tmp="$(mktemp -d)"
trap 'rm -f "$bench_tmp/prefill-test"; rmdir "$bench_tmp"' EXIT
cc -std=c11 -O2 -Itests/runtime/include -Isrc/src src/src/session.c tests/runtime/prefill_head_test.c -o "$bench_tmp/prefill-test"
"$bench_tmp/prefill-test"
