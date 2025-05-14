#include <Arduino.h>
#include <PinChangeInterrupt.h>

// ── RC 수신기 채널 입력핀 ──
const uint8_t CH8_PIN = 10;  // On/Off 스위치 (채널8)
const uint8_t CH6_PIN = 11;  // 밝기 조절   (채널6)
const uint8_t CH7_PIN = 12;  // 색상 제어   (채널7)

// ── LED 출력핀 ──
const uint8_t LED_ONOFF_PIN  = 2;   // On/Off 전용 LED
const uint8_t LED_BRIGHT_PIN = 3;   // 밝기 전용 LED
const uint8_t LED_R_PIN      = 5;   // RGB LED Red   (PWM)
const uint8_t LED_G_PIN      = 6;   // RGB LED Green (PWM)
const uint8_t LED_B_PIN      = 9;   // RGB LED Blue  (PWM)

// ── 펄스폭 측정 변수 ──
volatile unsigned long rise8 = 0, rise6 = 0, rise7 = 0;
volatile unsigned int  w8 = 1500, w6 = 1500, w7 = 1500;
volatile bool          ch8_upd = false, ch6_upd = false, ch7_upd = false;

// ── ISR ──
void ISR_ch8() {
  if (digitalRead(CH8_PIN)) rise8 = micros();
  else {
    unsigned long dt = micros() - rise8;
    w8 = (dt >= 1000 && dt <= 2000) ? dt : 1500;
    ch8_upd = true;
  }
}
void ISR_ch6() {
  if (digitalRead(CH6_PIN)) rise6 = micros();
  else {
    unsigned long dt = micros() - rise6;
    w6 = (dt >= 1000 && dt <= 2000) ? dt : 1500;
    ch6_upd = true;
  }
}
void ISR_ch7() {
  if (digitalRead(CH7_PIN)) rise7 = micros();
  else {
    unsigned long dt = micros() - rise7;
    w7 = (dt >= 1000 && dt <= 2000) ? dt : 1500;
    ch7_upd = true;
  }
}

// ── HSV→RGB 변환 (h:[0–360], s,v:[0–1]) → r,g,b:[0–255] ──
void hsvToRgb(float h, float s, float v, int &r, int &g, int &b) {
  float c = v * s;
  float x = c * (1 - fabs(fmod(h/60.0,2) - 1));
  float m = v - c;
  float r1, g1, b1;
  if      (h <  60) { r1 = c;  g1 = x;  b1 = 0; }
  else if (h < 120) { r1 = x;  g1 = c;  b1 = 0; }
  else if (h < 180) { r1 = 0;  g1 = c;  b1 = x; }
  else if (h < 240) { r1 = 0;  g1 = x;  b1 = c; }
  else if (h < 300) { r1 = x;  g1 = 0;  b1 = c; }
  else              { r1 = c;  g1 = 0;  b1 = x; }
  r = (r1 + m) * 255;
  g = (g1 + m) * 255;
  b = (b1 + m) * 255;
}

void setup(){
  pinMode(CH8_PIN,       INPUT_PULLUP);
  pinMode(CH6_PIN,       INPUT_PULLUP);
  pinMode(CH7_PIN,       INPUT_PULLUP);
  pinMode(LED_ONOFF_PIN,  OUTPUT);
  pinMode(LED_BRIGHT_PIN, OUTPUT);
  pinMode(LED_R_PIN,      OUTPUT);
  pinMode(LED_G_PIN,      OUTPUT);
  pinMode(LED_B_PIN,      OUTPUT);

  attachPCINT(digitalPinToPCINT(CH8_PIN), ISR_ch8, CHANGE);
  attachPCINT(digitalPinToPCINT(CH6_PIN), ISR_ch6, CHANGE);
  attachPCINT(digitalPinToPCINT(CH7_PIN), ISR_ch7, CHANGE);

  Serial.begin(115200);
}

void loop(){
  // 1) 인터럽트 변수 안전 복사
  noInterrupts();
    unsigned int pw8 = w8, pw6 = w6, pw7 = w7;
    bool u8 = ch8_upd, u6 = ch6_upd, u7 = ch7_upd;
    ch8_upd = ch6_upd = ch7_upd = false;
  interrupts();

  // 2) On/Off 전용 LED (D2)
  digitalWrite(LED_ONOFF_PIN, pw8 > 1500 ? HIGH : LOW);

  // 3) 밝기 전용 LED (D3)
  int bright = constrain(map(pw6,1000,2000,0,255), 0, 255);
  if (bright <= 20) digitalWrite(LED_BRIGHT_PIN, LOW);
  else              analogWrite(LED_BRIGHT_PIN, bright);

  // 4) 3색 RGB LED — 색상만 제어, 항상 최대 밝기
  {
    float hue = map(pw7,1000,2000,0,360);
    int r, g, b;
    hsvToRgb(hue, 1.0, 1.0, r, g, b);
    // ↓ 공통 애노드일 경우 PWM 값 반전
    analogWrite(LED_R_PIN, 255 - r);
    analogWrite(LED_G_PIN, 255 - g);
    analogWrite(LED_B_PIN, 255 - b);
  }

  // 5) 디버그 (값 변경 시)
  if (u8 || u6 || u7) {
    Serial.print("CH8="); Serial.print(pw8);
    Serial.print(" CH6="); Serial.print(pw6);
    Serial.print(" CH7="); Serial.print(pw7);
    Serial.print(" Hue="); Serial.print(map(pw7,1000,2000,0,360));
    Serial.print(" BrightLED="); Serial.print(bright);
    Serial.println();
  }

  delay(20);
}

