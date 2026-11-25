# 업로드 H-브리지 회로와 펌웨어 연결 검토

업로드된 KiCad 회로도와 PCB를 수정하지 않고 로컬 `hardware/` 경로에서 분석했다. 이 문서는 해당 파일의 연결을 읽어 펌웨어의 전제를 정리한 것이다. 원본 두 파일은 Git 저장 대상에서 제외했으며, 연결 검사를 재현하려면 사용자가 보유한 파일을 해당 경로에 넣는다. 사용자 확인 조건은 **12V 브러시 DC 모터, NUCLEO-F303ZE**이다.

## 검토 범위와 재현

원본 파일은 KiCad 10 형식이다. 작업 환경에 설치된 KiCad 9에서는 정상적인 netlist export를 수행할 수 없어, Python 표준 라이브러리로 S-expression을 읽었다. 회로도 라이브러리의 핀 위치, 심벌 회전, wire, junction, 전역 라벨, 전원 심벌에서 연결 그래프를 만들고 PCB의 footprint/pad net 그룹과 비교했다.

```sh
python3 scripts/check_hardware_nets.py
python3 scripts/check_hardware_nets.py --json
```

제공된 파일에서는 공유하는 **178개 pad의 연결 그룹이 일치**한다. 스크립트는 J1·전원·센서 연결, EN 고정, DT no-connect도 확인하며 불일치하면 종료 코드 1을 반환한다. 입력을 읽거나 해석할 수 없으면 종료 코드 2를 반환한다. 단일 시트의 현재 설계를 대상으로 하며 계층 회로·버스는 지원하지 않는다.

이 결과는 PCB routing DRC, ERC, 제조 가능성, 실장 부품, 절연 거리 또는 실제 게이트 파형을 검증한 결과가 아니다. 회로도와 PCB에 같은 오류가 있으면 연결 그룹 비교만으로 발견되지 않을 수 있다.

원본 SHA-256:

| 파일 | SHA-256 |
|---|---|
| `hardware/Hbridge.kicad_sch` | `5425e47d1be5a28e48763fe49f11844e365d303b4a514e9912fc66590e873231` |
| `hardware/Hbridge.kicad_pcb` | `be3c438cda5b045aa60d0d609725fb457c63bae6e584ed242f01f78098e08bb3` |

## 실제 드라이버와 J1 연결

IC1·IC2는 **UCC21331CDR**이고 Q1–Q4는 **PSMN014-40YS_115**이다. IC1은 왼쪽 half bridge, IC2는 오른쪽 half bridge를 구동한다. 입력은 모두 active high이다. `PWMA~`와 `PWMB~`는 net 이름이며, 보드에 반전 신호를 만들어 주는 회로는 없다.

| J1 핀 | PCB net | 실제 경로 | 이번 펌웨어의 NUCLEO 핀 |
|---|---|---|---|
| 1 | `PWMA` | R1 → IC1 INA → Q1 상측 | CN11.38, PC0, TIM1_CH1 AF2 |
| 2 | `3.3GND` | 드라이버 입력측 GND | NUCLEO GND (예: CN11.19) |
| 3 | `PWMA~` | R2 → IC1 INB → Q2 하측 | CN12.15, PA7, TIM1_CH1N AF6 |
| 4 | `PWMB` | R3 → IC2 INA → Q3 상측 | CN11.36, PC1, TIM1_CH2 AF2 |
| 5 | `3.3GND` | 드라이버 입력측 GND | NUCLEO GND (예: CN11.19) |
| 6 | `PWMB~` | R4 → IC2 INB → Q4 하측 | CN12.47, PE10, TIM1_CH2N AF2 |

