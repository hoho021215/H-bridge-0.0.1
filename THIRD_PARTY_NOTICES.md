# Upstream build inputs

`scripts/fetch_vendor.py` fetches the minimal official files needed for this build.
Their revision IDs and upstream license identifiers are recorded in
`scripts/vendor_versions.json`. Upstream copyright and license headers are kept.

| Upstream | License | Downloaded license location |
|---|---|---|
| STMicroelectronics/stm32f3xx-hal-driver | BSD-3-Clause | `Drivers/STM32F3xx_HAL_Driver/LICENSE.md` |
| STMicroelectronics/cmsis-device-f3 | Apache-2.0 | `Drivers/CMSIS/Device/ST/STM32F3xx/LICENSE.md` |
| ARM-software/CMSIS_5 | Apache-2.0 | `Drivers/CMSIS/LICENSE.txt` |

The supplied KiCad files are the user's original uploads, kept as local analysis
inputs and excluded from Git publication. Their logical connectivity was
inspected without modifying the files. User-owned STM32/buck
repositories were consulted at the user's request for structure and integration
conventions; they are identified in README.md. This notice does not assign a new
license to those hardware files or to the user's repository.
