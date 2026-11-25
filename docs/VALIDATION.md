# 검증 기록과 범위

실제 NUCLEO·전력단·모터가 연결되지 않은 Cloud 환경에서 검증했습니다. 아래는 코드·빌드·설계 연결 검증이며 실물 전류 추종이나 안전 정격의 검증이 아닙니다.

## 통과한 항목

### 1. 하드웨어 독립 제어 로직

```sh
python3 tests/run_tests.py
```

Host GCC에서 `-std=c11 -O2 -Wall -Wextra -Werror -Wpedantic`으로 빌드했습니다.

- 무전류 대기·1024표본 offset 보정과 보정 중 출력 OFF
- 부팅 중 유지된 run=1의 자동 시작 차단, IDLE run-low 후 시작
- 100표본 bootstrap, 운전·정지·재기동 시 재충전
- 기본 출력 허용 guard, latch fault, 필터 이전 과전류 검사
- 전류 반대 부호, 음수/상한 초과/NaN/무한대 지령 거부
- ADC rail, 잘못된 영점과 불안정 보정 거부
- 도달할 수 없는 지령에서 duty 포화와 적분항 제한
- 지령 0에서 적분·reference 초기화와 출력 OFF

임시 RL 모델(R=6 Ω, L=1 mH, 12 V, 1 tick 지연, 1 µs 손실 근사)에서는 0.200 A 지령에 약 **0.1995 A**로 수렴했습니다. 역기전력을 0 V에서 2.4 V로 바꾼 뒤에도 약 0.1995 A로 회복했습니다. ADC 양자화를 포함하지만 스위칭 회로·센서 실제 RC·기계적 회전·발열을 모델링한 결과가 아닙니다.

### 2. App 모의 HAL 29개 시나리오

동일 test runner가 `HB_HARDWARE_VERIFIED=0/1` 두 빌드를 실행했습니다.

- ADC 1회 arm, timer base 1회 시작, 초기 MOE OFF
- 교정 중 MOE OFF, bootstrap에서 CCR1/CCR2=0
- 운전에서 CCR1 PWM·CCR2=0, 정지와 오류에서 MOE OFF
- ADC driver 오류·샘플 중단·비상 정지, PRIMASK 저장/복원
- CH2 PWM2 오설정, ADC rank/differential 오설정 거부
- TIM1 PLL ×2 경로·잘못된 주파수 거부, APB prescaler 계산
- ADC/timer 시작 API 실패 시 출력 OFF

`tests/fakes/main.h`는 레지스터·HAL을 모의합니다. MCU의 실제 파형·인터럽트 지연을 재현한 시험은 아닙니다.

### 3. 실제 ARM 컴파일·링크

공식 ST HAL·CMSIS를 고정 revision에서 다운로드하고 **Arm GNU GCC 12.2.1**로 Cortex-M4 hard-float 빌드를 수행했습니다.

```sh
python3 scripts/fetch_vendor.py
make CROSS_COMPILE=/path/to/arm-none-eabi-
```

기본 출력 차단 빌드와 `HB_HARDWARE_VERIFIED=1` 검증용 빌드 모두 ELF·HEX·BIN 생성에 성공했습니다. ELF는 ARM EABI5 hard-float이며 `arm-none-eabi-nm -u` 결과 unresolved symbol이 없습니다.

| 항목 | 결과 |
|---|---|
| text | 11,952 B |
| data | 12 B |
| bss | 4,404 B, 4 KiB stack 예약 포함 |
| FLASH 사용량 | 11,964 B / 512 KiB |
| RAM 사용량 | 4,416 B / 64 KiB |

CCM 16 KiB는 이 linker script에서 사용하지 않습니다. 다운로드 도구가 없는 깨끗한 소스 복사본에서도 vendor fetch→빌드 경로를 확인했습니다. 다른 GCC 버전·옵션에서는 크기와 파일 hash가 달라질 수 있습니다. 생성 binary는 Git에 커밋하지 않으며 workflow에서 기본 출력 차단 빌드를 생성할 수 있습니다. 원격 workflow 실행 성공은 별도의 상태입니다.

### 4. 회로도·PCB 연결

사용자 업로드 원본을 로컬 `hardware/`에 넣고 실행했습니다.

```sh
python3 scripts/check_hardware_nets.py
```

공유하는 **178개 pad 연결 그룹 일치**와 J1/전원/센서/EN/DT 계약 확인에 통과했습니다. 원본은 공개 Git 업로드 대상에서 제외했습니다. 스크립트·원본 SHA-256·검토 방법은 [하드웨어 문서](HARDWARE_REVIEW_KO.md)에 있습니다. 원본이 없는 clone에서는 이 항목을 실행하려면 파일을 별도로 넣어야 합니다.

## 아직 확인하지 않은 항목

- CubeMX GUI에서 `.ioc` 열기·마이그레이션·Generate Code
- CubeIDE GUI project import·ST-LINK 다운로드·debug launch
- 실장품 A3B 감도와 R13 값, 실제 전원·부품·배선 상태
- PCB routing DRC/ERC, 절연·열·허용 전류
- 실제 4개 gate–source 파형, dead time, bootstrap 전압
- ADC sample 위상과 ISR이 12.5 µs 안에 끝나는지
- 실제 모터 정격/스톨/온도/속도 조건과 PI 안정성·추종 오차
- 단락 차단 성능, 회생 시 버스 과전압, 독립 하드웨어 OCP

`>3 ms` ADC timeout은 매 PWM 주기의 deadline 감시가 아닙니다. 출력 정지는 정상 callback에서 실행되며 ADC가 멈추면 약 4–5 ms 수준의 SysTick 감시를 거칩니다. CPU가 halt/lockup한 모든 경우를 이 코드가 보장하지는 않습니다.
