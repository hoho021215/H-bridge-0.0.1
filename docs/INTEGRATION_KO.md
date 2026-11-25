# CubeMX 코드 생성 후 CubeIDE에 추가할 파일

작업 순서는 **CubeMX Generate Code → 사용자 파일 추가/연결 → CubeIDE Build**입니다. CubeMX가 만든 STM32CubeIDE 프로젝트와 표준 C 실행환경을 사용합니다. `hb_runtime.c`는 저장소에서 삭제했으며 추가할 필요가 없습니다.

## 1. CubeMX에서 프로젝트 생성

1. [CubeMX 설정표](CUBEMX_KO.md)에 따라 NUCLEO-F303ZE, TIM1 상보 PWM/데드타임, ADC1과 NVIC를 설정합니다.
2. Project Manager → Toolchain/IDE를 `STM32CubeIDE`로 지정합니다.
3. Code Generator → `Keep User Code when re-generating`을 켭니다.
4. Generate Code를 실행하고 생성한 프로젝트를 CubeIDE에서 엽니다.

제공 `.ioc`는 수작성 설정 참고본이며 CubeMX GUI Generate Code 자체는 아직 검증하지 않았습니다. 생성된 TIM1/ADC/클럭 설정을 설정표와 비교합니다.

## 2. 직접 추가할 파일은 5개

저장소에서 아래 파일을 생성 프로젝트의 같은 위치에 복사합니다.

| 복사할 파일 | 복사 위치 | 역할 |
|---|---|---|
| `Core/Inc/hb_config.h` | 프로젝트 `Core/Inc/` | 센서 감도, 시험 전류 상한, PI 이득 등 설정 |
| `Core/Inc/hb_control.h` | 프로젝트 `Core/Inc/` | 제어 상태/오류/자료형/함수 선언 |
| `Core/Inc/hb_app.h` | 프로젝트 `Core/Inc/` | 사용자 지령/상태 변수와 App 함수 선언 |
| `Core/Src/hb_control.c` | 프로젝트 `Core/Src/` | 영점 보정, 보호 검사, 상태 처리와 PI 계산 |
| `Core/Src/hb_app.c` | 프로젝트 `Core/Src/` | ADC 콜백, PWM 적용, 출력 차단, ADC 중단 감시 |

CubeIDE에서 Refresh를 하고 두 `.c`가 build에서 제외되지 않았는지 확인합니다. `Core/Inc`는 보통 생성 프로젝트의 include 경로에 이미 들어 있습니다.

### CubeMX가 생성한 파일은 유지

`main.c`, `main.h`, `stm32f3xx_it.c`, `stm32f3xx_hal_msp.c`, `stm32f3xx_hal_conf.h`, startup/system 파일, linker script, Drivers는 생성 프로젝트의 것을 사용합니다. 저장소의 같은 이름 파일로 통째로 덮어쓰지 않습니다. 저장소의 파일은 초기화/인터럽트 연결을 비교하는 참고본입니다.

`Makefile`, `scripts/`, `tests/`, `hardware/`는 PC용 빌드 보조/검증 자료이므로 CubeIDE 프로젝트에 복사할 필요가 없습니다. 일반 STM32CubeIDE 프로젝트는 툴체인의 표준 C 라이브러리와 생성된 startup/system/syscalls/sysmem 구성을 사용합니다. 별도 사용자 runtime 파일을 만들지 않습니다.

## 3. 생성된 main.h 수정

`main.h`의 `USER CODE BEGIN Private defines` 안에 추가합니다.

```c
/* USER CODE BEGIN Private defines */
#define HB_DEADTIME_TICKS 72U  /* TIM1 72 MHz, CKD=DIV1에서 데드타임 1 us */
/* USER CODE END Private defines */
```

`hb_app.c`가 이 값을 실제 TIM1 설정과 비교합니다. CubeMX의 TIM1 Dead Time 설정도 72여야 합니다. 데드타임 정의를 `hb_config.h`에 중복 추가하지 않습니다.

## 4. 생성된 main.c 수정

### USER CODE Includes

```c
/* USER CODE BEGIN Includes */
#include "hb_app.h"
/* USER CODE END Includes */
```

### USER CODE 2 — 주변장치 초기화 이후

`MX_GPIO_Init()`, `MX_ADC1_Init()`, `MX_TIM1_Init()` 호출이 끝난 뒤에 추가합니다.

```c
/* USER CODE BEGIN 2 */
HB_App_Init();
/* USER CODE END 2 */
```