위 표는 CubeMX의 PC0·PA7·PC1·PE10 배치에 맞춘 외부 배선이다. NUCLEO의 CN11·CN12는 ST morpho 헤더다. 헤더 실장 여부와 H-브리지 J1의 1번 표시를 확인하며, Zio를 사용할 수 있는 신호는 [README 배선표](../README.md#배선-요약)에 정리했다. [ST 보드 매뉴얼 UM1974, 표 14·22](https://www.st.com/resource/en/user_manual/um1974-stm32-nucleo144-boards-mb1137-stmicroelectronics.pdf)

## EN, dead time, 풀다운

- IC1·IC2의 EN(pin 5)은 `+3.3V`에 직결되어 있다. MCU에서 제어할 수 있는 EN 선은 없다.
- 두 DT(pin 6)는 no-connect이다. TI 데이터시트의 pin table 및 electrical characteristics에 따르면 DT를 띄우면 dead-time/interlock 기능이 비활성화되어 두 입력 HIGH가 두 출력 HIGH로 전달될 수 있다.
- 따라서 상·하측을 켜는 순서는 **TIM1의 하드웨어 complementary PWM과 dead time**으로 보장해야 한다. 별도 GPIO 쓰기나 단순 신호 반전으로 대체하지 않는다. DT 저항을 추가하는 하드웨어 변경은 이번 저장본에 포함하지 않았다.
- INA/INB와 `3.3GND` 사이의 **외부 입력 풀다운 저항은 없다.** UCC21331 내부 입력 풀다운은 데이터시트상 typ. 90kΩ, 50–185kΩ 범위이다.
- R8/R11/R17/R18의 10kΩ는 각 MOSFET의 **gate–source 출력측 풀다운**이다. 입력측 풀다운과 다르다. R1–R4는 91Ω 직렬 입력 저항이며 C1/C2/C5/C6의 `680`은 통상 68pF capacitor code이다. 실제 실장값은 별도 확인한다.

정방향 전류의 초기 구동은 Q1/Q2를 complementary PWM으로 구동하고 Q3 OFF·Q4 ON을 유지하는 unipolar 방식이다. 이 경우 PWM OFF 구간에는 하측 MOSFET으로 전류가 순환한다. 반대편까지 역상 스위칭하는 bipolar 방식으로 바꾸면 평균 전압 관계가 달라지므로 현재 PI 출력의 의미도 바꿔야 한다.

complementary PWM에서 CCR=0은 하측 MOSFET이 켜지는 제동 상태가 될 수 있다. 모든 입력 LOW가 필요한 정지·fault에는 TIM1 MOE를 무조건 해제하고 OISx/OISxN LOW 및 OSSI/OSSR 설정을 확인한다. EN을 끈다고 설명해서는 안 된다. 공급 전원이 살아 있는 상태에서 디버거를 멈추면 PWM이 계속될 수 있으므로 디버그 freeze 동작도 확인한다.

## 전원·모터·전류 출력 커넥터

| 커넥터 | 핀 1 | 핀 2 | 용도 |
|---|---|---|---|
| `DCV1` | `DCV` | `15GND` | 모터용 DC 12V 입력 |
| `15V1` | `+15V` | `15GND` | 별도 게이트 드라이버 전원 |
| `3.3V1` | `+3.3V` | `3.3GND` | 드라이버 입력측·센서 전원 |
| `Motorout1` | `OUT+` | `OUT-` | 2선 브러시 DC 모터 |
| `Currentsensor1` | `STM32_ADC` | `3.3GND` | CN11.28의 PA0/ADC1_IN1 및 NUCLEO GND로 연결 |

`DCV`와 `+15V`는 서로 다른 net이다. UCC21331CDR의 권장 VDD 최솟값은 **13.5V**이므로 모터용 12V만 게이트 전원으로 사용하면 이 조건을 만족하지 못한다. `15V1`과 `DCV1`의 전원측 GND는 모두 `15GND`이다.

`3.3GND`와 `15GND`는 두 원본 파일에서 서로 분리되어 있다. NUCLEO의 GND는 `3.3GND`에 연결한다. 게이트 드라이버와 전류 센서의 절연 구조 때문에 펌웨어 연결을 위해 두 GND를 추가로 직결할 필요는 없다. 다른 장비·전원·계측기를 연결할 때는 실제 접지 경로도 확인한다.

## 전류 센서의 위치·부호·필터

IC3의 회로도/PCB **Value는 TMCS1108A3BQDRQ1**이다. 다만 회로도의 library ID는 `TMCS1108A2BQDRQ1`로 남아 있다. 같은 핀 배치이지만 감도가 다르므로 **실제 실장 마킹과 BOM이 A3B인지 확인한 뒤 0.200V/A를 적용**한다.

연결은 다음과 같다.

```text
Q1 source / Q2 drain (Current_Sensor)
    → IC3 IN+ (pin 1, 2)
    → IC3 IN− (pin 3, 4)
    → OUT+ / Motorout1 pin 1
    → motor
    → OUT− / Motorout1 pin 2
```

따라서 OUT+에서 모터를 지나 OUT−로 흐르는 전류가 양수이다. 센서는 모터 권선 경로에 직렬로 있어 PWM ON뿐 아니라 하측 프리휠 중에도 전류를 측정할 수 있다. IC3 VS(pin 8)=3.3V, GND(pin 5)=`3.3GND`이다. 실제 A3B의 무전류 출력은 VS/2(약 1.65V), 감도는 0.200V/A이며 3.3V 공급 시 선형 측정 범위는 ±7.25A이다. 이 범위가 모터·PCB의 허용 전류를 뜻하지는 않는다.

VOUT(pin 7) → R13 → `STM32_ADC`이고 C20은 `STM32_ADC`에서 `3.3GND`로 연결된다. C20의 `104K`는 100nF이다. **R13의 원본 문자열 `100J`는 100Ω 명목 표기인지, 3자리 저항 코드 `100`=10Ω인지 파일만으로 확정하지 않았다.** 실장/BOM/측정으로 확인한다.

| R13 실제값 | R13 × C20 시정수 | 차단 주파수 |
|---|---|---|
| 10Ω | 1µs | 약 159kHz |
| 100Ω | 10µs | 약 15.9kHz |

이 필터와 센서 자체 응답 지연은 샘플 시각과 PI 튜닝에 반영해야 한다. ADC offset 교정은 정지한 모터에서 실제 권선 전류가 0인 상태로 수행한다. 외부에서 회전시키는 모터는 출력을 껐더라도 이 조건을 만족하지 않을 수 있다.

## Bootstrap와 초기 구동 확인

IC1의 상측 전원은 R5(5.1Ω)·D1로 충전하며 C9–C12가 각각 10µF, 명목 합계 40µF이다. IC2는 R12·D2 및 C14/C16/C17/C18의 동일한 구조이다. 모든 게이트를 OFF한 상태만으로 bootstrap 충전이 보장되지는 않는다.

이번 펌웨어의 PRECHARGE는 양 하측을 켜고 상측을 끈 상태로 5ms를 유지한다. 명목 R×C는 약 204µs이다. 실제 공급 전압·용량·다이오드·UVLO 상태에 따라 충전 조건이 달라지므로 이 시간만으로 충전 완료를 검출했다고 볼 수 없다. `VDDA−VSSA`를 실제로 확인하고 100% 상측 연속 ON을 허용하지 않는다.

`Core/Inc/hb_config.h`의 `HB_HARDWARE_VERIFIED` 출력 허용 guard를 **1로 바꾸기 전**, 모터 전원 없이 J1의 네 파형과 정지 시 모두 LOW인지 확인하는 제한된 bench 절차를 먼저 준비한다. 실제 출력 파형을 만드는 검증용 빌드는 모터 버스를 분리한 상태에서만 사용하고, 그 결과를 확인한 후 전류 제한 전원에서 각 MOSFET의 **gate–source** 파형과 dead time, bootstrap 전압, 센서 양의 부호를 확인한다. 기본 guard=0인 정상 빌드에서는 모터 구동이 거부되므로 구동 파형이 나오지 않는 것이 정상이다. 상측 gate를 `3.3GND` 또는 일반 접지형 프로브로 직접 측정하지 말고 해당 부유 source를 기준으로 측정 가능한 차동/절연 계측 방법을 사용한다.

업로드 설계에는 별도 과전류 comparator·MCU로 들어오는 fault 선·모터 버스 전압 측정 회로가 없다. ADC 과전류 차단은 센서·필터·변환·ISR 지연을 거치는 소프트웨어 보호이며 단락 차단을 보장하는 회로가 아니다. 초기 전류 지령·duty·PI gain은 낮게 두고 실제 모터 조건에서 검증한다.

## 공식 자료

- [TI UCC21331 데이터시트](https://www.ti.com/lit/ds/symlink/ucc21331.pdf): pin table, 입력 풀다운, DT/EN 동작, VDD/UVLO 조건.
- [TI TMCS1108-Q1 데이터시트](https://www.ti.com/lit/ds/symlink/tmcs1108-q1.pdf): A3B 감도, 부호, 핀 배치, 3.3V 선형 측정 범위.
