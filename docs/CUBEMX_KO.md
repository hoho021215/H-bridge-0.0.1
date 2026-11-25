# STM32CubeMX 설정 방법

대상은 NUCLEO-F303ZE / STM32F303ZET6입니다. 제공한 `hbridge_f303ze.ioc`는 아래 설정을 기록한 수작성 프로필입니다. CubeMX GUI 열기·마이그레이션·Generate Code는 검증하지 않았으므로, 버전 경고나 누락이 있으면 아래 표로 설정을 다시 확인하세요. 실제 ARM 빌드에 사용한 초기화 코드는 `Core/Src/main.c`입니다.

## 1. 프로젝트 만들기

1. STM32CubeMX 또는 CubeIDE의 STM32 Project에서 Board Selector → `NUCLEO-F303ZE`를 선택합니다.
2. 보드 기본 주변장치 초기화를 모두 사용하지 않아도 됩니다. 이번 프로젝트에는 ADC1, TIM1, RCC, SYS만 필요합니다. USART·USB·RTOS·DMA는 사용하지 않습니다.
3. SYS → Debug를 `Serial Wire`로 설정해 PA13/PA14를 SWD용으로 둡니다.
4. Project Manager → Project Name `hbridge_f303ze`, Toolchain/IDE `STM32CubeIDE`, Keep User Code 활성화를 선택합니다.
5. 아래 GPIO·클럭·타이머·ADC 설정을 적용한 뒤 Generate Code를 수행하고 [코드 통합 방법](INTEGRATION_KO.md)을 따릅니다.

## 2. 핀 배치

핀 화면에서 같은 TIM1 기능이 여러 핀에 배치될 수 있으므로 **이 핀으로 직접 고정**합니다.

| MCU 핀 | 기능 | GPIO 설정 | 연결 |
|---|---|---|---|
| PE9 | TIM1_CH1, AF2 | Alternate push-pull / Pull-down / High speed | J1.1 |
| PE8 | TIM1_CH1N, AF2 | 동일 | J1.3 |
| PE11 | TIM1_CH2, AF2 | 동일 | J1.4 |
| PE10 | TIM1_CH2N, AF2 | 동일 | J1.6 |
| PA0 | ADC1_IN1 | Analog / No pull | Currentsensor1.1 |
| PA13, PA14 | SWD | Serial Wire | ST-LINK |
| PF0 OSC_IN | HSE 외부 클럭 입력 | Bypass | 보드 ST-LINK MCO 8 MHz |

PE 핀 번호는 MCU 신호명이며 커넥터의 물리적 핀 번호가 아닙니다. NUCLEO 확장 헤더의 해당 PE 신호를 찾아 배선하고 J1의 1번 표시를 확인합니다. `PWMA~` 이름만으로 반전이 생성되지 않으므로 CH1N·CH2N에도 실제 배선이 필요합니다.

## 3. RCC와 Clock Configuration

| 설정 | 값 |
|---|---|
| RCC High Speed Clock | Bypass Clock Source |
| HSE 입력 | 8 MHz, NUCLEO ST-LINK MCO |
| PLL source / predivider / multiplier | HSE / 1 / ×9 |
| System Clock source | PLLCLK |
| SYSCLK, HCLK | 72 MHz |
| APB1 divider / PCLK1 | /2 / 36 MHz |
| APB2 divider / PCLK2 | /1 / 72 MHz |
| TIM1 clock source | PCLK2/HCLK 경로, 72 MHz (`RCC_TIM1CLK_HCLK`) |
| TIM1 PLL ×2 선택 | 사용하지 않음 |
| ADC1/2 clock | Synchronous HCLK / 2 = 36 MHz |

현재 HSE 코드는 ST-LINK MCO 8 MHz가 실제로 공급되는 보드 상태를 전제로 합니다. ST-LINK를 분리하거나 해당 연결을 바꾸면 클럭 설정을 함께 수정해야 합니다. HSE 설정 실패 시 `Error_Handler()`에서 출력을 정지합니다.

## 4. TIM1

Mode 화면에서 `Internal Clock`, `PWM Generation CH1 CH1N`, `PWM Generation CH2 CH2N`을 선택합니다. Channel 4는 내부 ADC 트리거용 `PWM Generation No Output`으로 사용하며 외부 핀을 할당하지 않습니다. CubeMX 버전에 해당 메뉴가 없으면 CH4를 외부 핀에 억지로 연결하지 마세요. `HB_App_Init()`가 PWM2 CH4와 TRGO를 직접 구성합니다.

