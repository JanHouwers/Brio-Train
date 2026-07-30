/*
 * Brio RC Locomotive — handheld remote (transmitter)             rev F (2026-07-28)
 * Board: Seeed XIAO ESP32-C5  (PlatformIO: env xiao_esp32c5, esp32 core 3.3.x)
 *
 * Power: 1S XTAR 26650 on the XIAO BAT pads.
 * Input: KY-023 joystick — Y axis = throttle (fwd/rev), push = horn.
 * Link:  ESP-NOW broadcast @ 25 Hz, MAGIC key, fixed channel, 2.4 GHz only.
 * Sleep: deep sleep after IDLE_SLEEP_MS of centered stick; wake = joystick press.
 *
 * Change versus the previous version: the BATTERY MEASUREMENT only. See the
 * "battery measurement" block below and notes 3/4 on hardware/wiring-remote.svg.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <HWCDC.h>

// ---------- pins ----------
// Silkscreen labels instead of bare GPIO numbers: the wiring (wire list rev F)
// goes by label, and behind D0..D4 the C5 has completely different GPIOs than
// the S3.
//   D0=GPIO1  D1=GPIO0  D2=GPIO25* D3=GPIO7*  D4=GPIO23 D5=GPIO24
//   D6=GPIO11 D7=GPIO12 D8=GPIO8   D9=GPIO9   D10=GPIO10      (* = strapping)
//
// rev F: the LEDs moved from D2/D3 to D9/D10. GPIO25 and GPIO7 are strapping
// pins on the C5 (together with GPIO2, GPIO27, GPIO28). An LED with series
// resistor parks such a pin around 1.9 V at reset — exactly in the region where
// high or low is undefined. D9/D10 are ordinary GPIOs.
#define PIN_JOY_Y    D0   // GPIO1,  ADC1_CH0 <- VRy
#define PIN_JOY_SW   D1   // GPIO0,  LP-GPIO (ext1 wake) <- SW (active low, pull-up)
#define PIN_LED_LINK D9   // GPIO9  -> green LED
#define PIN_LED_BATT D10  // GPIO10 -> red LED

// ---------- battery measurement ----------
// The ESP32-C5 has exactly ONE ADC unit: ADC1, six channels, on GPIO1..GPIO6.
// There is no ADC2 — the Wi-Fi/ADC2 conflict of the S3 does not exist on this
// board. D4 is GPIO23 here and has no ADC at all, so the external divider
// R3/R4 from rev D cannot work here.
// The XIAO C5 has the divider on board: BAT+ -> load switch (GPIO26) ->
// 100k/100k -> GPIO6 (ADC1_CH5). Both pins are defined as BAT_VOLT_PIN /
// BAT_VOLT_PIN_EN in variants/XIAO_ESP32C5/pins_arduino.h.
#define PIN_VSENSE     BAT_VOLT_PIN     // GPIO6
#define PIN_VSENSE_EN  BAT_VOLT_PIN_EN  // GPIO26, HIGH = enable the divider

// Two equal 100k resistors, so 1:2. CONFIRMED, not guessed.
#define VDIV_RATIO     2.0f
// Trim factor. The two 100k are typically +-1%, so the ratio can be off by up
// to ~2% (at 4.2 V that is ~84 mV). Measure BAT+ with a multimeter, compare
// with the line printed below and set VDIV_CAL = (multimeter / printed).
// Leave at 1.000f until you have measured.
#define VDIV_CAL       1.000f
// Averaging is not a luxury here: the divider has 100k||100k = 50k source
// impedance while the SAR ADC wants to see below 10k, and GPIO6 is not on the
// header so you cannot add a capacitor. On top of that, transmitting puts
// spikes on the reading. Seeed recommends 16x averaging in their own example.
#define VSENSE_SAMPLES 16

// ---------- tuning ----------
#define WIFI_CHANNEL   1
#define MAGIC          0xB210C0DEUL   // must match loco
#define SEND_MS        40             // 25 Hz
#define DEADBAND       80             // of ±1000
#define LOW_BATT_MV    3300
#define IDLE_SLEEP_MS  (5UL * 60UL * 1000UL)

typedef struct __attribute__((packed)) {
  uint32_t magic;
  int16_t  speed;
  uint8_t  flags;
  uint8_t  seq;
} Packet;

typedef struct __attribute__((packed)) {  // telemetry ack from loco
  uint32_t magic;
  uint16_t packMv;
  uint8_t  seq;
} Ack;

const uint8_t BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
volatile uint32_t lastAckMs  = 0;
volatile uint16_t locoPackMv = 8000;
int joyCenter = 2048;
uint8_t seq = 0;
uint32_t lastActive = 0;
bool usbBench = false;   // USB host seen at boot -> skip deep sleep

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len);
int16_t readThrottle();
uint32_t cellMv();

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(Ack)) return;
  Ack a;
  memcpy(&a, data, sizeof(a));
  if (a.magic != MAGIC) return;
  lastAckMs  = millis();
  locoPackMv = a.packMv;
}

int16_t readThrottle() {
  long a = 0;
  for (int i = 0; i < 4; i++) a += analogRead(PIN_JOY_Y);
  a /= 4;
  long v = ((a - joyCenter) * 1000L) / 2048L;    // approx -1000..1000
  v = constrain(v, -1000L, 1000L);
  if (abs(v) < DEADBAND) v = 0;
  return (int16_t)v;
}

// Cell voltage in mV, averaged over VSENSE_SAMPLES readings.
uint32_t cellMv() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < VSENSE_SAMPLES; i++) sum += analogReadMilliVolts(PIN_VSENSE);
  float pin = (float)sum / VSENSE_SAMPLES;      // voltage AT GPIO6
  return (uint32_t)(pin * VDIV_RATIO * VDIV_CAL);
}

void setup() {
  Serial.begin(115200);   // needed to calibrate the battery measurement

  pinMode(PIN_JOY_SW, INPUT_PULLUP);
  pinMode(PIN_LED_LINK, OUTPUT);
  pinMode(PIN_LED_BATT, OUTPUT);
  // After waking from deep sleep the pad is still clamped by gpio_hold_en();
  // release it first, otherwise digitalWrite() has no effect.
  gpio_hold_dis((gpio_num_t)PIN_VSENSE_EN);
  pinMode(PIN_VSENSE_EN, OUTPUT);
  digitalWrite(PIN_VSENSE_EN, HIGH);   // enable the cell divider
  analogSetPinAttenuation(PIN_JOY_Y, ADC_11db);
  analogSetPinAttenuation(PIN_VSENSE, ADC_11db);
  delay(5);                            // let the load switch + 50k/RC settle

  // calibrate stick center at boot (stick must be released!)
  long a = 0;
  for (int i = 0; i < 32; i++) { a += analogRead(PIN_JOY_Y); delay(5); }
  joyCenter = a / 32;

  WiFi.mode(WIFI_STA);
  // Modem sleep off. The remote is not only a transmitter: it receives the
  // telemetry ack from the loco, which drives the green link LED. Without this
  // line the radio may switch off between beacons, you miss acks and the link
  // LED blinks while the connection is perfectly fine. The loco already has it.
  // Costs current while running, not in deep sleep — the radio is off there.
  WiFi.setSleep(false);
  // Both ends are now a C5 and therefore dual-band. Lock explicitly to 2.4 GHz,
  // otherwise channel 1 can end up on a different band on either side.
  // 2.4 GHz also penetrates wood and PETG better.
  esp_wifi_set_band_mode(WIFI_BAND_MODE_2G_ONLY);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_now_init();
  esp_now_register_recv_cb(onRecv);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BCAST, 6);
  peer.channel = WIFI_CHANNEL;
  esp_now_add_peer(&peer);

  // Detect a USB host and LATCH it. A continuous check in loop() does not work,
  // this was tried: isPlugged() looks at SOF packets, and as soon as nothing has
  // the COM port open the host puts the device into selective suspend after a
  // few minutes. The SOFs then stop and the check reports "unplugged" while the
  // cable is still in — the board fell asleep anyway and took the COM port with
  // it. Right after boot the host is guaranteed to be active.
  for (int i = 0; i < 40 && !usbBench; i++) { usbBench = HWCDC::isPlugged(); delay(25); }
  Serial.printf("boot: usb=%d (deep sleep %s)\n", usbBench, usbBench ? "off" : "on");

  lastActive = millis();
}

void loop() {
  static uint32_t lastSend = 0, lastVbat = 0;
  uint32_t now = millis();
  if (now - lastSend < SEND_MS) return;
  lastSend = now;

  Packet p;
  p.magic = MAGIC;
  p.speed = readThrottle();
  p.flags = (digitalRead(PIN_JOY_SW) == LOW) ? 0x01 : 0x00;  // horn
  p.seq   = seq++;
  esp_now_send(BCAST, (uint8_t *)&p, sizeof(p));

  // link LED: telemetry ack from loco seen recently
  digitalWrite(PIN_LED_LINK, (now - lastAckMs) < 500);

  // battery LED: solid = remote cell low, blink = loco pack low
  if (now - lastVbat > 2000) {
    lastVbat = now;
    uint32_t mv = cellMv();
    if (mv < LOW_BATT_MV)            digitalWrite(PIN_LED_BATT, HIGH);
    else if (locoPackMv < 6400)      digitalWrite(PIN_LED_BATT, !digitalRead(PIN_LED_BATT));
    else                             digitalWrite(PIN_LED_BATT, LOW);

    // CALIBRATION LINE. GPIO6 is not on the header, so this is the only way to
    // check the battery measurement: put a multimeter on BAT+ and compare, at
    // the same moment, with the value below. This may be done with USB plugged
    // in: multimeter and divider measure the same node, so the fact that the
    // SGM40567 is charging the cell at that moment makes no difference to the
    // RATIO. What does apply: the absolute value is then the charge voltage,
    // not the resting voltage of the cell. So the red LED never comes on with
    // USB plugged in.
    Serial.printf("cell %lu mV (pin %lu mV, ratio %.3f x cal %.3f) | loco %u mV\n",
                  (unsigned long)mv,
                  (unsigned long)(mv / (VDIV_RATIO * VDIV_CAL)),
                  VDIV_RATIO, VDIV_CAL, locoPackMv);
  }

  // idle -> deep sleep, wake on joystick press.
  // usbBench is latched at boot (see setup): if the XIAO hangs off a host it
  // does not sleep, because otherwise the COM port would disappear every
  // IDLE_SLEEP_MS during development and the board would have to be woken
  // physically before it can be flashed. On battery (no host at boot) it behaves
  // like a normal remote.
  if (p.speed != 0 || p.flags || usbBench) lastActive = now;
  if (now - lastActive > IDLE_SLEEP_MS) {
    digitalWrite(PIN_LED_LINK, LOW);
    digitalWrite(PIN_LED_BATT, LOW);
    // Pinch off the on-board cell divider during sleep: saves the 21 uA the
    // 100k/100k otherwise draws permanently. Without hold the pad falls back to
    // high-Z in deep sleep and the enable input floats — worse than leaving it
    // high. The C5 has SOC_GPIO_SUPPORT_HOLD_SINGLE_IO_IN_DSLP, so gpio_hold_en()
    // is enough; the separate gpio_deep_sleep_hold_en() does not exist here.
    digitalWrite(PIN_VSENSE_EN, LOW);
    gpio_hold_en((gpio_num_t)PIN_VSENSE_EN);
    // ext0 does not exist on the C5 (RISC-V). ext1 with per-pin level does the
    // same; D1 = GPIO0 falls within the LP-GPIOs (LP_GPIO0..LP_GPIO6), so wake
    // on press works. GPIO0 is not a strapping pin on the C5 (those are
    // GPIO27/GPIO28), so a pressed joystick during reset does not put the board
    // into download mode.
    esp_sleep_enable_ext1_wakeup(1ULL << PIN_JOY_SW, ESP_EXT1_WAKEUP_ANY_LOW); // press = LOW
    esp_deep_sleep_start();
  }
}
