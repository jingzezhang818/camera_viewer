#!/usr/bin/env bash
set -euo pipefail

# Default to MSI mode (equivalent to: ./load_driver.sh 1)
mode="${1:-1}"

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tests_dir="${script_dir}/../dma_ip_drivers/XDMA/linux-kernel/tests"
load_script="${tests_dir}/load_driver.sh"

if [[ ! -f "${load_script}" ]]; then
  echo "Error: cannot find ${load_script}" >&2
  exit 1
fi

cd "${tests_dir}"

if [[ "${EUID}" -ne 0 ]]; then
  exec sudo "${load_script}" "${mode}"
else
  exec "${load_script}" "${mode}"
fi
