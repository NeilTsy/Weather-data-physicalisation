#include <Servo.h>
#include <EEPROM.h>

const int LIGHT_SENSOR_PIN = A1;
const int JOYSTICK_X_PIN = A2;
const int JOYSTICK_Y_PIN = A3;
const int JOYSTICK_BUTTON_PIN = 2;
const int SUN_LED_PIN = 3;
const int SUNNY_LED_PIN = 4;
const int CLOUDY_LED_PIN = 5;
const int WIND_LED_PIN = 6;
const int BUZZER_PIN = 7;
const int WIND_SWITCH_PIN = 8;
const int MOTOR1_PIN = 9;
const int MOTOR2_PIN = 10;
const int FAN_PIN = 11;
const int TILT_SWITCH_MIN = 12;
const int TILT_SWITCH_MAX = 13;

const int JOYSTICK_THRESHOLD_LOW = 400;
const int JOYSTICK_THRESHOLD_HIGH = 600;
const int LIGHT_THRESHOLD = 500;

enum Mode { NORMAL, PREDICT };
Mode currentMode = NORMAL;

const String DAYS[7] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

struct WeatherData {
  String weather;
  String wind;
};

WeatherData weatherDataset[7];
int currentDayIndex = 0;
int lastDayIndex = 0;

Servo dateMotor;
Servo cloudMotor;
int dateAngle = 0;
int cloudAngle = 90;

bool dataReceived = false;

const int MAX_STR_LEN = 10;
const int EEPROM_BASE_ADDR = 0;
const int RECORD_SIZE = 2 * MAX_STR_LEN;

void eepromWriteString(int startAddr, const String &data) {
  int len = data.length();
  if (len > MAX_STR_LEN - 1) len = MAX_STR_LEN - 1;
  for (int i = 0; i < len; i++) {
    EEPROM.write(startAddr + i, data[i]);
  }
  EEPROM.write(startAddr + len, '\0');
}

String eepromReadString(int startAddr) {
  char buf[MAX_STR_LEN];
  for (int i = 0; i < MAX_STR_LEN - 1; i++) {
    buf[i] = EEPROM.read(startAddr + i);
    if (buf[i] == '\0') {
      buf[i] = '\0';
      return String(buf);
    }
  }
  buf[MAX_STR_LEN - 1] = '\0';
  return String(buf);
}

void loadWeatherDataFromEEPROM() {
  dataReceived = false;
  for (int i = 0; i < 7; i++) {
    int addr = EEPROM_BASE_ADDR + i * RECORD_SIZE;
    weatherDataset[i].weather = eepromReadString(addr);
    weatherDataset[i].wind = eepromReadString(addr + MAX_STR_LEN);
    if (weatherDataset[i].weather.length() > 0) dataReceived = true;
  }
}

void saveWeatherDataToEEPROM() {
  for (int i = 0; i < 7; i++) {
    int addr = EEPROM_BASE_ADDR + i * RECORD_SIZE;
    eepromWriteString(addr, weatherDataset[i].weather);
    eepromWriteString(addr + MAX_STR_LEN, weatherDataset[i].wind);
  }
}

void setup() {
  pinMode(SUN_LED_PIN, OUTPUT);
  pinMode(SUNNY_LED_PIN, OUTPUT);
  pinMode(CLOUDY_LED_PIN, OUTPUT);
  pinMode(WIND_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(JOYSTICK_BUTTON_PIN, INPUT_PULLUP);
  pinMode(WIND_SWITCH_PIN, INPUT_PULLUP);
  pinMode(TILT_SWITCH_MIN, INPUT_PULLUP);
  pinMode(TILT_SWITCH_MAX, INPUT_PULLUP);

  dateMotor.attach(MOTOR1_PIN);
  cloudMotor.attach(MOTOR2_PIN);
  dateMotor.write(dateAngle);
  cloudMotor.write(cloudAngle);

  Serial.begin(9600);

  loadWeatherDataFromEEPROM();

  if (!dataReceived) {
    Serial.println("No EEPROM data found. Loading default dataset.");
    for (int i = 0; i < 7; i++) {
      weatherDataset[i].weather = "Sunny";
      weatherDataset[i].wind = "NoWind";
    }
    saveWeatherDataToEEPROM();
  } else {
    Serial.println("Loaded weather data from EEPROM:");
    for (int i = 0; i < 7; i++) {
      Serial.print(DAYS[i]);
      Serial.print(",");
      Serial.print(weatherDataset[i].weather);
      Serial.print(",");
      Serial.println(weatherDataset[i].wind);
    }
  }

  updateWeatherDisplay();
  digitalWrite(SUN_LED_PIN, HIGH);
}

void loop() {
  receiveWeatherData();

  if (currentMode == NORMAL) {
    normalMode();
  } else {
    predictMode();
  }

  delay(50);
}

void receiveWeatherData() {
  static String input = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      input.trim();
      if (input.length() > 0) {
        parseWeatherLine(input);
      }
      input = "";
    } else {
      input += c;
    }
  }
}

void parseWeatherLine(String line) {
  int firstComma = line.indexOf(',');
  int secondComma = line.indexOf(',', firstComma + 1);
  if (firstComma != -1 && secondComma != -1) {
    String dayStr = line.substring(0, firstComma);
    String weatherStr = line.substring(firstComma + 1, secondComma);
    String windStr = line.substring(secondComma + 1);

    int dayIndex = -1;
    for (int i = 0; i < 7; i++) {
      if (dayStr.equalsIgnoreCase(DAYS[i])) {
        dayIndex = i;
        break;
      }
    }

    if (dayIndex != -1) {
      weatherDataset[dayIndex].weather = weatherStr;
      weatherDataset[dayIndex].wind = windStr;
      dataReceived = true;

      int addr = EEPROM_BASE_ADDR + dayIndex * RECORD_SIZE;
      eepromWriteString(addr, weatherStr);
      eepromWriteString(addr + MAX_STR_LEN, windStr);

      updateWeatherDisplay();

      Serial.print("Updated: ");
      Serial.println(line);
    }
  }
}

