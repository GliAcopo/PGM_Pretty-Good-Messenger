#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

PORT="${PGM_TEST_PORT:-6666}"
if ! [[ "${PORT}" =~ ^[0-9]+$ ]]; then
  echo "PGM_TEST_PORT must be numeric"
  exit 1
fi
if [ "${PORT}" -lt 1024 ] || [ "${PORT}" -gt 65535 ]; then
  echo "PGM_TEST_PORT must be between 1024 and 65535 for this test"
  exit 1
fi

USER="pgm_test_user_$(date +%s)"
PASS="TestPass_123"
USER_DIR="${ROOT}/${USER}.pgmusr"
SERVER_LOG="/tmp/pgm_server_${USER}.log"
CLIENT_REG_LOG="/tmp/pgm_client_reg_${USER}.log"
CLIENT_WRONG_LOG="/tmp/pgm_client_wrong_${USER}.log"
CLIENT_OK_LOG="/tmp/pgm_client_ok_${USER}.log"

if [ -e "${USER_DIR}" ]; then
  echo "Test user directory already exists: ${USER_DIR}"
  exit 1
fi

SERVER_PID=""
cleanup() {
  if [ -n "${SERVER_PID}" ] && kill -0 "${SERVER_PID}" 2>/dev/null; then
    kill "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

make -C "${ROOT}"

"${ROOT}/bin/server" "${PORT}" >"${SERVER_LOG}" 2>&1 &
SERVER_PID=$!

for _ in $(seq 1 20); do
  if grep -q "Server listening on IP" "${SERVER_LOG}"; then
    break
  fi
  if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
    echo "Server exited early, log:"
    tail -n 50 "${SERVER_LOG}"
    exit 1
  fi
  sleep 0.2
done

if ! grep -q "Server listening on IP" "${SERVER_LOG}"; then
  echo "Server did not start in time, log:"
  tail -n 50 "${SERVER_LOG}"
  exit 1
fi

"${ROOT}/bin/client" "${USER}" 127.0.0.1 "${PORT}" "${PASS}" >"${CLIENT_REG_LOG}" 2>&1 || true
"${ROOT}/bin/client" "${USER}" 127.0.0.1 "${PORT}" "wrong_pass" >"${CLIENT_WRONG_LOG}" 2>&1 || true
"${ROOT}/bin/client" "${USER}" 127.0.0.1 "${PORT}" "${PASS}" >"${CLIENT_OK_LOG}" 2>&1 || true

if [ ! -d "${USER_DIR}" ]; then
  echo "User directory missing: ${USER_DIR}"
  exit 1
fi
if [ ! -f "${USER_DIR}/.PASSWORD" ]; then
  echo ".PASSWORD missing: ${USER_DIR}/.PASSWORD"
  exit 1
fi
if [ ! -f "${USER_DIR}/.DATA" ]; then
  echo ".DATA missing: ${USER_DIR}/.DATA"
  exit 1
fi

PASS_LINE="$(head -n 1 "${USER_DIR}/.PASSWORD")"
if [ "${PASS_LINE}" != "${PASS}" ]; then
  echo "Unexpected password line: ${PASS_LINE}"
  exit 1
fi

DATA_LINE="$(head -n 1 "${USER_DIR}/.DATA")"
if [ "${DATA_LINE}" != "0" ]; then
  echo "Unexpected .DATA first line: ${DATA_LINE}"
  exit 1
fi

echo "Integration test completed."
echo "User directory: ${USER_DIR}"
echo "Server log: ${SERVER_LOG}"
echo "Client logs: ${CLIENT_REG_LOG} ${CLIENT_WRONG_LOG} ${CLIENT_OK_LOG}"
