# CubeIDE에서 빌드하고 코드를 넣을 위치

저장소는 실제 ARM GCC로 빌드한 HAL C 소스와 Makefile을 제공합니다. `.project`/`.cproject`를 CubeIDE GUI에서 생성·시험한 프로젝트는 아닙니다. 아래 A는 제공 소스를 그대로 빌드하는 방법이고 B는 CubeMX가 생성한 managed project에 사용자 모듈을 넣는 방법입니다.

## A. 제공된 소스·Makefile 사용

1. 저장소를 clone하거나 ZIP으로 내려받습니다.
2. Python 3로 `python3 scripts/fetch_vendor.py`를 실행합니다. 공식 HAL·CMSIS와 라이선스를 고정 revision에서 내려받습니다.
3. Arm GNU Toolchain의 bin 디렉터리를 PATH에 넣고 저장소 루트에서 `make`를 실행합니다. CubeIDE에 포함된 GCC도 사용할 수 있습니다.

```sh
make CROSS_COMPILE=/path/to/toolchain/bin/arm-none-eabi-
```

결과는 `build/hbridge_f303ze.elf`, `.hex`, `.bin`입니다. GNU make 명령과 경로 표기법은 OS에 맞춰 사용합니다. 공백 있는 toolchain 경로는 셸에서 적절히 인용하세요.

CubeIDE에서는 `File → New → Makefile Project with Existing Code`로 저장소 루트를 선택하는 방법을 사용할 수 있습니다. 메뉴가 `C/C++` 아래에 있을 수 있습니다. ARM GCC/cross toolchain을 선택하고 프로젝트 build command가 위 Makefile을 실행하도록 설정합니다. ELF는 STM32CubeProgrammer/ST-LINK로 다운로드할 수 있으며 IDE debug launch를 만들 때 MCU는 STM32F303ZETx, 실행 파일은 해당 ELF, 연결은 ST-LINK/SWD를 선택합니다. 이 GUI import/debug launch 자체는 이 환경에서 시험하지 않았습니다.

## B. CubeMX 생성 프로젝트에 넣기

[CubeMX 설정](CUBEMX_KO.md)을 적용하고 Toolchain을 STM32CubeIDE로 지정해 코드를 생성합니다. 생성 결과의 초기화 설정은 제공된 `Core/Src/main.c`와 비교할 수 있습니다.

사용자 모듈은 다음 **5개 파일**입니다.

```text
Core/Inc/hb_config.h
Core/Inc/hb_control.h
Core/Inc/hb_app.h
Core/Src/hb_control.c
Core/Src/hb_app.c
```

이 파일들을 생성 프로젝트의 같은 위치로 복사합니다. CubeIDE가 `.c` 파일을 build 대상에 포함하는지 확인합니다. CubeIDE의 일반 managed project에서는 제공된 `hb_runtime.c`, 자체 linker script, vendor startup/system 파일을 추가 복사하지 않습니다. 해당 프로젝트의 표준 runtime/startup을 사용합니다.

### main.h — USER CODE Private defines

데드타임 상수는 `hb_config.h` 대신 **`main.h` 한 곳**에서 정의합니다. CubeMX가 새로 생성한 main.h의 해당 USER CODE 영역에 추가합니다.

```c
/* USER CODE BEGIN Private defines */
#define HB_DEADTIME_TICKS 72U
/* USER CODE END Private defines */
```

`main.c`의 TIM1 초기화와 `hb_app.c`의 설정 검사가 이 값을 함께 사용합니다. CubeMX의 TIM1 Dead Time 설정도 72로 맞춥니다. 이 상수를 추가하지 않고 사용자 모듈만 복사하면 `hb_app.c`가 컴파일되지 않습니다.

### main.c — USER CODE Includes

```c
/* USER CODE BEGIN Includes */
#include "hb_app.h"
/* USER CODE END Includes */
```

### main.c — USER CODE 2