`hadc1`, `htim1`은 생성된 전역 handle이어야 합니다. App이 이 이름을 `extern`으로 참조합니다. 다른 이름의 ADC/타이머를 사용했다면 App도 맞춰야 합니다.

`HAL_TIM_PWM_Start()`/`HAL_TIMEx_PWMN_Start()`를 별도로 추가하지 않습니다. TIM1에서는 MOE를 켜서 초기화 직후 출력이 활성화될 수 있습니다. App이 ADC 시작, counter 시작, 출력 허용과 정지를 소유합니다.

### while(1)의 USER CODE 3

```c
while (1)
{
    /* USER CODE BEGIN 3 */
    HB_App_Poll();
}
/* USER CODE END 3 */
```

PI를 while(1)에서 다시 계산하지 않습니다. 실제 제어는 ADC 변환 완료마다 수행합니다.

### Error_Handler

생성된 `Error_Handler()`의 USER CODE 영역에서 인터럽트를 끄고 무한 루프로 들어가기 전에 추가합니다.

```c
HB_App_EmergencyStop();
```

## 5. 생성된 stm32f3xx_it.c 수정

### include USER CODE 영역

```c
#include "hb_app.h"
```

### SysTick_Handler

`HAL_IncTick()` 뒤의 handler USER CODE 영역에 추가합니다.

```c
void SysTick_Handler(void)
{
    HAL_IncTick();
    /* 해당 handler의 USER CODE 영역 */
    HB_App_SafetyTick1ms();
}
```

생성 코드에 `HAL_SYSTICK_IRQHandler()`가 있다면 유지하고 `HAL_IncTick()`을 중복 호출하지 않습니다.

### ADC 인터럽트

CubeMX에서 ADC1/ADC2 global interrupt를 켰다면 다음 호출이 생성되어 있어야 합니다. 없다면 NVIC 설정을 확인하고 재생성합니다.

```c
void ADC1_2_IRQHandler(void)
{
    HAL_ADC_IRQHandler(&hadc1);
}
```

`HAL_ADC_ConvCpltCallback()`과 `HAL_ADC_ErrorCallback()`은 `hb_app.c`가 이미 정의합니다. main.c 등에 같은 함수를 다시 만들지 않습니다. 다른 ADC를 추가하면 하나의 콜백 안에서 Instance별로 분기합니다.

### CPU 예외 handler

HardFault/MemManage/BusFault/UsageFault/NMI handler가 무한 루프에 들어가기 전에 USER CODE 영역에서 다음을 호출합니다.

```c
HB_App_EmergencyStop();
```

제공 `stm32f3xx_it.c`의 연결을 참고합니다. MOE를 해제해 네 게이트 입력을 LOW로 처리하며 EN GPIO는 제어하지 않습니다.

## 6. CubeIDE Build와 확인

1. Project → Build Project를 실행합니다. 일반적인 Debug 설정의 ELF는 `Debug/<프로젝트명>.elf`에 생성됩니다.
2. `HB_DEADTIME_TICKS undeclared`라면 3단계 main.h 정의를 확인합니다.
3. `multiple definition of HAL_ADC_...Callback`이면 중복 콜백을 제거하고 App의 함수 한 개만 유지합니다.
4. `undefined reference to HB_App_...`/`HB_Control_...`이면 두 사용자 `.c`의 추가 위치와 build 제외 여부를 확인합니다.
5. 최초 다운로드/구동은 [첫 시험 절차](BRINGUP_KO.md)를 따릅니다. 기본 `HB_HARDWARE_VERIFIED=0`에서는 측정·영점 보정만 허용합니다.

Generate Code를 다시 실행한 뒤에는 위 USER CODE 연결과 main.h 데드타임 정의가 유지되는지 확인합니다. 실제 GPIO/클럭/PWM1/idle/dead time 설정도 설정표와 다시 비교합니다.

## 선택 사항: PC 명령행 검증

저장소 Makefile은 GitHub CI/명령행 검증용 보조 경로입니다. CubeIDE 사용자 프로젝트에 복사하지 않아도 됩니다. 사용자 runtime 대신 Arm GNU Toolchain의 **표준 newlib-nano/nosys와 CRT**로 링크합니다.

```sh
python3 scripts/fetch_vendor.py
make CROSS_COMPILE=/path/to/toolchain/bin/arm-none-eabi-
```

컴파일러뿐 아니라 해당 toolchain의 newlib-nano/nosys 라이브러리도 설치되어 있어야 합니다. CubeIDE GUI 빌드는 생성 프로젝트의 build 설정과 linker script를 사용합니다.
