#!/usr/bin/env bash
set -euo pipefail

ADDR="${1:?usage: $0 <BT_ADDR>}"
OUT_DIR="docs/test-results"
OUT_FILE="${OUT_DIR}/sdp-${ADDR//:/-}.txt"

mkdir -p "${OUT_DIR}"

run_sdptool() {
  local title="$1"
  shift

  echo
  echo "## ${title}"
  echo "+ $*"
  if ! "$@" 2>&1; then
    echo "Command failed; continuing so other SDP formats can still be captured."
  fi
}

{
  echo "Browsing SDP for ${ADDR}"
  run_sdptool "sdptool browse" sdptool browse "${ADDR}"
  run_sdptool "sdptool browse --tree" sdptool browse --tree "${ADDR}"
  run_sdptool "sdptool browse --raw" sdptool browse --raw "${ADDR}"
  run_sdptool "sdptool records" sdptool records "${ADDR}"
  run_sdptool "sdptool records --xml" sdptool records --xml "${ADDR}"
} | tee "${OUT_FILE}"

if grep -qiE '0x0206|0x206|DescriptorList' "${OUT_FILE}"; then
  echo "DescriptorList found in ${OUT_FILE}"
else
  echo "WARNING: DescriptorList / attribute 0x0206 was not found in ${OUT_FILE}" >&2
  echo "The SDP dump is incomplete for T07; retry while the device is still discoverable." >&2
  echo "If this is the 203-byte Joy-Con descriptor build, patch ESP-IDF SDP_MAX_PAD_LEN and rebuild." >&2
fi

echo "Saved SDP dump to ${OUT_FILE}"
