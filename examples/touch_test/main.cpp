#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>

#define TOUCH_SDA     5
#define TOUCH_SCL     6
#define TOUCH_INT     7
#define TOUCH_RST     41
#define FT6336U_ADDR  0x38

TFT_eSPI tft = TFT_eSPI();
static uint16_t colorWheel = 0;

static bool readTouchPoint(int16_t *x, int16_t *y) {
  Wire.beginTransmission(FT6336U_ADDR);
  Wire.write(0x02);
  if (Wire.endTransmission() == 0) {
    Wire.requestFrom((uint16_t)FT6336U_ADDR, (uint8_t)5, true);
    if (Wire.available() >= 5) {
      uint8_t touches = Wire.read() & 0x0F;
      uint8_t x_high = Wire.read();
      uint8_t x_low = Wire.read();
      uint8_t y_high = Wire.read();
      uint8_t y_low = Wire.read();

      if (touches > 0 && x && y) {
        int16_t raw_x = ((x_high & 0x0F) << 8) | x_low;
        int16_t raw_y = ((y_high & 0x0F) << 8) | y_low;
        *x = constrain(raw_x, 0, tft.width() - 1);
        *y = constrain(raw_y, 0, tft.height() - 1);
        return true;
      }
    }
  }
  return false;
}

static uint16_t nextColor() {
  colorWheel += 97;
  uint8_t r = (uint8_t)(40 + (colorWheel * 3u) % 180u);
  uint8_t g = (uint8_t)(40 + (colorWheel * 5u) % 180u);
  uint8_t b = (uint8_t)(40 + (colorWheel * 7u) % 180u);
  return tft.color565(r, g, b);
}

static void drawTouchCircle(int16_t x, int16_t y) {
  tft.drawCircle(x, y, 10, nextColor());
  tft.drawCircle(x, y, 11, TFT_WHITE);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nStarting touch circle test...");

  // Power up VExt (GPIO 36)
  pinMode(36, OUTPUT);
  digitalWrite(36, HIGH);
  delay(150);

  // Initialize Touch Hardware
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);
  delay(10);
  digitalWrite(TOUCH_RST, HIGH);
  delay(50);

  pinMode(TOUCH_INT, INPUT);
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  Wire.setClock(400000);

  // PWM for Backlight
  ledcSetup(0, 5000, 8);
  ledcAttachPin(21, 0);
  ledcWrite(0, 160);
  Serial.println("Backlight ON (PWM)");

  // Initialize TFT
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Touch test: tap screen", 10, 10, 2);
  tft.drawString("Draws circles at touch", 10, 30, 2);
  Serial.println("TFT init OK (Rotation 0)");
}

void loop() {
  int16_t x = 0;
  int16_t y = 0;
  if (readTouchPoint(&x, &y)) {
    drawTouchCircle(x, y);
    Serial.printf("Touch: x=%d y=%d\n", x, y);
    delay(25);
  }

  delay(5);
}
