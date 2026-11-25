# 처음 시험하는 순서

현재 저장본은 실제 모터에서 시험한 펌웨어가 아닙니다. `HB_HARDWARE_VERIFIED=0`으로 부팅하면 영점 보정·측정만 하고 모터 구동 요청은 거부합니다. 출력 허용 설정을 바꾸는 절차는 아래와 같습니다.

## 1. 배선·부품부터 확인

- [실제 커넥터 표](HARDWARE_REVIEW_KO.md)대로 네 게이트 입력과 센서 출력을 배선합니다. J1 핀 2/5는 `3.3GND`입니다.
- 전류 센서 실장 마킹이 **A3B**인지 확인합니다. library ID A2와 Value A3가 다릅니다. A2라면 센서 감도부터 수정해야 합니다.
- 모터 12 V, 게이트 15 V, 로직 3.3 V의 각 공급을 구분합니다. NUCLEO GND는 `3.3GND`에 연결합니다.
- 현재 하드웨어에는 MCU 제어 EN·외부 과전류 break 선이 없습니다. DT도 floating입니다. 네 입력을 단순 GPIO 반전으로 대체하지 않습니다.

## 2. 기본 빌드에서 측정 확인

모터용 DCV1 전원을 분리하고 모터가 멈춘 상태에서 MCU·로직·센서만 구동합니다. 다음 값을 Live Expressions에 추가합니다.

```text
hb_request.run
hb_request.current_A
hb_status.state
hb_status.faults
hb_status.calibrated
hb_status.adc_raw
hb_status.offset_count
hb_status.current_A
hb_status.filtered_A
hb_status.reference_A
hb_status.duty
hb_status.gates_enabled
hb_status.adc_irq_count
```

정상적으로 약 10 ms sensor settle + 51.2 ms 평균 보정 후 `state=2(IDLE)`, `calibrated=1`, `faults=0`, offset 약 2048, current 약 0 A가 됩니다. 이때 네 게이트 입력은 LOW여야 합니다. 영점 보정은 전류가 실제 0일 때만 유효하며, 일정한 작은 전류를 영점으로 잘못 학습하는 경우는 소프트웨어만으로 완전히 검출할 수 없습니다.

`adc_irq_count`는 초당 약 20,000 증가해야 합니다. 정지 상태에서 `run=1`과 양의 지령을 주어도 기본 guard=0에서는 fault `0x10`으로 정지하는 것이 정상입니다. 이 오류를 냈다면 다음 시험 전 MCU를 reset합니다.

## 3. 모터 버스 없이 출력 파형 확인

이 단계는 **DCV1 모터 버스를 물리적으로 분리한 검증용 빌드**에서 수행합니다. `hb_config.h`의 `HB_HARDWARE_VERIFIED`를 1로 설정하고 다시 빌드·다운로드합니다. 이름과 달리 이 매크로 자체가 실측을 수행하거나 하드웨어 확인 사실을 보증하지는 않습니다.

1. J1의 신호를 측정할 수 있게 연결합니다. `hb_request.run=0`을 유지합니다.
2. 보정 완료 후 `hb_request.current_A=0.05`, 그다음 `hb_request.run=1`을 입력합니다.
3. bootstrap 동안 상측 두 입력 LOW, 하측 두 입력 HIGH인지 확인합니다. 이후 A_H/A_L은 20 kHz 상보 신호, B_H는 LOW, B_L은 HIGH여야 합니다.
4. A_H와 A_L이 동시에 HIGH가 되지 않는지 확인하고, 양 전환에서 약 1 µs dead time을 측정합니다. 입력 필터와 실제 MOSFET gate 파형에서는 값이 달라질 수 있습니다.
5. `run=0` 시 네 입력이 모두 LOW인지 확인합니다.

모터 버스가 없으면 지령 전류가 생기지 않아 PI 출력이 최대 듀티 50%까지 올라갑니다. 파형 확인용 조건이며 정상적인 전류 추종 시험 결과가 아닙니다.

## 4. 전류 제한 전원에서 전력단 시험

