#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>
#include <Preferences.h>

// ================= TELEGRAM =================
#define TG_TOKEN "8110705370:AAE3CE1_oV6Kx4RCNb4yEDFFBlS8Ze3Rrow"
#define TG_CHAT_ID "8703269706"

// ================= WIFI =================
char ssid[] = "Airtel_COWORKINSTA_A";
char pass[] = "Bandinsta1234";

// ================= PINS =================
#define BUZZER_PIN 18
#define SERVO_PIN  19
#define BTN_SET    25
#define BTN_UP     26
#define BTN_DOWN   27

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo dispenserServo;
Preferences prefs;
WiFiClientSecure tg;

int servoPositions[3] = {20, 90, 160};
int currentServoIndex = 0;

const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

struct DoseTime { int hour; int minute; };
DoseTime doses[3] = {{8, 0}, {12, 25}, {20, 0}};

bool doseDone[3] = {false, false, false};
int slotsLeft = 3;
int lastTriggeredDose = -1;
int lastTriggeredYDay = -1;
int storedDay = -1;
String currentTimeStr = "--:--";
String nextDoseStr = "--:--";

bool settingMode = false;
int settingStep = 0;

bool lastSetState = HIGH, lastUpState = HIGH, lastDownState = HIGH;
unsigned long lastBtnTime = 0;
const unsigned long debounce = 250;

bool alarmActive = false;
unsigned long alarmStartMs = 0;
bool alarmState = false;
unsigned long alarmLastToggle = 0;

unsigned long lastBtnRead = 0;
unsigned long lastScheduleCheck = 0;

String fmt12(int h, int m) {
  String ampm = (h >= 12) ? "PM" : "AM";
  int h12 = h % 12; if (h12 == 0) h12 = 12;
  char buf[12]; snprintf(buf, sizeof(buf), "%02d:%02d %s", h12, m, ampm.c_str());
  return String(buf);
}

String fmt24(int h, int m) {
  char buf[6]; snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
  return String(buf);
}

bool getTimeSafe(struct tm *t) { return getLocalTime(t); }

void saveDoses() {
  prefs.begin("med", false);
  prefs.putInt("d1h", doses[0].hour); prefs.putInt("d1m", doses[0].minute);
  prefs.putInt("d2h", doses[1].hour); prefs.putInt("d2m", doses[1].minute);
  prefs.putInt("d3h", doses[2].hour); prefs.putInt("d3m", doses[2].minute);
  prefs.end();
}

void loadDoses() {
  prefs.begin("med", true);
  doses[0].hour = prefs.getInt("d1h", 8);   doses[0].minute = prefs.getInt("d1m", 0);
  doses[1].hour = prefs.getInt("d2h", 12);  doses[1].minute = prefs.getInt("d2m", 25);
  doses[2].hour = prefs.getInt("d3h", 20);  doses[2].minute = prefs.getInt("d3m", 0);
  prefs.end();
}

String urlEncode(String s) {
  String out;
  for (int i = 0; i < s.length(); i++) {
    char c = s.charAt(i);
    if (isAlphaNumeric(c) || c == '-' || c == '_' || c == '.' || c == '~') out += c;
    else { out += '%'; out += String(c, HEX); }
  }
  return out;
}

void sendTelegram(String msg) {
  Serial.print("Telegram: ");
  Serial.println(msg);
  tg.setInsecure();
  if (!tg.connect("api.telegram.org", 443)) { Serial.println("Connect failed"); return; }
  String body = "chat_id=" + String(TG_CHAT_ID) + "&text=" + urlEncode(msg);
  tg.println("POST /bot" + String(TG_TOKEN) + "/sendMessage HTTP/1.1");
  tg.println("Host: api.telegram.org");
  tg.println("Content-Type: application/x-www-form-urlencoded");
  tg.print("Content-Length: "); tg.println(body.length());
  tg.println(); tg.println(body);
  while (tg.connected()) { String l = tg.readStringUntil('\n'); if (l == "\r") break; }
  Serial.println(tg.readString());
  tg.stop();
}

String computeNextDose() {
  struct tm t;
  if (!getTimeSafe(&t)) return "--:--";
  int nowMin = t.tm_hour * 60 + t.tm_min;
  for (int i = 0; i < 3; i++) {
    if (!doseDone[i]) {
      int dm = doses[i].hour * 60 + doses[i].minute;
      if (dm >= nowMin) return fmt12(doses[i].hour, doses[i].minute);
    }
  }
  return "Tomorrow";
}

void showLCD(String l1, String l2) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(l1.substring(0, 16));
  lcd.setCursor(0, 1); lcd.print(l2.substring(0, 16));
}

void showSettingScreen() {
  char title[17], val[17];
  int h = doses[settingStep / 2].hour;
  int m = doses[settingStep / 2].minute;
  if (settingStep % 2 == 0) snprintf(title, sizeof(title), "D%d Hour", settingStep / 2 + 1);
  else snprintf(title, sizeof(title), "D%d Minute", settingStep / 2 + 1);
  snprintf(val, sizeof(val), "%02d:%02d", h, m);
  showLCD(title, val);
}

void beepBuzzer() {
  alarmActive = true; alarmStartMs = millis(); alarmState = false; alarmLastToggle = 0;
}

