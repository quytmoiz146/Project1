#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>

RTC_DS1307 rtc;

#define LED 42
#define BUZZER 39
#define INPUT_PIN 33
#define RS485_EN 34

HardwareSerial RS485Serial(1);

unsigned long startTime = 0;
unsigned long currentTime = 0;
unsigned long lastLedTime = 0;
unsigned long lastBuzzerTime = 0;
unsigned long lastSend = 0;

bool ledState = 1;
bool buzzerState = 0;
bool currentInputState = 0;
bool lastInputState = 0;
bool active = false;

void Led_Buzzer();
void Real_Time();

void setup() {
  Serial.begin(9600);

  pinMode(INPUT_PIN, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(LED, HIGH);
  digitalWrite(BUZZER, LOW);

  Wire.begin(8, 9);

  if(!rtc.begin()){
    Serial.println("Không tìm thấy DS1307");
    while(1);
  }

  if(!rtc.isrunning()){
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  pinMode(RS485_EN, OUTPUT);
  digitalWrite(RS485_EN, LOW);

  RS485Serial.begin(9600, SERIAL_8N1, 18, 17);
}

void loop() {
  Led_Buzzer();
  Real_Time();
}

void Led_Buzzer(){
  currentInputState = digitalRead(INPUT_PIN);
  if(currentInputState != lastInputState){
    active = true;
    startTime = millis();
  }
  lastInputState = currentInputState;

  if(active){
    currentTime = millis();
    if(currentTime - startTime >= 30000){
      active = false;
      digitalWrite(LED, HIGH);
      digitalWrite(BUZZER, LOW);
      return;
    }

    if(currentTime - lastBuzzerTime >= 1000){
      lastBuzzerTime = currentTime;
      buzzerState = !buzzerState;
      digitalWrite(BUZZER, buzzerState);
    }

    if(currentTime - lastLedTime >= 500){
      lastLedTime = currentTime;
      ledState = !ledState;
      digitalWrite(LED, ledState);
    }
  }
}

void Real_Time(){
  unsigned long nowMillis = millis();
  if(nowMillis - lastSend >= 1000){
    lastSend = nowMillis;
    DateTime now = rtc.now();
    char data[30];
    sprintf(data, "%02d:%02d:%02d %02d/%02d/%04d", now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year());
    digitalWrite(RS485_EN, HIGH);
    RS485Serial.println(data);
    delay(10);
    digitalWrite(RS485_EN, LOW);
    Serial.println(data);
  }
}