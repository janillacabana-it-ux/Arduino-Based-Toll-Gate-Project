#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define SS_PIN 10
#define RST_PIN 9

MFRC522 rfid(SS_PIN, RST_PIN);
Servo gateServo;
LiquidCrystal_I2C lcd(0x27,16,2);

#define buzzer 6
#define servoPin 5
#define trigPin 8
#define echoPin 4

// Registered Users
byte janilla[] = {0x77, 0x0A, 0x32, 0x25};
byte jade[] = {0x9A, 0xBC, 0xDE, 0xF0};

String userNames[] = {"Janilla", "Jade"};

bool registrationMode = false;

// ================= SETUP =================
void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  pinMode(buzzer, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  gateServo.attach(servoPin);
  gateServo.write(0);

  lcd.init();
  lcd.backlight();

  showIdle();

  Serial.println("Type REG to enter registration mode");
  Serial.println("Type EXIT to return to normal mode");
}

// ================= LOOP =================
void loop() {

  // SERIAL COMMANDS
  if (Serial.available()) {
    String cmd = Serial.readString();
    cmd.trim();

    if (cmd == "REG" || cmd == "reg") {
      registrationMode = true;

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("REG MODE");
      lcd.setCursor(0,1);
      lcd.print("Scan RFID");
      delay(2000);

      showIdle();
    }

    if (cmd == "EXIT" || cmd == "exit") {
      registrationMode = false;
      showIdle();
    }
  }

  // WAIT FOR CARD
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  // ================= REGISTRATION MODE =================
  if (registrationMode) {

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Reading UID...");

    Serial.println("----- NEW RFID TAG -----");

    Serial.print("UID: {0x");
    for (byte i = 0; i < rfid.uid.size; i++) {
      Serial.print(rfid.uid.uidByte[i], HEX);
      if (i < rfid.uid.size - 1) Serial.print(", 0x");
    }
    Serial.println("}");

    lcd.setCursor(0,1);
    lcd.print("Check Serial");

    beep(300);
    delay(2000);

    showIdle();
    rfid.PICC_HaltA();
    return;
  }

  // ================= NORMAL MODE =================

  // SCANNING
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Scanning...");
  lcd.setCursor(0,1);
  lcd.print("Please wait");

  int user = getAuthorizedUser();

  // ✅ ACCESS GRANTED
  if (user >= 0) {

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Access Granted");
    lcd.setCursor(0,1);
    lcd.print(userNames[user]);
    delay(1500);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Toll Paid");
    lcd.setCursor(0,1);
    lcd.print("Thank You!");
    delay(1500);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Gate Opening");
    lcd.setCursor(0,1);
    lcd.print("Please proceed");

    beep(300);
    openGate();
  }

  // ❌ ACCESS DENIED
  else {

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Access Denied");
    lcd.setCursor(0,1);
    lcd.print("Try Again");

    Serial.print("Unknown UID: {0x");
    for (byte i = 0; i < rfid.uid.size; i++) {
      Serial.print(rfid.uid.uidByte[i], HEX);
      if (i < rfid.uid.size - 1) Serial.print(", 0x");
    }
    Serial.println("}");

    beep(1500);
    delay(1500);

    showIdle();
  }

  rfid.PICC_HaltA();
}

// ================= FUNCTIONS =================

// IDLE
void showIdle() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Welcome to SCRB");
  lcd.setCursor(0,1);

  if (registrationMode) {
    lcd.print("REG Mode Active");
  } else {
    lcd.print("Scan your ID");
  }
}

// CHECK USERS
int getAuthorizedUser() {
  if (compareUID(rfid.uid.uidByte, janilla)) return 0;
  if (compareUID(rfid.uid.uidByte, jade)) return 1;
  return -1;
}

// COMPARE UID
boolean compareUID(byte *tag, byte *known) {
  for (byte i = 0; i < 4; i++) {
    if (tag[i] != known[i]) return false;
  }
  return true;
}

// BUZZER
void beep(int duration) {
  digitalWrite(buzzer, HIGH);
  delay(duration);
  digitalWrite(buzzer, LOW);
}

// ================= GATE =================
void openGate() {

  gateServo.write(90);
  delay(1500);

  while (true) {
    long d = readDistance();

    lcd.setCursor(0,1);
    lcd.print("Waiting Car...");

    if (d > 0 && d < 20) break;
  }

  while (true) {
    long d = readDistance();

    if (d > 0 && d < 20) {
      lcd.setCursor(0,0);
      lcd.print("Vehicle Detected");
      lcd.setCursor(0,1);
      lcd.print("Passing...     ");
    } else {

      for (int i = 3; i > 0; i--) {
        lcd.setCursor(0,0);
        lcd.print("Gate Closing   ");
        lcd.setCursor(0,1);
        lcd.print("In ");
        lcd.print(i);
        lcd.print(" sec   ");
        beep(100);
        delay(1000);
      }

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Closing Gate");
      lcd.setCursor(0,1);
      lcd.print("Please wait");
      delay(1000);

      break;
    }
  }

  gateServo.write(0);
  delay(1500);

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Thank You!");
  lcd.setCursor(0,1);
  lcd.print("Drive Safely");
  delay(2000);

  showIdle();
}

// ================= ULTRASONIC =================
long readDistance() {

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);

  if (duration == 0) return 999;

  return duration * 0.034 / 2;
}