`MX_GPIO_Init()`, `MX_ADC1_Init()`, `MX_TIM1_Init()`이 끝난 후 넣습니다.

```c
/* USER CODE BEGIN 2 */
HB_App_Init();
/* USER CODE END 2 */
```

`hadc1`과 `htim1`은 생성된 전역 handle이어야 합니다. App은 이를 `extern`으로 참조합니다. 다른 ADC/타이머 이름으로 바꾸었다면 App도 함께 수정해야 합니다.

### main.c — while(1)의 USER CODE 3

```c
while (1)
{
    /* USER CODE BEGIN 3 */
    HB_App_Poll();
}
/* USER CODE END 3 */
```

전류 PI를 while(1)에 추가하지 않습니다. `HB_App_Poll()`은 interrupt를 기다리며, 제어는 ADC 변환 완료마다 수행합니다.

### stm32f3xx_it.c — include와 SysTick

include USER CODE 영역에 `#include "hb_app.h"`를 추가합니다. 생성된 SysTick에서 `HAL_IncTick()` 뒤의 USER CODE 영역에 다음을 넣습니다.

```c
void SysTick_Handler(void)
{
    HAL_IncTick();
    /* 해당 handler의 USER CODE 영역 */
    HB_App_SafetyTick1ms();
}
```

기존 `HAL_SYSTICK_IRQHandler()` 호출이 있는 생성 프로젝트에서는 그 호출을 유지합니다. `HAL_IncTick()`을 중복 호출하지 않습니다.

ADC IRQ에는 다음 호출이 있어야 합니다.

```c
void ADC1_2_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&hadc1);
}
```

`HAL_ADC_ConvCpltCallback()`과 `HAL_ADC_ErrorCallback()`은 이미 `hb_app.c`가 정의합니다. main.c 등에 동일한 함수를 추가하지 않습니다. 다른 ADC도 사용하는 경우 하나의 콜백에서 Instance별로 분기해야 합니다.

### Error_Handler와 예외 처리

`Error_Handler()`, HardFault·MemManage·BusFault·UsageFault 등의 handler가 무한 루프로 들어가기 **전에** 아래 호출을 둡니다. handler에서 hb_app.h를 include합니다.

```c
HB_App_EmergencyStop();
```

이 함수는 EN GPIO를 제어하지 않습니다. TIM1 MOE를 직접 해제해 네 게이트 입력을 LOW로 합니다. 이미 켜진 모터를 단순 무한 루프로 방치하지 않도록 제공 reference의 handler를 비교하세요.

## 파일별 수정 목적

| 파일 | 수정할 때 |
|---|---|
| `hb_config.h` | 센서 감도, 시험 전류 상한, PI gain, 출력 허용 guard |
| `main.h` | 데드타임 상수 `HB_DEADTIME_TICKS`의 유일한 정의 |
| `hb_control.c` | 하드웨어 독립 상태·PI 알고리즘 |
| `hb_app.c` | ADC 콜백, MOE/CCR 적용, 샘플 중단 감시 |
| `main.c` | 클럭과 ADC/TIM 초기화, App 진입 |
| `stm32f3xx_hal_msp.c` | 실제 GPIO alternate function과 NVIC |
| `stm32f3xx_it.c` | ADC IRQ·SysTick·예외 연결 |

## Generate Code를 다시 실행한 후

제공 reference의 모든 수작업 초기화·handler 코드가 CubeMX 재생성으로 유지되는 것은 아닙니다. 사용자 모듈 다섯 파일은 별도로 유지하고 위 include·App 호출·SysTick 감독·예외 정지 연결을 다시 확인합니다. GPIO PE8/9/10/11과 PWM1/idle/dead time을 표와 비교합니다. 무심코 추가한 `HAL_TIM_PWM_Start()`가 없는지도 확인합니다.

제공 Makefile은 freestanding 최소 runtime을 사용하지만 CubeIDE 생성 프로젝트는 표준 runtime을 사용합니다. 두 runtime을 중복 링크하지 않습니다.
