#include <OneWire.h>
#include <DallasTemperature.h>

// -------- PINS --------
#define VOLT_TOTAL 34
#define VOLT_CELL1 35
#define CURRENT_PIN 32
#define TEMP_PIN 33

#define MOSFET1 25
#define MOSFET2 26
#define RELAY_PIN 27

// -------- CONSTANTS --------
float ADC_REF = 3.3;
int ADC_RES = 4095;
float RATIO = 3.0;
float BALANCE_THRESHOLD = 0.9;

// Current sensor
float sensitivity = 0.185;
float offset = 2.5;

// -------- SOC VARIABLES --------
float soc = 80.0;
float capacity = 2.0;
unsigned long lastTime = 0;

// -------- TEMP --------
OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

// -------- FUNCTIONS --------
float readVoltage(int pin) {
  int adc = analogRead(pin);
  return ((adc * ADC_REF) / ADC_RES) * RATIO;
}

float readCurrent() {
  int adc = analogRead(CURRENT_PIN);
  float voltage = (adc * ADC_REF) / ADC_RES;
  return (voltage - offset) / sensitivity;
}

float readTemperature() {
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

// -------- SETUP --------
void setup() {
  Serial.begin(115200);

  pinMode(MOSFET1, OUTPUT);
  pinMode(MOSFET2, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, HIGH); // OFF initially (active LOW relay)

  sensors.begin();

  lastTime = millis();
}

// -------- LOOP --------
void loop() {

  // -------- READ VALUES --------
  float totalV = readVoltage(VOLT_TOTAL);
  float cell1 = readVoltage(VOLT_CELL1);
  float cell2 = totalV - cell1;

  float current = readCurrent();
  float temp = readTemperature();

  float diff = abs(cell1 - cell2);

  // -------- SOC CALCULATION --------
  unsigned long now = millis();
  float dt = (now - lastTime) / 3600000.0;
  lastTime = now;

  // Using raw current directly (no filter)
  if (current > 0) {
    soc = soc - (current * dt / capacity) * 100.0;
  }

  float soc_voltage = ((totalV - 6.0) / 2.4) * 100.0;
  soc_voltage = constrain(soc_voltage, 0, 100);

  soc = 0.95 * soc + 0.05 * soc_voltage;
  soc = constrain(soc, 0, 100);

  // -------- BALANCING --------
  bool balancing = false;

  if (cell1 > cell2 + BALANCE_THRESHOLD) {
    digitalWrite(MOSFET1, HIGH);
    digitalWrite(MOSFET2, LOW);
    balancing = true;
  }
  else if (cell2 > cell1 + BALANCE_THRESHOLD) {
    digitalWrite(MOSFET2, HIGH);
    digitalWrite(MOSFET1, LOW);
    balancing = true;
  }
  else {
    digitalWrite(MOSFET1, LOW);
    digitalWrite(MOSFET2, LOW);
  }

  // -------- PROTECTION --------
  bool fault = false;

  // Overvoltage (FIXED)
  if (cell1 > 4.2 || cell2 > 4.2) fault = true;

  // Undervoltage
  if (cell1 < 2.0 || cell2 < 2.0) fault = true;

  // Severe imbalance
  if (diff > 0.6) fault = true;

  // Temperature
  if (temp > 45) fault = true;

  // Overcurrent
  if (abs(current) > 1.0) fault = true;

  // Reverse current
  if (current < -0.1) fault = true;

  // -------- RELAY CONTROL --------
  if (fault) {
    digitalWrite(RELAY_PIN, HIGH);  // OFF
  } else {
    digitalWrite(RELAY_PIN, LOW);   // ON
  }

  // -------- SERIAL OUTPUT --------
  Serial.print("C1: "); Serial.print(cell1, 2);
  Serial.print(" C2: "); Serial.print(cell2, 2);
  Serial.print(" Tot: "); Serial.print(totalV, 2);
  Serial.print(" I: "); Serial.print(current, 2);
  Serial.print(" T: "); Serial.print(temp, 1);
  Serial.print(" SoC: "); Serial.print(soc, 1);

  Serial.print(" Diff: "); Serial.print(diff, 2);

  if (balancing) Serial.print(" BALANCING ON ");
  else Serial.print(" BALANCED");

  if (fault) Serial.println(" FAULT");
  else Serial.println(" NORMAL");

  delay(300);
}