| TIM1 기본 설정 | 값 |
|---|---|
| Prescaler | 0 |
| Counter Mode | Up, edge aligned |
| Counter Period (ARR) | 3599 |
| Clock Division | DIV1 |
| Repetition Counter | 0 |
| Auto-reload preload | Enable |
| CH1, CH2 Mode | **PWM mode 1** |
| CH1, CH2 Pulse | 0 |
| CH1, CH2 Polarity | High |
| CH1N, CH2N Polarity | High |
| CH1/CH2 output idle state | Reset/LOW |
| CH1N/CH2N idle state | Reset/LOW |
| Fast Mode | Disable |
| CH4 Mode / Pulse | PWM mode 2 / 2700 |
| Master Output Trigger (TRGO) | OC4REF |
| TRGO2 / Master-Slave | Reset / Disable |

Break and Dead Time에서는 다음과 같이 설정합니다.

| 설정 | 값 |
|---|---|
| Dead Time | **72 ticks** |
| Off State Run Mode (OSSR) | Enable |
| Off State Idle Mode (OSSI) | Enable |
| Lock Level | Off |
| Automatic Output (AOE) | Disable |
| Break 1, Break 2 | Disable — 제공 PCB에 연결된 fault/break 선 없음 |

`72 MHz / 3600 = 20 kHz`, 주기는 50 µs입니다. CKD=DIV1, DTG=72는 직접 선형 구간이므로 dead time은 `72/72 MHz = 1 µs`입니다. CH1/CH2의 compare preload도 활성화하여 계산된 듀티를 다음 update부터 적용합니다. App에서 명시적으로 설정합니다.

CH1/CH1N은 Q1/Q2를 상보 구동합니다. CH2의 CCR=0은 Q3 OFF·Q4 ON 상태입니다. **CH2를 PWM2로 바꾸면 이 관계가 반대가 됩니다.** 펌웨어의 초기 설정 검사가 이 오류를 거부합니다.

Center-aligned로 변경하면 샘플·update 횟수와 제어 주기가 달라집니다. 현재의 `Ts=50 µs` 코드를 그대로 사용하지 않습니다.

## 5. ADC1

| 설정 | 값 |
|---|---|
| Channel / Rank | IN1 (PA0) / Regular Rank 1 |
| Input | Single ended |
| Resolution / Alignment | 12 bit / Right |
| Clock Prescaler | Synchronous / 2 |
| Sampling Time | 19.5 cycles |
| Scan / Continuous / Discontinuous | 모두 Disable |
| Number Of Conversion | 1 |
| External Trigger Source | TIM1 Trigger Out event, TRGO |
| External Trigger Edge | Rising |
| EOC Selection | Single Conversion |
| DMA Continuous Requests / Low Power Auto Wait | Disable |
| Overrun | Data preserved |
| Hardware channel offset | None |

NVIC → `ADC1 and ADC2 global interrupt`를 Enable, Preemption Priority 1, Subpriority 0으로 둡니다. SysTick Priority는 3입니다. TIM1 update/compare interrupt와 DMA interrupt는 이번 루프에 필요하지 않습니다.

CH4 PWM2의 OC4REF는 CNT=2700에서 상승하므로 ADC는 주기 시작 후 **37.5 µs**에 시작됩니다. 36 MHz에서 19.5+12.5 cycles 변환 시간은 약 0.89 µs입니다. 최대 듀티 50%라 왼쪽 상측 OFF 이후에 측정하며 센서가 권선에 직렬이라는 전제를 사용합니다.

이 샘플은 OFF 구간의 순간 전류입니다. PWM 평균 전류나 FOC의 Iq와 동일한 값으로 간주하지 않습니다. 센서 지연과 R13/C20 필터를 고려해 실측으로 시점을 확인해야 합니다. ADC ISR의 계산·HAL 처리까지 다음 update 전에 끝나야 하며 실측 실행 시간은 아직 확인하지 않았습니다.

## 6. 초기화·시작 코드 확인

`MX_*_Init()`은 설정만 해야 합니다. 별도로 아래를 추가하지 않습니다.

```c
/* 이 보드에서 초기화 직후 호출하면 MOE를 켜므로 사용하지 않습니다. */
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
```

출력 소유자는 `HB_App`입니다. ADC calibration → `HAL_ADC_Start_IT()` 1회 → `HAL_TIM_Base_Start()` 1회로 샘플만 시작합니다. Counter는 동작하지만 MOE는 OFF입니다. 보정·출력 허용 검사·기동 요청이 모두 충족된 뒤에만 MOE를 켜고 5 ms bootstrap 상태로 들어갑니다.