void moveToNextSlot() {
  currentServoIndex = (currentServoIndex + 1) % 3;
  dispenserServo.write(servoPositions[currentServoIndex]);
  delay(700);
}

void resetDayIfNeeded(struct tm &t) {
  if (storedDay == -1) { storedDay = t.tm_mday; return; }
  if (t.tm_mday != storedDay) {
    storedDay = t.tm_mday;
    for (int i = 0; i < 3; i++) doseDone[i] = false;
    slotsLeft = 3; lastTriggeredDose = -1; lastTriggeredYDay = -1;
  }
}

void dispenseDose(int idx, struct tm &t) {
  doseDone[idx] = true; slotsLeft--; if (slotsLeft < 0) slotsLeft = 0;
  showLCD("Take Medicine", String(slotsLeft) + " slots left");
  beepBuzzer(); moveToNextSlot();
  String msg = "Dose " + String(idx + 1) + " at " + fmt12(t.tm_hour, t.tm_min) + ". " + String(slotsLeft) + " left";
  Serial.println(msg);
  sendTelegram("Medicine Alert - Dose " + String(idx + 1) + ": " + msg);
  delay(1000);
}

void handleAlarm() {
  if (!alarmActive) return;
  if (millis() - alarmStartMs >= 10000) { alarmActive = false; digitalWrite(BUZZER_PIN, LOW); return; }
  if (millis() - alarmLastToggle >= 200) { alarmLastToggle = millis(); alarmState = !alarmState; digitalWrite(BUZZER_PIN, alarmState); }
}

void processButtons() {
  if (millis() - lastBtnRead < 100) return;
  lastBtnRead = millis();
  bool s = digitalRead(BTN_SET), u = digitalRead(BTN_UP), d = digitalRead(BTN_DOWN);
  bool sPress = (s == LOW && lastSetState == HIGH);
  bool uPress = (u == LOW && lastUpState == HIGH);
  bool dPress = (d == LOW && lastDownState == HIGH);
  lastSetState = s; lastUpState = u; lastDownState = d;
  if (millis() - lastBtnTime < debounce) return;
  if (!sPress && !uPress && !dPress) return;
  lastBtnTime = millis();
  if (sPress) {
    if (!settingMode) { settingMode = true; settingStep = 0; showSettingScreen(); return; }
    settingStep++;
    if (settingStep > 5) { saveDoses(); settingMode = false; settingStep = 0; showLCD("Times Saved", "System Running"); delay(1000); }
    else showSettingScreen();
    return;
  }
  if (!settingMode) return;
  if (uPress) {
    int idx = settingStep / 2;
    if (settingStep % 2 == 0) doses[idx].hour = (doses[idx].hour + 1) % 24;
    else doses[idx].minute = (doses[idx].minute + 1) % 60;
    showSettingScreen();
  }
  if (dPress) {
    int idx = settingStep / 2;
    if (settingStep % 2 == 0) doses[idx].hour = (doses[idx].hour + 23) % 24;
    else doses[idx].minute = (doses[idx].minute + 59) % 60;
    showSettingScreen();
  }
}

void checkSchedule() {
  if (millis() - lastScheduleCheck < 1000) return;
  lastScheduleCheck = millis();
  if (settingMode) return;
  struct tm t;
  if (!getTimeSafe(&t)) { currentTimeStr = "No Time"; nextDoseStr = "--:--"; showLCD("Time sync fail", "Check WiFi"); return; }
  resetDayIfNeeded(t);
  currentTimeStr = fmt12(t.tm_hour, t.tm_min);
  nextDoseStr = computeNextDose();
  for (int i = 0; i < 3; i++) {
    if (!doseDone[i] && t.tm_hour == doses[i].hour && t.tm_min == doses[i].minute) {
      if (!(lastTriggeredDose == i && lastTriggeredYDay == t.tm_yday)) {
        lastTriggeredDose = i; lastTriggeredYDay = t.tm_yday;
        dispenseDose(i, t);
      }
    }
  }
  showLCD(currentTimeStr, "L:" + String(slotsLeft) + " N:" + nextDoseStr);
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT); digitalWrite(BUZZER_PIN, LOW);
  pinMode(BTN_SET, INPUT_PULLUP); pinMode(BTN_UP, INPUT_PULLUP); pinMode(BTN_DOWN, INPUT_PULLUP);
  Wire.begin(21, 22); lcd.init(); lcd.backlight();
  showLCD("Medicine System", "Starting...");
  loadDoses();
  dispenserServo.setPeriodHertz(50); dispenserServo.attach(SERVO_PIN, 500, 2400);
  dispenserServo.write(servoPositions[currentServoIndex]);
  showLCD("Connecting WiFi", "Please wait");
  WiFi.begin(ssid, pass);
  unsigned long ws = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - ws < 20000) { delay(500); Serial.print("."); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: "); Serial.println(WiFi.localIP());
    showLCD("WiFi OK", WiFi.localIP().toString());
    configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
    sendTelegram("Medicine Dispenser Online - System started. Doses: " + fmt24(doses[0].hour, doses[0].minute) + ", " + fmt24(doses[1].hour, doses[1].minute) + ", " + fmt24(doses[2].hour, doses[2].minute));
  } else showLCD("WiFi Failed", "Check SSID");
  delay(2000);
}

void loop() {
  processButtons();
  checkSchedule();
  handleAlarm();
}
