#!/usr/bin/env bash
set -euo pipefail

BIN="./bin/portctrl"

if [[ ! -x "${BIN}" ]]; then
  echo "integration tests: missing binary ${BIN}" >&2
  exit 1
fi

fail() {
  echo "integration tests: FAIL - $1" >&2
  exit 1
}

expect_exit() {
  local expected="$1"
  local name="$2"
  shift 2

  set +e
  "$@" >/dev/null 2>&1
  local code=$?
  set -e

  if [[ "${code}" -ne "${expected}" ]]; then
    fail "${name} expected exit ${expected}, got ${code}"
  fi
}

expect_exit_one_of() {
  local name="$1"
  shift

  local -a allowed=()
  while [[ "$1" != "--" ]]; do
    allowed+=("$1")
    shift
  done
  shift

  set +e
  "$@" >/dev/null 2>&1
  local code=$?
  set -e

  for expected in "${allowed[@]}"; do
    if [[ "${code}" -eq "${expected}" ]]; then
      return 0
    fi
  done

  fail "${name} expected one of [${allowed[*]}], got ${code}"
}

expect_exit 1 "no args usage" "${BIN}"
expect_exit 1 "unknown command usage" "${BIN}" nope
expect_exit 1 "invalid port usage" "${BIN}" who abc
expect_exit 1 "invalid option usage" "${BIN}" list --bad

# Clear PATH so lsof cannot be found; this should map to inspect/runtime failure.
expect_exit 2 "inspect failure without lsof" env PATH="" "${BIN}" list

BUSY_PORT=$( "${BIN}" list 2>/dev/null | awk -F'|' '
  /^\|/ {
    port=$2
    gsub(/[[:space:]]/, "", port)
    if (port != "" && port != "PORT") {
      print port
      exit
    }
  }
')

if [[ -z "${BUSY_PORT}" ]]; then
  echo "integration tests: no busy port found; skipping free command checks"
  echo "integration tests: ok"
  exit 0
fi

expect_exit 0 "free dry-run on busy port" "${BIN}" free "${BUSY_PORT}"
expect_exit_one_of "free apply on busy port" 0 3 4 -- "${BIN}" free "${BUSY_PORT}" --apply --yes

echo "integration tests: ok"
