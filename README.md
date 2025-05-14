# PWM_With_LED_Control

---
# Youtube Link

---


# 🔥 RC PWM 기반 LED 제어 시스템

RC 송수신기(RD9S)로부터 들어오는 PWM 신호를 이용해 **3개 LED**를 제어하는 프로젝트입니다.  
- 🔲 **On/Off 전용 LED** (D2)  
- 🌗 **밝기 조절 전용 LED** (D3)  
- 🌈 **RGB LED (3색)** (D5/D6/D9)

---

## 🎯 기능 요구 사항

| 채널 | 조작                          | 기능                                         |
|:----:|:----------------------------:|:-------------------------------------------:|
| CH8  | 왼쪽 상단 토글 스위치 (Switch A) | 💡 On/Off 전용 LED 켜기/끄기                  |
| CH6  | 오른쪽 조이스틱 상/하 (VrA)     | 🔆 밝기 전용 LED 밝기 조절 (0~255 PWM)         |
| CH7  | 오른쪽 조이스틱 좌/우 (VrB)     | 🎨 RGB LED 색상 연속 변화 (Hue 기반)           |

---

## 🔌 배선 연결

### 1. 전원 & 접지  
- **5V (아두이노 5V) →** 수신기 VCC, LED 모듈 공통 애노드(+5V)  
- **GND (아두이노 GND) →** 수신기 GND, LED 모듈 GND

### 2. RC 수신기 → 아두이노 입력 핀 (풀업)  
| RC 채널 | 입력 용도           | Arduino 핀  |
|:-------:|:------------------:|:-----------:|
| **CH8** | On/Off 스위치      | D10         |
| **CH6** | 밝기 조절          | D11         |
| **CH7** | 색상 제어(Hue)     | D12         |

### 3. LED 제어 핀  
| LED 종류               | Arduino 핀    | 메모                                                  |
|:---------------------:|:------------:|:----------------------------------------------------:|
| **On/Off 전용**       | D2           | 스위치(CH8) 상태 표시                                 |
| **밝기 전용 (PWM)**   | D3           | CH6 → `map(1000~2000μs → 0~255)`                      |
| **RGB Red (PWM)**     | D5           | CH7 → Hue 변환 후 Red 채널                            |
| **RGB Green (PWM)**   | D6           | CH7 → Hue 변환 후 Green 채널                          |
| **RGB Blue (PWM)**    | D9           | CH7 → Hue 변환 후 Blue 채널                           |

---

## 📚 사용 라이브러리

- **PinChangeInterrupt**: 디지털 핀의 상태 변화를 인터럽트로 감지  
  ```cpp
  #include <PinChangeInterrupt.h>```

---

### 🔄 핀 변경 인터럽트(Pin Change Interrupt) 상세

#### 1) 인터럽트 등록  
```cpp
// D10(CH8_PIN) → PCINT 번호로 변환
uint8_t pcintNum = digitalPinToPCINT(CH8_PIN);

// CH8_PIN 핀에 상승/하강 엣지 모두 감지
attachPCINT(pcintNum, ISR_ch8, CHANGE);

// CHANGE 옵션: HIGH→LOW, LOW→HIGH 모두 트리거
// 필요에 따라 RISING 또는 FALLING 으로 변경 가능'''

## 2) ISR 작성

```cpp
void ISR_ch8() {
  if (digitalRead(CH8_PIN)) {
    // ↑ 상승 엣지: HIGH 신호 시작 시점 기록
    rise8 = micros();
  } else {
    // ↓ 하강 엣지: HIGH 지속 시간(펄스폭) 계산
    unsigned long dt = micros() - rise8;
    // 유효 범위(1000~2000µs) 외 값은 중립(1500µs)으로 보정
    w8 = (dt >= 1000 && dt <= 2000) ? dt : 1500;
    ch8_upd = true;  // 새로운 데이터 도착 플래그
  }
}'''

## 3) 🧠 내부 동작 원리

- **🧠 PCINT 그룹**:  
  ATmega328P에는 `PCINT0`~`PCINT23`까지 총 24개의 핀 변경 인터럽트가 있으며,  
  이는 **PORTB**, **PORTC**, **PORTD**의 세 그룹으로 나뉘어 존재합니다.

- **⚙️ attachPCINT()**:  
  지정한 핀에 대해 `PCMSKx`, `PCICR` 레지스터를 설정하여 인터럽트를 활성화합니다.

---

### 📈 인터럽트 발생 흐름

1. 하드웨어가 핀의 상태 변화 감지  
2. 해당 ISR (예: `ISR_ch8`) 실행  
3. `micros()`를 사용하여 펄스폭(μs) 정확히 측정

---

## 4) 🚨 ISR 작성 시 주의사항

- ✔️ **최소한의 연산만 수행**  
  ISR 함수는 **짧고 빠르게** 실행되어야 하며, 루프나 복잡한 계산은 지양해야 합니다.

- ❌ **블로킹 함수 금지**  
  ISR 내부에서는 `delay()`, `Serial.print()` 등의 **시간 지연 함수 사용은 금지**해야 합니다.  
  → 이런 함수는 전체 시스템 타이밍을 깨뜨릴 수 있습니다.

- 🔒 **전역 변수는 volatile로 선언**  
  메인 루프(`loop()`)와 ISR이 공유하는 변수는 반드시 `volatile`로 선언해야 합니다.  
  그렇지 않으면 컴파일러 최적화에 의해 올바르게 동작하지 않을 수 있습니다.

```cpp
// 예시: ISR에서 갱신하고 loop()에서 사용하는 변수
volatile unsigned int w8 = 1500;
volatile bool ch8_upd = false;
