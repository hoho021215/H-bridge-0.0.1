#!/usr/bin/env python3
"""Download the pinned upstream source files needed by the standalone build.

Uses normal HTTPS certificate verification and inherited proxy configuration.
Downloaded vendor files are build inputs; do not edit them.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse
import json
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
HAL_REV = '664a3bd898dc001d253a47c3e058d4f053cc4da5'
DEV_REV = '402c039a0a6d73c31303a05e6a1d6b8b59474fa1'
CMSIS_REV = '55b19837f5703e418ca37894d5745b1dc05e4c91'
HAL_HEADERS = ['hal', 'hal_def', 'hal_adc', 'hal_adc_ex', 'hal_cortex', 'hal_dma',
               'hal_dma_ex', 'hal_gpio', 'hal_gpio_ex', 'hal_flash', 'hal_flash_ex',
               'hal_pwr', 'hal_pwr_ex', 'hal_rcc', 'hal_rcc_ex', 'hal_tim', 'hal_tim_ex']
HAL_SOURCES = ['hal', 'hal_adc', 'hal_adc_ex', 'hal_cortex', 'hal_dma', 'hal_gpio',
               'hal_flash', 'hal_flash_ex', 'hal_pwr', 'hal_pwr_ex', 'hal_rcc',
               'hal_rcc_ex', 'hal_tim', 'hal_tim_ex']


def sources():
    out = []
    def add(repo, rev, src, dst):
        out.append((f'https://raw.githubusercontent.com/{repo}/{rev}/{src}', ROOT / dst))
    for name in HAL_HEADERS:
        add('STMicroelectronics/stm32f3xx-hal-driver', HAL_REV,
            f'Inc/stm32f3xx_{name}.h', f'Drivers/STM32F3xx_HAL_Driver/Inc/stm32f3xx_{name}.h')
    for name in HAL_SOURCES:
        add('STMicroelectronics/stm32f3xx-hal-driver', HAL_REV,
            f'Src/stm32f3xx_{name}.c', f'Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_{name}.c')
    add('STMicroelectronics/stm32f3xx-hal-driver', HAL_REV,
        'Inc/Legacy/stm32_hal_legacy.h', 'Drivers/STM32F3xx_HAL_Driver/Inc/Legacy/stm32_hal_legacy.h')
    add('STMicroelectronics/stm32f3xx-hal-driver', HAL_REV,
        'LICENSE.md', 'Drivers/STM32F3xx_HAL_Driver/LICENSE.md')
    for name in ['stm32f3xx.h', 'stm32f303xe.h', 'system_stm32f3xx.h']:
        add('STMicroelectronics/cmsis-device-f3', DEV_REV,
            f'Include/{name}', f'Drivers/CMSIS/Device/ST/STM32F3xx/Include/{name}')
    add('STMicroelectronics/cmsis-device-f3', DEV_REV,
        'Source/Templates/system_stm32f3xx.c', 'Core/Src/system_stm32f3xx.c')
    add('STMicroelectronics/cmsis-device-f3', DEV_REV,
        'Source/Templates/gcc/startup_stm32f303xe.s', 'Core/Startup/startup_stm32f303xe.s')
    add('STMicroelectronics/cmsis-device-f3', DEV_REV,
        'LICENSE.md', 'Drivers/CMSIS/Device/ST/STM32F3xx/LICENSE.md')
    for name in ['core_cm4.h', 'cmsis_version.h', 'cmsis_compiler.h', 'cmsis_gcc.h', 'mpu_armv7.h']:
        add('ARM-software/CMSIS_5', CMSIS_REV,
            f'CMSIS/Core/Include/{name}', f'Drivers/CMSIS/Include/{name}')
    add('ARM-software/CMSIS_5', CMSIS_REV, 'LICENSE.txt', 'Drivers/CMSIS/LICENSE.txt')
    return out


def fetch(item):
    url, target = item
    if target.exists() and target.stat().st_size > 0:
        return
    with urllib.request.urlopen(url, timeout=45) as response:
        data = response.read()
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--list', action='store_true', help='List pinned inputs without downloading')
    args = parser.parse_args()
    items = sources()
    if args.list:
        for url, path in items:
            print(path.relative_to(ROOT), url)
        return
    with ThreadPoolExecutor(max_workers=6) as pool:
        list(pool.map(fetch, items))
    print(f'Pinned vendor inputs ready: {len(items)} files')


if __name__ == '__main__':
    main()
