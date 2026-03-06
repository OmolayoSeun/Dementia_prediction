#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <MAX30105.h>
#include <heartRate.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "Diastolic.h"
#include "Systolic.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SAMPLE_SIZE 400
#define BTN 15

StaticJsonDocument<128> doc;
const char* ssid = "Dr. P";
const char* password = "99999999";

float data[SAMPLE_SIZE];
float systolicVal, diastolicVal;
float features[6];
int32_t result[4];
char buffer[50];

String baseUrl = "url1";
String baseUrl2 = "url2";
String userId = "id";

MAX30105 particleSensor;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Systolic systolic;
Diastolic diastolic;

int8_t BMI;

void setup() {
  pinMode(BTN, INPUT_PULLUP);

  Serial.begin(9600);
  Wire.setClock(400000);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextColor(WHITE);

  particleSensor.begin(Wire, I2C_SPEED_FAST);
  particleSensor.setup();

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print("Demantia");
  display.setCursor(0, 20);
  display.print("Predicting ");
  display.setCursor(0, 40);
  display.print("Device ");
  display.display();
  delay(1000);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    display.setTextSize(1);
    display.setCursor(0, 55);
    display.print("Connecting...");
    display.display();
    delay(500);
  }
  while (!getParam()) {
    display.setTextSize(1);
    display.setCursor(0, 55);
    display.print("Getting info...");
    display.display();
    delay(500);
  }
  Serial.println("digitalRead(BTN): ");
  Serial.println(digitalRead(BTN));

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 5);
  display.print("Click button ");
  display.setCursor(0, 25);
  display.print("to start. ");
  display.display();
}

void loop() {

  if (digitalRead(BTN) == LOW) {
    delay(200);
    getPPGSignal();
  }
}

bool getParam() {
  HTTPClient http;

  http.begin(baseUrl + userId);
  http.addHeader("Content-Type", "application/json");

  if (http.GET() > 0) {

    DeserializationError error = deserializeJson(doc, http.getString());

    if (error) {
      http.end();
      return false;
    }

    features[5] = doc["age"].as<float>();
    features[4] = doc["bmi"].as<float>();

    if (features[4] < 19) BMI = 1;
    else if (features[4] < 25) BMI = 2;
    else BMI = 3;

  } else {
    http.end();
    return false;
  }
  http.end();
  return true;
}

void getPPGSignal() {
  while (1) {
    if (checkForBeat(particleSensor.getIR())) {

      display.clearDisplay();
      display.setTextSize(3);
      display.setCursor(10, 20);
      display.print("Hold");
      display.display();

      for (int16_t i = 0; i < SAMPLE_SIZE; i++) {
        data[i] = particleSensor.getIR();
      }

      if (data[SAMPLE_SIZE - 7] < 12000.0 && data[SAMPLE_SIZE - 2] < 12000.0) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 10);
        display.print("Distorted PPG");
        display.setCursor(0, 20);
        display.print("signal");
        display.display();
        delay(5000);

        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 5);
        display.print("Click button ");
        display.setCursor(0, 25);
        display.print("to start. ");
        display.display();
        return;
      }

      float average = 0.0;
      for (int16_t i = 0; i < SAMPLE_SIZE; i++) {
        average += (data[i] / 1000.0f);
      }
      average = (average / SAMPLE_SIZE) * 1000;

      // find the highest peak (foot)
      float temp = data[0];
      int16_t pos = 0;

      for (int16_t i = 1; i < SAMPLE_SIZE; i++) {
        if (data[i] > temp) {
          temp = data[i];
          pos = i;
        }
      }
      features[0] = temp;

      // find lowest peak (systolic peak)
      pos++;
      temp = data[pos];

      for (int16_t i = pos; i < SAMPLE_SIZE; i++) {
        if (data[i] > temp && (features[0] - temp) > 250) break;
        temp = data[i];
        pos = i;
      }
      features[1] = temp;

      // find diastolic notch
      pos++;
      temp = data[pos];

      for (int16_t i = pos; i < SAMPLE_SIZE; i++) {
        if (temp > data[i] && temp > features[1] && temp < features[0]) break;
        temp = data[i];
        pos = i;
      }
      features[2] = temp;

      // find diastolic peak
      pos++;
      temp = data[pos];
      for (int16_t i = pos; i < SAMPLE_SIZE; i++) {
        if (temp < data[i] && temp > features[1] && temp < features[2]) break;
        temp = data[i];
        pos = i;
      }
      features[3] = temp;

      for (int i = 0; i < 6; i++) {
        Serial.print("x[");
        Serial.print(i);
        Serial.print("] = ");
        Serial.println(features[i]);
      }

      result[0] = (int32_t)(predictBGL(average, BMI));
      result[1] = (int32_t)(systolic.predict(features));
      result[2] = (int32_t)(diastolic.predict(features));

      Serial.print("Blood glucose: ");
      Serial.println(result[0]);
      Serial.print("Blood Pressure: ");
      Serial.print(result[1]);
      Serial.print("/");
      Serial.println(result[2]);

      display.clearDisplay();
      display.setTextSize(1);

      display.setCursor(0, 5);
      display.print("BGL: ");
      display.print(result[0]);

      display.setCursor(0, 15);
      display.print("BP: ");
      display.print(result[1]);
      display.print("/");
      display.print(result[2]);

      display.setCursor(0, 25);
      display.print("CHOL STATUS: ");

      if (result[0] > 150 && result[1] > 130 and result[2] > 80) {
        result[3] = 1;
        display.print("Yes");
      } else {
        result[3] = 0;
        display.print("No");
      }
      display.display();

      delay(500);
      getDementialStatus();
      break;
    } else {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 10);
      display.print("Put your finger");
      display.setCursor(0, 20);
      display.print("on the sensor");
      display.display();
    }
  }
}

