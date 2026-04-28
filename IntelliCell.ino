#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ================= WIFI =================
const char* ssid     = "Shinchan";
const char* password = "shindora@27";
const char* serverIP = "http://10.172.45.244:3000";

// ================= PINS =================
#define ONE_WIRE_BUS  4
#define FAN_PIN       18
#define PELTIER_PIN   19

// ================= SENSORS =================
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
Adafruit_INA219 ina219;

// ================= VARIABLES =================
float temp1, temp2, temp3;
float current, voltage;
float batteryPercent;

// ================= BATTERY % =================
float getBatteryPercentage(float voltage) {
  float percent = (voltage - 9.0) / (12.6 - 9.0) * 100.0;
  if (percent > 100) percent = 100;
  if (percent < 0)   percent = 0;
  return percent;
}

// ================= FAN SPEED =================
void setFanSpeed(int speed) {
  ledcWrite(FAN_PIN, speed);
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  // DS18B20
  sensors.begin();
  int deviceCount = sensors.getDeviceCount();
  Serial.print("DS18B20 sensors found: ");
  Serial.println(deviceCount);

  // INA219
  if (!ina219.begin()) {
    Serial.println("[ERROR] INA219 not found! Check wiring.");
  } else {
    Serial.println("INA219 ready.");
  }

  // PWM fan
  ledcAttach(FAN_PIN, 5000, 8);
  ledcWrite(FAN_PIN, 0);

  // Peltier
  pinMode(PELTIER_PIN, OUTPUT);
  digitalWrite(PELTIER_PIN, LOW);

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

// ================= LOOP =================
void loop() {

  // ── Read DS18B20 Temperature Sensors ──────────────
  sensors.requestTemperatures();

  temp1 = sensors.getTempCByIndex(0);   // Sensor 1
  temp2 = sensors.getTempCByIndex(1);   // Sensor 2
  temp3 = sensors.getTempCByIndex(2);   // Sensor 3

  // Check for bad readings (-127 means sensor not found)
  if (temp1 == -127.0) {
    Serial.println("[WARN] Sensor 1 not responding");
    temp1 = 0.0;
  }
  if (temp2 == -127.0) {
    Serial.println("[WARN] Sensor 2 not responding — using Sensor 1 value");
    temp2 = temp1;   // fallback to sensor 1 if only 1 sensor connected
  }
  if (temp3 == -127.0) {
    Serial.println("[WARN] Sensor 3 not responding — using Sensor 1 value");
    temp3 = temp1;   // fallback to sensor 1 if only 1 sensor connected
  }

  // ── Read INA219 (Voltage + Current) ───────────────
  current       = ina219.getCurrent_mA() / 1000.0;  // mA → A
  voltage       = ina219.getBusVoltage_V();
  batteryPercent = getBatteryPercentage(voltage);

  // ── Serial Monitor Debug ───────────────────────────
  Serial.println("-------- SENSOR READINGS --------");
  Serial.printf("T1 = %.2f °C\n",   temp1);
  Serial.printf("T2 = %.2f °C\n",   temp2);
  Serial.printf("T3 = %.2f °C\n",   temp3);
  Serial.printf("Voltage = %.2f V\n", voltage);
  Serial.printf("Current = %.3f A\n", current);
  Serial.printf("Battery = %.1f %%\n", batteryPercent);

  // ── Send Data to Server ────────────────────────────
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = String(serverIP) +
      "/data?t1="    + String(temp1,        2) +
      "&t2="         + String(temp2,        2) +
      "&t3="         + String(temp3,        2) +
      "&current="    + String(current,      3) +
      "&voltage="    + String(voltage,      2) +
      "&battery="    + String(batteryPercent, 1);

    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      Serial.printf("Data sent OK (HTTP %d)\n", httpCode);
    } else {
      Serial.printf("[ERROR] Send failed: %s\n",
                    http.errorToString(httpCode).c_str());
    }
    http.end();

  } else {
    Serial.println("[WARN] WiFi lost — attempting reconnect...");
    WiFi.reconnect();
  }

  // ── Get Cooling Command from Server ───────────────
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http2;
    http2.begin(String(serverIP) + "/command");
    int code2 = http2.GET();

    if (code2 > 0) {
      String command = http2.getString();
      command.trim();

      Serial.print("Command: ");
      Serial.println(command);

      if (command == "OFF") {
        setFanSpeed(0);
        digitalWrite(PELTIER_PIN, LOW);
      }
      else if (command == "FAN_LOW") {
        setFanSpeed(100);
        digitalWrite(PELTIER_PIN, LOW);
      }
      else if (command == "FAN_HIGH") {
        setFanSpeed(200);
        digitalWrite(PELTIER_PIN, LOW);
      }
      else if (command == "PELTIER") {
        setFanSpeed(255);
        digitalWrite(PELTIER_PIN, HIGH);
      }
      else {
        setFanSpeed(0);
        digitalWrite(PELTIER_PIN, LOW);
      }

    } else {
      Serial.println("[WARN] No command from server");
    }
    http2.end();
  }

  Serial.println("---------------------------------");
  delay(1000);
}