void normalMode() {
  int xValue = analogRead(JOYSTICK_X_PIN);
  if (xValue < JOYSTICK_THRESHOLD_LOW) {
    currentDayIndex = (currentDayIndex + 6) % 7;
    updateWeatherDisplay();
    delay(300);
  } else if (xValue > JOYSTICK_THRESHOLD_HIGH) {
    currentDayIndex = (currentDayIndex + 1) % 7;
    updateWeatherDisplay();
    delay(300);
  }

  int yValue = analogRead(JOYSTICK_Y_PIN);
  if (yValue < JOYSTICK_THRESHOLD_LOW) {
    lastDayIndex = currentDayIndex;
    currentMode = PREDICT;
    Serial.println("MODE:PREDICT");
    delay(300);
  }
}

void predictMode() {
  int lightValue = analogRead(LIGHT_SENSOR_PIN);
  bool isSunny = (lightValue > LIGHT_THRESHOLD);

  if (isSunny) {
    safeMoveCloudMotor(0);
    digitalWrite(SUNNY_LED_PIN, HIGH);
    digitalWrite(CLOUDY_LED_PIN, LOW);
  } else {
    safeMoveCloudMotor(90);
    digitalWrite(SUNNY_LED_PIN, LOW);
    digitalWrite(CLOUDY_LED_PIN, HIGH);
  }

  bool isWindy = (digitalRead(WIND_SWITCH_PIN) == HIGH);

  if (isWindy) {
    analogWrite(FAN_PIN, 255);
    digitalWrite(WIND_LED_PIN, HIGH);
  } else {
    analogWrite(FAN_PIN, 0);
    digitalWrite(WIND_LED_PIN, LOW);
  }

  if (digitalRead(JOYSTICK_BUTTON_PIN) == LOW) {
    checkPrediction(isSunny, isWindy);
    delay(300);
  }

  int yValue = analogRead(JOYSTICK_Y_PIN);
  if (yValue > JOYSTICK_THRESHOLD_HIGH) {
    currentMode = NORMAL;
    currentDayIndex = lastDayIndex;
    updateWeatherDisplay();
    Serial.println("MODE:NORMAL");
    delay(300);
  }
}

// Motor movement
void safeMoveDateMotor(int targetAngle) {
  targetAngle = constrain(targetAngle, 0, 180);
  int step = (targetAngle > dateAngle) ? 1 : -1;

  while (dateAngle != targetAngle) {
    if (step == 1 && digitalRead(TILT_SWITCH_MAX) == LOW) break;
    else if (step == -1 && digitalRead(TILT_SWITCH_MIN) == LOW) break;

    dateAngle += step;
    dateMotor.write(dateAngle);
    delay(15);
  }
}

void safeMoveCloudMotor(int targetAngle) {
  targetAngle = constrain(targetAngle, 0, 90);
  int step = (targetAngle > cloudAngle) ? 1 : -1;

  while (cloudAngle != targetAngle) {
    if (step == 1 && digitalRead(TILT_SWITCH_MAX) == LOW) break;
    else if (step == -1 && cloudAngle <= 0) break;

    cloudAngle += step;
    cloudMotor.write(cloudAngle);
    delay(15);
  }
}

void updateWeatherDisplay() {
  int targetAngle = map(currentDayIndex, 0, 6, 0, 180);
  safeMoveDateMotor(targetAngle);

  digitalWrite(SUNNY_LED_PIN, (weatherDataset[currentDayIndex].weather == "Sunny") ? HIGH : LOW);
  digitalWrite(CLOUDY_LED_PIN, (weatherDataset[currentDayIndex].weather == "Cloudy") ? HIGH : LOW);

  if (weatherDataset[currentDayIndex].weather == "Cloudy") {
    safeMoveCloudMotor(90);
  } else {
    safeMoveCloudMotor(0);
  }

  bool isWindy = (weatherDataset[currentDayIndex].wind == "Windy");
  digitalWrite(WIND_LED_PIN, isWindy ? HIGH : LOW);
  analogWrite(FAN_PIN, isWindy ? 255 : 0);

  Serial.print(DAYS[currentDayIndex]);
  Serial.print(",");
  Serial.print(weatherDataset[currentDayIndex].weather);
  Serial.print(",");
  Serial.println(weatherDataset[currentDayIndex].wind);
}

void checkPrediction(bool predictedSunny, bool predictedWindy) {
  int nextDayIndex = (lastDayIndex + 1) % 7;
  WeatherData actual = weatherDataset[nextDayIndex];

  String predictedWeather = predictedSunny ? "Sunny" : "Cloudy";
  String predictedWind = predictedWindy ? "Windy" : "NoWind";

  bool weatherCorrect = (predictedWeather == actual.weather);
  bool windCorrect = (predictedWind == actual.wind);

  if (weatherCorrect && windCorrect) {
    beep(1);
    Serial.println("PREDICT:CORRECT");
  } else {
    beep(3);
    Serial.println("PREDICT:WRONG");
    Serial.print("ACTUAL:");
    Serial.print(actual.weather);
    Serial.print(",");
    Serial.println(actual.wind);
  }
}

void beep(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < times - 1) delay(150);
  }
}