void getDementialStatus() {
  int8_t retryCount = 0;

  while (WiFi.status() != WL_CONNECTED) {
    display.setTextSize(1);
    display.setCursor(0, 35);
    display.print("WiFi");
    display.setCursor(0, 45);
    display.print("Disconnected");
    display.display();
    delay(200);

    if (retryCount > 10) return;
    retryCount++;
  }

  HTTPClient http;

  http.begin(baseUrl);
  http.addHeader("Content-Type", "application/json");

  const char* chol = (result[3] == 1) ? "Yes" : "No";

  snprintf(buffer, sizeof(buffer), "{\"userId\": \"123456\",\"bloodGlucose\": \"%d\", \"bloodPressure\":\"%d/%d\",\"cholesterol\":\"%s\"}",
           result[0], result[1], result[2], chol);

  retryCount = 0;

  while (!(http.POST(buffer) > 0)) {
    display.setTextSize(1);
    display.setCursor(0, 35);
    display.print("                ");
    display.setCursor(0, 45);
    display.print("                ");
    display.display();

    display.setTextSize(1);
    display.setCursor(0, 35);
    display.print("Failed");
    display.setCursor(0, 45);
    display.print("Retrying...");
    display.display();
    delay(500);

    if (retryCount > 10) {
      http.end();
      return;
    }
    retryCount++;
  }

  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("Waiting for ");
  display.setCursor(0, 45);
  display.print("response...");
  display.display();
  http.end();

  delay(10000);

  HTTPClient httpGet;
  httpGet.begin(baseUrl2 + userId;);

  retryCount = 0;

  while (!(httpGet.GET() > 0)) {
    // display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 35);
    display.print("Failed");
    display.setCursor(0, 45);
    display.print("Uploading");
    display.display();
    delay(500);

    if (retryCount > 10) {
      httpGet.end();
      return;
    }
    retryCount++;
  }

  DeserializationError error = deserializeJson(doc, httpGet.getString());
  if (error) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Test Result");
    display.setCursor(0, 20);
    display.print("Null: Null");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Test Result");
    display.setCursor(0, 15);
    display.print("Prediction: ");
    display.print(doc["prediction"].as<String>());

    display.setCursor(0, 30);
    display.print("Time: ");
    display.print(doc["timestamp"].as<String>());
    display.display();
  }

  display.setCursor(0, 45);
  display.print("Click to cancel");
  display.print(doc["timestamp"].as<String>());
  display.display();
  http.end();

  while (digitalRead(BTN) != LOW)
    ;
  delay(300);
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 5);
  display.print("Click button ");
  display.setCursor(0, 25);
  display.print("to start. ");
  display.display();
}

int32_t predictBGL(float ppgValue, byte category) {

  const float underweight_coeff = -0.00044198;
  const float underweight_intercept = 133.42150013;
  const float moderate_coeff = 0.00031054;
  const float moderate_intercept = 64.63886263;
  const float overweight_coeff = -0.00091497;
  const float overweight_intercept = 195.88946766;

  if (category == 1) {
    return (int32_t)(underweight_coeff * ppgValue + underweight_intercept);
  } else if (category == 2) {
    return (int32_t)(moderate_coeff * ppgValue + moderate_intercept);
  } else if (category == 3) {
    return (int32_t)(overweight_coeff * ppgValue + overweight_intercept);
  }
  return 0;
}
