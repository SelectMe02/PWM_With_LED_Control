#include <Arduino.h>
#include <PinChangeInterrupt.h>  // 핀 변경 인터럽트 라이브러리 (pin change interrupt) 사용

// ───────────────────────────────────────────────────────────────────────
//  - RC 수신기(RD9S) 서보 커넥터 채널 8 (Switch A) 신호선 → D10 (CH8_PIN)
//  - RC 수신기 서보 커넥터 채널 6 (VrA)       신호선 → D11 (CH6_PIN)
//  - RC 수신기 서보 커넥터 채널 7 (VrB)       신호선 → D12 (CH7_PIN)
//
//  - On/Off 전용 단색 LED (채널8 스위치 상태 표시) → D2 (LED_ONOFF_PIN)
//    ※ 스위치가 ON이면 LED 켜짐, OFF이면 꺼짐
//
//  - 밝기 조절 전용 단색 LED (채널6 밝기 표시) → D3 (PWM) (LED_BRIGHT_PIN)
//    ※ 채널6 펄스폭으로 PWM 밝기 제어
//
//  - 3색(RGB) LED (채널7 색상 제어) → 
//      Red 핀 → D5 (PWM)  (LED_R_PIN)
//      Green 핀 → D6 (PWM) (LED_G_PIN)
//      Blue 핀 → D9 (PWM)  (LED_B_PIN)
//    ※ HSV→RGB 변환으로 Hue 값에 따라 색상 변화, 항상 최대 밝기
//    ※ 공통 애노드(Common Anode) RGB LED 기준으로 PWM 반전 처리
// ───────────────────────────────────────────────────────────────────────

// 수신기 채널 입력 핀 정의
const uint8_t CH8_PIN = 10;  // 채널8: On/Off 스위치 A
const uint8_t CH6_PIN = 11;  // 채널6: 밝기 조절 VrA
const uint8_t CH7_PIN = 12;  // 채널7: 색상 제어 VrB

// LED 출력 핀 정의
const uint8_t LED_ONOFF_PIN  = 2;  // On/Off 전용 LED
const uint8_t LED_BRIGHT_PIN = 3;  // 밝기 전용 LED (PWM 지원)
const uint8_t LED_R_PIN      = 5;  // RGB LED 빨강 채널 (PWM 지원)
const uint8_t LED_G_PIN      = 6;  // RGB LED 초록 채널 (PWM 지원)
const uint8_t LED_B_PIN      = 9;  // RGB LED 파랑 채널 (PWM 지원)

// ───────────────────────────────────────────────────────────────────────
// 2) PWM 펄스폭(pulse width) 측정용 변수
//    - riseX : 신호 HIGH가 시작된 시간(micros())
//    - wX     : HIGH 상태 유지 시간(μs 단위), 보정 후 1000~2000μs
//    - chX_upd: 새로운 값이 들어왔음을 표시하는 플래그
// ───────────────────────────────────────────────────────────────────────
volatile unsigned long rise8 = 0, rise6 = 0, rise7 = 0;
volatile unsigned int  w8 = 1500, w6 = 1500, w7 = 1500;
volatile bool          ch8_upd = false, ch6_upd = false, ch7_upd = false;

// ───────────────────────────────────────────────────────────────────────
// 3) ISR(Interrupt Service Routine) 함수
//    attachPCINT()으로 핀 변경(상승/하강) 감지 시 호출됨
// ───────────────────────────────────────────────────────────────────────
void ISR_ch8() {
  if (digitalRead(CH8_PIN)) {
    // 상승 엣지: HIGH 시작 시간 기록
    rise8 = micros();
  } else {
    // 하강 엣지: HIGH 지속 시간 계산
    unsigned long dt = micros() - rise8;
    // 1000~2000μs 범위 내면 그대로, 아니면 중립값 1500μs 적용
    w8 = (dt >= 1000 && dt <= 2000) ? dt : 1500;
    ch8_upd = true;  // 업데이트 플래그
  }
}

void ISR_ch6() {
  if (digitalRead(CH6_PIN)) {
    rise6 = micros();
  } else {
    unsigned long dt = micros() - rise6;
    w6 = (dt >= 1000 && dt <= 2000) ? dt : 1500;
    ch6_upd = true;
  }
}

void ISR_ch7() {
  if (digitalRead(CH7_PIN)) {
    rise7 = micros();
  } else {
    unsigned long dt = micros() - rise7;
    w7 = (dt >= 1000 && dt <= 2000) ? dt : 1500;
    ch7_upd = true;
  }
}

