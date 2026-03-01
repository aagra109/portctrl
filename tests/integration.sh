#!/usr/bin/env bash
set -euo pipefail

BIN="./bin/portctrl"
ALLOW_LISTENER_SETUP_SKIP="${PORTCTRL_INTEGRATION_ALLOW_SETUP_SKIP:-0}"

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
expect_exit 1 "invalid option usage" "${BIN}" who 3000 --bad
expect_exit 1 "invalid option usage" "${BIN}" list --bad

# Clear PATH so lsof cannot be found; this should map to inspect/runtime failure.
expect_exit 2 "inspect failure without lsof" env PATH="" "${BIN}" list

if ! command -v python3 >/dev/null 2>&1; then
  fail "python3 is required for integration tests"
fi
if ! command -v lsof >/dev/null 2>&1; then
  fail "lsof is required for integration tests"
fi

PORT_FILE=$(mktemp)
LISTENER_ERR_FILE=$(mktemp)
LISTENER_PID=""
TEST_PORT=""

cleanup() {
  if [[ -n "${LISTENER_PID}" ]]; then
    kill -9 "${LISTENER_PID}" >/dev/null 2>&1 || true
  fi
  rm -f "${PORT_FILE}" "${LISTENER_ERR_FILE}"
}
trap cleanup EXIT

# Start a dedicated temporary listener and ignore TERM/INT so force escalation can be tested.
PORT_FILE="${PORT_FILE}" python3 -c '
import os
import signal
import socket
import time

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", 0))
s.listen(1)

signal.signal(signal.SIGTERM, signal.SIG_IGN)
signal.signal(signal.SIGINT, signal.SIG_IGN)

with open(os.environ["PORT_FILE"], "w", encoding="utf-8") as fp:
    fp.write(str(s.getsockname()[1]))
    fp.flush()

while True:
    time.sleep(1)
' >/dev/null 2>"${LISTENER_ERR_FILE}" &
LISTENER_PID=$!

for _ in {1..50}; do
  if [[ -s "${PORT_FILE}" ]]; then
    TEST_PORT=$(cat "${PORT_FILE}")
    break
  fi
  if ! kill -0 "${LISTENER_PID}" >/dev/null 2>&1; then
    break
  fi
  sleep 0.1
done

if [[ -z "${TEST_PORT}" ]]; then
  if [[ "${ALLOW_LISTENER_SETUP_SKIP}" == "1" ]]; then
    echo "integration tests: unable to start dedicated listener; skipping free apply checks (PORTCTRL_INTEGRATION_ALLOW_SETUP_SKIP=1)"
    echo "integration tests: ok"
    exit 0
  fi
  if [[ -s "${LISTENER_ERR_FILE}" ]]; then
    echo "integration tests: listener setup stderr follows:" >&2
    cat "${LISTENER_ERR_FILE}" >&2
  fi
  fail "unable to start dedicated listener for free apply checks"
fi

if ! lsof -nP -iTCP:"${TEST_PORT}" -sTCP:LISTEN >/dev/null 2>&1; then
  fail "dedicated listener did not become visible on port ${TEST_PORT}"
fi

expect_exit 0 "free dry-run on dedicated test port" "${BIN}" free "${TEST_PORT}"
expect_exit 4 "free apply without force remains busy" "${BIN}" free "${TEST_PORT}" --apply --yes
expect_exit 0 "free apply with force resolves listener" "${BIN}" free "${TEST_PORT}" --apply --yes --force

echo "integration tests: ok"
