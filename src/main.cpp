#include <Arduino.h>
#include <SoftSerial.h>
#include <TinyPinChange.h>

#define SWITCH_PIN 1    // Пин в сторону транзистора
#define RX_PIN 2
#define TX_PIN 3
#define PULSEIN_PIN 4   // Пин входящего сигнала (канал для света)

SoftSerial swSerial(RX_PIN, TX_PIN); //(RX, TX)

/*
 * Теущее состояние, единственный источник правды для обработки сигналов:
 * 0 - Не инициализировано
 * 1 - Свет выключен
 * 2 - Слабый свет
 * 3 - Сильный свет
 * 4 - ⚡️⚡️⚡️ Особоый режим ⚡️⚡️⚡️
 */
int STATE = 0;

int lastState = 0;      // Для проверки отсечения 
int virtualState = 0;   // Костыль для сохранения последнего *активного* состояния

unsigned long lastStateChange = 0;
unsigned long lastDebounceTime = 0;

unsigned long debounceDelay = 50;
unsigned long advModeDelay = 1000; // Чувствительность быстрого переключения 2->1->2 (4ый режим)

void setup() {
  pinMode(SWITCH_PIN, OUTPUT); 
  swSerial.begin(9600);
  swSerial.txMode();
  swSerial.println("Firmware started");
}

void sendUpdates() {
  switch (STATE) {
    case 1:
      analogWrite(SWITCH_PIN,0);
      swSerial.println("Mode 1: Lights - OFF");
      break;
    case 2:
      analogWrite(SWITCH_PIN,160);
      swSerial.println("Mode 2: Lights - LOW");
      break;
    case 3:
      analogWrite(SWITCH_PIN,255);
      swSerial.println("Mode 3: Lights - HIGH");
      break;
    case 4:
      analogWrite(SWITCH_PIN,100);
      swSerial.println("Mode 4: Special mode");
      break;
  }
}

void setState() {
  int PULSEIN_DATA = pulseIn(PULSEIN_PIN, HIGH, 25000); // Чтение импульса с канала приемника

  delay(100);

  int normalizedDataValue = 0;

  if(PULSEIN_DATA >= 1200 && PULSEIN_DATA < 1500) normalizedDataValue = 1;
  else if(PULSEIN_DATA >= 1500 && PULSEIN_DATA < 1750) normalizedDataValue = 2;
  else if(PULSEIN_DATA >= 1750) normalizedDataValue = 3;

  if (normalizedDataValue != lastState) {
    // Считанное состояние отличается от предыдущего
    // либо шум, либо мы действительно что-то меняем
    // в любом случае актуализируем дебаунс таймер
    lastDebounceTime = millis();
  }
  swSerial.print("Pulse In Data: ");
  swSerial.print(PULSEIN_DATA);
  swSerial.print(" ---- Software Mode: ");
  swSerial.println(normalizedDataValue);

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Какое-то время не было никаких изменений, считаем что состояние стабильно и мы можем начать обработку
    if (normalizedDataValue != STATE && normalizedDataValue != virtualState) {
      bool fastFingerPass = lastStateChange != 0 && (millis() - lastStateChange) < advModeDelay;
      bool historyPass = virtualState == 1 && normalizedDataValue == 2;
      bool doubleCheckPass = STATE != 4;
      // swSerial.println("New state differs from current, check for new value");
      
      if (fastFingerPass && historyPass && doubleCheckPass) {
        // Быстро щелкнули туда-сюда, переходим в 4ый режим!
        STATE = 4;
        // swSerial.println("Fast finger detected! Locking Front Diff");
      } else {
        // Больше advModeDelay? Значит просто сохраняем текущее значение как новое состояние
        STATE = normalizedDataValue; 
        // swSerial.println("Just old plain state change, no revealed secret modes for now...");
      }

      lastStateChange = millis();
      sendUpdates();
      virtualState = normalizedDataValue;
    }
  }

  lastState = normalizedDataValue;
}

void loop() {
  setState();
}