#!/usr/bin/env bash
set -euo pipefail
# Source the intended ESP-IDF 6.1 environment before running. Build only.
root="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$root/build/gentle-games"
mkdir -p "$build_dir"
if [[ ! -f "$build_dir/sdkconfig" ]]; then
    cp "$root/docs/local-firmware/gentle-games-build.config" "$build_dir/sdkconfig"
fi
cd "$root"
python "$IDF_PATH/tools/idf.py" -B "$build_dir" -DIDF_TARGET=esp32s3 \
    -DBOARD_NAME=lichuang-dev -DSDKCONFIG="$build_dir/sdkconfig" build
python - "$build_dir/config/sdkconfig.json" <<'PY'
import json, sys
c = json.load(open(sys.argv[1]))
expected = {'CONTROLLED_TRIAL': False, 'CUSTOM_FIRMWARE_PROTECTION': True,
            'CUSTOM_PRESERVE_SHARED_ASSETS': True, 'FLASH_NONE_ASSETS': True,
            'BOARD_TYPE_LICHUANG_DEV_S3': True, 'LANGUAGE_ZH_CN': True,
            'USE_DEVICE_AEC': True, 'USE_EMOTE_MESSAGE_STYLE': False,
            'ESPTOOLPY_FLASHSIZE_16MB': True, 'SPIRAM_MODE_OCT': True}
for k, v in expected.items():
    assert c.get(k, False) == v, f'Unexpected build configuration: {k}'
print('Gentle-games configuration verified. Build only; no device writes.')
PY
