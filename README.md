# H-bridge 0.0.1 — STM32 전류제어

첨부 Hbridge PCB와 **NUCLEO-F303ZE**로 **12 V 브러시 DC 모터의 양(+) 전류 지령**을 제어하는 초기 펌웨어입니다. 3상 BLDC용 정류 코드는 아닙니다.

회로의 UCC21331CDR 두 개에 **4개 독립 게이트 입력**을 제공합니다. TIM1 상보 PWM·1 µs dead time, 20 kHz ADC 동기 측정, TMCS1108A3B 영점 보정, 지령 ramp, PI anti-windup, 부트스트랩 충전, 소프트웨어 과전류·ADC 중단 감시를 포함합니다. 정지 시 MOE를 해제해 네 입력을 LOW로 합니다.

**기본값은 `HB_HARDWARE_VERIFIED=0`: 측정·보정은 실행하지만 모터 구동은 거부합니다.** [첫 시험 절차](docs/BRINGUP_KO.md)를 통해 배선·실장·파형을 확인하는 작업자를 위한 설정입니다. 실제 보드 또는 모터에서 시험한 결과는 없습니다. 모터 정격 0.5 A·스톨 2 A·R=6 Ω·L=1 mH는 사용자의 요청에 따라 둔 **임시 가정**이며, 시험 지령 상한 0.5 A·소프트웨어 차단 1 A도 실제 정격에 맞춰 다시 정해야 합니다.

## 먼저 읽을 문서

1. [회로·PCB 분석과 실제 커넥터](docs/HARDWARE_REVIEW_KO.md)
2. [STM32CubeMX 설정 순서](docs/CUBEMX_KO.md)
3. [CubeIDE 가져오기와 코드를 넣을 위치](docs/INTEGRATION_KO.md)
4. [모터 추정값·제어 파라미터](docs/MOTOR_ASSUMPTIONS_KO.md)
5. [처음 켜는 순서와 지령 입력](docs/BRINGUP_KO.md)
6. [검증 결과와 확인하지 못한 항목](docs/VALIDATION.md)

## 배선 요약

| Hbridge 커넥터 | NUCLEO 신호 | 역할 |
|---|---|---|
| J1.1 `PWMA` | PE9 / TIM1_CH1 AF2 | 왼쪽 상측 Q1 |
| J1.3 `PWMA~` | PE8 / TIM1_CH1N AF2 | 왼쪽 하측 Q2 |
| J1.4 `PWMB` | PE11 / TIM1_CH2 AF2 | 오른쪽 상측 Q3 |
| J1.6 `PWMB~` | PE10 / TIM1_CH2N AF2 | 오른쪽 하측 Q4 |
| J1.2, J1.5 | NUCLEO GND | `3.3GND` |
| Currentsensor1.1 | PA0 / ADC1_IN1 | 전류 센서 출력 |
| Currentsensor1.2 | NUCLEO GND | 센서 출력 기준 |

이 PE 핀 배치는 펌웨어에 맞춰 정한 **외부 배선 제안**입니다. 원본 Hbridge 보드에는 STM32가 없습니다. 모터 전원은 `DCV1`에 12 V, 별도 게이트 전원은 `15V1`에 15 V, 로직·센서 전원은 `3.3V1`에 3.3 V입니다. `3.3GND`와 `15GND`는 설계에서 분리되어 있습니다. IC3의 library ID에는 A2, Value에는 A3가 적혀 있으므로 실제 실장품의 감도를 확인하세요.

## 빌드

Python 3, GNU Make, Arm GNU Toolchain의 `arm-none-eabi-gcc`를 사용합니다.

```sh
python3 scripts/fetch_vendor.py
make
python3 tests/run_tests.py
```

공식 ST HAL·CMSIS는 고정된 Git revision에서 가져오며 해당 라이선스도 함께 다운로드합니다. `Drivers/`와 vendor startup/system 파일은 Git에 중복 보관하지 않습니다. 생성 결과는 `build/hbridge_f303ze.elf`, `.hex`, `.bin`입니다.

`hbridge_f303ze.ioc`는 설정에 맞춰 **수작업으로 작성한 CubeMX 프로필**입니다. CubeMX GUI에서 열기·Generate Code는 이 환경에서 검증하지 않았습니다. 제공된 C 소스의 ARM 컴파일·링크와 GUI 코드 생성을 구분합니다.

## 지령 입력

CubeIDE Live Expressions에 `hb_request`와 `hb_status`를 추가합니다. 보정 완료, 정상 IDLE, 배선 검증용 설정을 확인한 뒤 다음 순서로 입력합니다.

```c
hb_request.run = 0;          // 보정 후 run=0을 한 번 확인해야 시작 가능
hb_request.current_A = 0.1f;
hb_request.run = 1;          // 5 ms bootstrap 후 전류 지령을 서서히 적용

hb_request.run = 0;          // 정지: 4개 게이트 입력 LOW
```

부팅·보정 중 미리 넣은 run=1로 자동 시작하지 않습니다. 오류는 MCU reset까지 유지됩니다. 기본 검증 guard=0에서 시작을 요청하면 `HB_FAULT_NOT_VERIFIED`로 정지합니다.

## 참고한 기존 예제

- 사용자 [`stm-buck-0.1.1`](https://github.com/hoho021215/stm-buck-0.1.1/tree/6722cf2ba08b38e582be48d7e6fc6c0efe49e5c5): 사용자 모듈·요청 변수·문서 구성
- 사용자 [`stm-buck-0.0.2`](https://github.com/hoho021215/stm-buck-0.0.2/tree/c8b6320a3db2230ddb80b11abbcf6bd11fab6271): F303ZE CubeMX 설정 설명 방식

기존 벅 회로의 센서 감도, 전압 PI 수치, GPIO EN, 50 kHz 설정은 이 보드에 재사용하지 않았습니다. 업로드한 두 KiCad 원본은 로컬 분석에 사용했습니다. 저장소에는 펌웨어·설정 안내·연결 검증 스크립트를 올리며, 원본 회로/PCB를 공개하는 작업은 포함하지 않습니다. 원본을 로컬 `hardware/`에 넣으면 `python3 scripts/check_hardware_nets.py`로 연결 검사를 재현할 수 있습니다.
