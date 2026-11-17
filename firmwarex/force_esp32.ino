// Push-up Coach v2 - ESP32 firmware (BLE)
// Packet (18 bytes, little-endian):
// 4 x [sag int16, lat int16] (deg*100), dist uint16 (mm, 0 = no echo)
// Order: 0=R upper arm (ch2), 1=R wrist (ch3), 2=L upper arm (ch4), 3=L wrist (ch5)

#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHAR_UUID    "12345678-1234-1234-1234-1234567890ac"

#define TCA_ADDR 0x70
#define MPU_ADDR 0x68
#define TCA_FIRST_CH 2

#define SDA_PIN 22
#define SCL_PIN 21

#define USE_ULTRASONIC 1   // 1 = chest sensor on, 0 = off
#define TRIG 5
#define ECHO 18

BLECharacteristic *pCh;
bool connected = false;

float ax[4], ay[4], az[4];
bool seeded[4] = {false, false, false, false};

uint16_t lastDist = 0;

class SrvCb : public BLEServerCallbacks {
  void onConnect(BLEServer *server) {
    connected = true;
    Serial.println("BLE client connected");
  }

  void onDisconnect(BLEServer *server) {
    connected = false;
    Serial.println("BLE client disconnected");

    delay(300);
    BLEDevice::startAdvertising();

    Serial.println("BLE advertising restarted");
  }
};

void tcaSelect(uint8_t ch) {
  Wire.beginTransmission(TCA_ADDR);
  Wire.write(1 << ch);
  Wire.endTransmission();
}

void mpuInit() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission();
}

bool readAccel(float &x, float &y, float &z) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);

  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)6) != 6)
    return false;

  // High byte first, then low byte (separate statements = guaranteed order)
  int16_t rx = Wire.read() << 8; rx |= Wire.read();
  int16_t ry = Wire.read() << 8; ry |= Wire.read();
  int16_t rz = Wire.read() << 8; rz |= Wire.read();

  x = rx / 16384.0;
  y = ry / 16384.0;
  z = rz / 16384.0;

  return true;
}

uint16_t readDist() {
#if USE_ULTRASONIC
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG, LOW);

  unsigned long d = pulseIn(ECHO, HIGH, 25000);

  lastDist = d ? (uint16_t)(d * 0.343 / 2.0) : 0;   // 0 = no echo (too close or too far)

  return lastDist;
#else
  return 0;
#endif
}

void i2cRecover() {            // frees a stuck I2C bus and wakes the MPUs again
  Wire.end();
  pinMode(SCL_PIN, OUTPUT);
  for (int i = 0; i < 9; i++) {
    digitalWrite(SCL_PIN, HIGH); delayMicroseconds(5);
    digitalWrite(SCL_PIN, LOW);  delayMicroseconds(5);
  }
  digitalWrite(SCL_PIN, HIGH);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  for (int i = 0; i < 4; i++) { tcaSelect(TCA_FIRST_CH + i); mpuInit(); }
}

void put16(uint8_t *b, int i, int16_t v) {
  b[i] = v & 0xFF;
  b[i + 1] = (v >> 8) & 0xFF;
}

void setup() {
  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);   // SDA = GPIO22, SCL = GPIO21
  Wire.setClock(400000);

#if USE_ULTRASONIC
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
#endif

  for (int i = 0; i < 4; i++) {
    tcaSelect(TCA_FIRST_CH + i);
    mpuInit();
  }

  // Initialize BLE
  BLEDevice::init("PushupCoach");

  BLEServer *srv = BLEDevice::createServer();
  srv->setCallbacks(new SrvCb());

  BLEService *svc = srv->createService(SERVICE_UUID);

  pCh = svc->createCharacteristic(
    CHAR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY |
    BLECharacteristic::PROPERTY_READ
  );

  pCh->addDescriptor(new BLE2902());

  uint8_t initialData[18] = {0};
  pCh->setValue(initialData, sizeof(initialData));

  svc->start();

  // Configure advertising
  BLEAdvertising *advertising = BLEDevice::getAdvertising();

  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);   // helps phones connect reliably
  advertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.println("========================");
  Serial.println("PushupCoach BLE READY");
  Serial.println("Name: PushupCoach");
  Serial.println("Advertising started");
  Serial.println("========================");
}

void loop() {
  static uint32_t t0 = 0;
  static uint32_t lastWake = 0;
  static int failCount = 0;

  if (millis() - t0 < 50) return;
  t0 = millis();

  if (millis() - lastWake > 1000) {   // wake the MPUs every second in case they fell asleep
    lastWake = millis();
    for (int i = 0; i < 4; i++) { tcaSelect(TCA_FIRST_CH + i); mpuInit(); }
  }

  uint8_t buf[18];
  int failed = 0;

  for (int i = 0; i < 4; i++) {
    tcaSelect(TCA_FIRST_CH + i);

    float x, y, z;

    if (readAccel(x, y, z)) {
      if (!seeded[i]) {
        ax[i] = x;
        ay[i] = y;
        az[i] = z;
        seeded[i] = true;
      }

      ax[i] += 0.3 * (x - ax[i]);
      ay[i] += 0.3 * (y - ay[i]);
      az[i] += 0.3 * (z - az[i]);
    } else {
      failed++;
    }

    float sag = atan2(ay[i], -ax[i]) * 57.2958;
    float lat = atan2(az[i], -ax[i]) * 57.2958;

    put16(buf, i * 4, (int16_t)(sag * 100));
    put16(buf, i * 4 + 2, (int16_t)(lat * 100));
  }

  failCount = (failed == 4) ? failCount + 1 : 0;
  if (failCount >= 20) { i2cRecover(); failCount = 0; }   // all four failing for about 1 s

  uint16_t d = readDist();

  buf[16] = d & 0xFF;
  buf[17] = d >> 8;

  if (connected) {
    pCh->setValue(buf, 18);
    pCh->notify();
  }
}