// ───────────────────────────────────────────────────────────────────────
// 4) HSV → RGB 변환 함수
//    입력: h(0~360°), s(채도 0~1), v(명도 0~1)
//    출력: r,g,b(0~255)
//    - HSV 색상환 모델 기반으로 계산
// ───────────────────────────────────────────────────────────────────────
void hsvToRgb(float h, float s, float v, int &r, int &g, int &b) {
  float c = v * s;
  float x = c * (1 - fabs(fmod(h / 60.0, 2) - 1));
  float m = v - c;
  float r1, g1, b1;

  // Hue가 0°~360° 범위 안에서 각 구간별로 RGB 비율을 설정
  if      (h <  60) { 
    // 0°~60°: 빨강 최대, 초록 c→x 증가, 파랑 0
    r1 = c;  
    g1 = x;  
    b1 = 0; 
  }
  else if (h < 120) { 
    // 60°~120°: 초록 최대, 빨강 c→x 감소, 파랑 0
    r1 = x;  
    g1 = c;  
    b1 = 0; 
  }
  else if (h < 180) { 
    // 120°~180°: 초록 최대, 파랑 0→x 증가, 빨강 0
    r1 = 0;  
    g1 = c;  
    b1 = x; 
  }
  else if (h < 240) { 
    // 180°~240°: 파랑 최대, 초록 c→x 감소, 빨강 0
    r1 = 0;  
    g1 = x;  
    b1 = c; 
  }
  else if (h < 300) { 
    // 240°~300°: 파랑 최대, 빨강 0→x 증가, 초록 0
    r1 = x;  
    g1 = 0;  
    b1 = c; 
  }
  else {           
    // 300°~360°: 빨강 최대, 파랑 c→x 감소, 초록 0
    r1 = c;  
    g1 = 0;  
    b1 = x; 
  } // Hue 구간에 따라 r1,g1,b1 값이 결정됨

  // 0~1 범위값 → 0~255 정수로 변환
  r = (r1 + m) * 255;
  g = (g1 + m) * 255;
  b = (b1 + m) * 255;
}

void setup() {
  // 입력핀 풀업 설정: 스위치나 포텐셔미터 신호에서 잡음 방지
  pinMode(CH8_PIN, INPUT_PULLUP);
  pinMode(CH6_PIN, INPUT_PULLUP);
  pinMode(CH7_PIN, INPUT_PULLUP);

  // LED 출력핀 설정
  pinMode(LED_ONOFF_PIN,  OUTPUT);
  pinMode(LED_BRIGHT_PIN, OUTPUT);
  pinMode(LED_R_PIN,      OUTPUT);
  pinMode(LED_G_PIN,      OUTPUT);
  pinMode(LED_B_PIN,      OUTPUT);

  // 핀 변경 인터럽트 등록 (상승·하강 엣지 모두)
  attachPCINT(digitalPinToPCINT(CH8_PIN), ISR_ch8, CHANGE);
  attachPCINT(digitalPinToPCINT(CH6_PIN), ISR_ch6, CHANGE);
  attachPCINT(digitalPinToPCINT(CH7_PIN), ISR_ch7, CHANGE);

  Serial.begin(115200);  // 디버그용 시리얼 시작
}

void loop() {
  // ───────────────────────────────────────────────────────────────
  // 인터럽트에서 갱신된 전역 변수(w8,w6,w7)를 안정적으로 복사
  // 변화 플래그(ch*_upd)도 함께 복사하여 loop 내에서만 사용
  // ───────────────────────────────────────────────────────────────
  noInterrupts();
    unsigned int pw8 = w8;
    unsigned int pw6 = w6;
    unsigned int pw7 = w7;
    bool u8 = ch8_upd, u6 = ch6_upd, u7 = ch7_upd;
    ch8_upd = ch6_upd = ch7_upd = false;  // 플래그 리셋
  interrupts();

  // ───────────────────────────────────────────────────────────────
  // 2) On/Off LED 제어 (D2 핀)
  //    - pw8 > 1500μs 이면 HIGH(ON), 아니면 LOW(OFF)
  // ───────────────────────────────────────────────────────────────
  digitalWrite(LED_ONOFF_PIN, pw8 > 1500 ? HIGH : LOW);

  // ───────────────────────────────────────────────────────────────
  // 3) 밝기 조절 전용 LED 제어 (D3 핀, PWM)
  //    - w6(1000~2000μs) → map()으로 0~255 PWM 값으로 변환
  //    - threshold(20) 이하일 때 완전 OFF 처리
  // ───────────────────────────────────────────────────────────────
  int bright = constrain(map(pw6, 1000, 2000, 0, 255), 0, 255);
  if (bright <= 20) {
    digitalWrite(LED_BRIGHT_PIN, LOW);  // 아주 어두울 땐 완전 꺼짐
  } else {
    analogWrite(LED_BRIGHT_PIN, bright);
  }

  // ───────────────────────────────────────────────────────────────
  // 4) 3색 RGB LED 제어 (D5, D6, D9 핀, PWM)
  //    - pw7(1000~2000μs) → map()으로 Hue(0~360°) 값으로 변환
  //    - hsvToRgb()로 Hue→(r,g,b) 각각 0~255 값 구함
  //    - 공통 애노드(Common Anode) 방식이므로 255-r/g/b 값을 PWM 출력
  //    - 항상 최대 밝기(v=1.0), 채널6 밝기와는 별도
  // ───────────────────────────────────────────────────────────────
  {
    float hue = map(pw7, 1000, 2000, 0, 360);
    int r, g, b;
    hsvToRgb(hue, 1.0, 1.0, r, g, b);
    analogWrite(LED_R_PIN, 255 - r);
    analogWrite(LED_G_PIN, 255 - g);
    analogWrite(LED_B_PIN, 255 - b);
  }

  // ───────────────────────────────────────────────────────────────
  // 5) 디버그 출력 (채널 값이 바뀔 때만)
  //    - Serial Monitor에서 펄스폭과 Hue, 밝기 상태 확인 가능
  // ───────────────────────────────────────────────────────────────
  if (u8 || u6 || u7) {
    Serial.print("CH8="); Serial.print(pw8);
    Serial.print("  CH6="); Serial.print(pw6);
    Serial.print("  CH7="); Serial.print(pw7);
    Serial.print("  Hue="); Serial.print(map(pw7,1000,2000,0,360));
    Serial.print("  BrightLED="); Serial.println(bright);
  }

  delay(20);  // 20ms마다 갱신 (약 50Hz)
}