파형 확인 후 시험용 제한 전원을 사용해 12 V 모터 버스를 연결합니다. 임시 모터 가정은 [별도 문서](MOTOR_ASSUMPTIONS_KO.md)에 기록했습니다. 실제 부품·모터의 허용 조건을 확인해 초기 전류 제한을 정합니다.

MOSFET의 gate–source 전압과 bootstrap `VDDA−VSSA`를 적절한 차동/절연 계측 방법으로 확인합니다. 상측 source는 스위칭하므로 접지형 오실로스코프의 GND를 상측 source에 임의로 연결하지 않습니다. 독립 전류계와 센서 값·부호를 비교합니다.

보정 완료 후 다음 순서로 시작합니다.

```c
hb_request.run = 0;
hb_request.current_A = 0.05f; // 0.05 A부터 시험
hb_request.run = 1;
```

이후 상태는 `3(BOOTSTRAP)` 약 5 ms → `4(RUNNING)`입니다. 지령은 0.5 A/s로 증가합니다. 필요하면 0.1 A, 0.2 A로 단계적으로 올려 `reference_A`, `current_A`, `duty`를 비교합니다. 실제 ADC 값이 노이즈·리플 때문에 지령과 완전히 동일하지는 않습니다.

```c
hb_request.run = 0; // 정지: 최대 다음 ADC callback, 정상 상태에서는 약 50 us 이내
```

정상적인 run 정지는 다음 ADC callback에서 처리합니다. ADC가 중단된 경우 이 시간은 성립하지 않고 SysTick의 별도 timeout을 거칩니다. 전류는 인덕턴스·body diode·부하에 따라 감쇠하므로 입력 LOW가 즉시 권선 전류 0을 의미하지는 않습니다.

## 상태·오류 읽기

| state | 의미 |
|---|---|
| 0 | 센서 안정 대기, 출력 OFF |
| 1 | 무전류 offset 보정, 출력 OFF |
| 2 | IDLE, 출력 OFF |
| 3 | 양 하측 ON으로 bootstrap 충전 |
| 4 | 양의 전류 PI 제어 |
| 5 | 오류 latch, 출력 OFF |

`hb_status.faults`는 bit mask입니다. 복수 원인이 함께 표시될 수 있습니다.

| 값 | 의미 |
|---|---|
| 0x001 | ADC 입력 rail 근처 |
| 0x002 | offset 범위 또는 보정 흔들림 |
| 0x004 | 원시 측정 전류 ±1 A 초과 |
| 0x008 | 음수·상한 초과·NaN/무한대 전류 지령 |
| 0x010 | 기본 출력 허용 guard=0 |
| 0x020 | HAL ADC 오류 |
| 0x040 | 양의 구동 중 예상과 반대의 측정 전류 |
| 0x080 | ADC callback 중단 감시 |
| 0x100 | 타이머·ADC·클럭 설정 불일치 |
| 0x200 | Error_Handler/CPU 예외/비상 정지 함수 |

오류 원인을 해결하고 MCU를 reset합니다. `hb_status`나 내부 fault를 디버거에서 강제로 0으로 지워 계속 운전하지 않습니다.

## 계측과 디버깅의 한계

ADC·필터·ISR 지연을 거치는 소프트웨어 과전류 감시는 MOSFET 단락 보호 회로를 대신하지 않습니다. `>3 ms` ADC timeout은 샘플이 멈춘 후 tick 위상에 따라 약 4–5 ms 수준에서 작동하며, 50 µs 한 주기 누락이나 ADC 이후 12.5 µs 계산 마감을 감시하지 않습니다. 실제 ISR 실행 시간과 샘플 위상은 오실로스코프 또는 CPU cycle counter로 별도 확인해야 합니다.

디버거에서 CPU를 halt해도 이 코드가 자동으로 모든 게이트를 끄는 것은 아닙니다. PWM이 계속되거나, timer freeze 설정에 따라 한 상태에 멈출 수 있습니다. 모터 전원이 연결된 상태에서 breakpoint/halt를 전류제어의 정지 수단으로 사용하지 마세요. 전원을 분리한 상태에서 디버그 동작부터 확인합니다.
