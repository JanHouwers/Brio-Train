/*
 * Brio RC Locomotive — receiver / drive controller               rev D (2026-07-28)
 * Board: Seeed XIAO ESP32-C5  (PlatformIO: env xiao_esp32c5, core 3.3.x)
 *
 * Power: 2S (2x XTAR 14500, protected), 6.0–8.4 V, through an adjustable buck
 *        set to 5.0 V into the 5V pin of the XIAO.
 * Motor: N20 6 V via DRV8833 channel A, PWM capped at MAX_DUTY
 * Link:  ESP-NOW broadcast, filtered on MAGIC, channel 1, locked to 2.4 GHz
 *
 * ---------------------------------------------------------------------------
 * WHY THE PINOUT DIFFERS FROM THE S3 (rev C)
 *
 * The ESP32-C5 has exactly ONE ADC unit: ADC1, six channels, GPIO1..GPIO6.
 * There is no ADC2 — the Wi-Fi/ADC2 conflict of the S3 does not exist here.
 * Of the XIAO C5 header pins, only D0 (GPIO1) is on that ADC. The pack voltage
 * divider therefore MUST go to D0, which pushes the motor control elsewhere.
 * That is harmless: AIN1/AIN2 of the DRV8833 are digital inputs (speed = duty
 * cycle of a square wave), as are nSLEEP and nFAULT. So the pack voltage is
 * the only analog measurement in the entire locomotive.
 *
 * Strapping pins on the C5 are GPIO2, GPIO7, GPIO25, GPIO27 and GPIO28.
 * On the header those are D3 (GPIO7) and D2 (GPIO25): leave them EMPTY. Two
 * signals here carry an external resistor (R5 pull-up on nFAULT, R6 pulldown
 * on nSLEEP) and that would drag a strapping pin to the wrong level at reset.
 * D6/D7 (GPIO11/GPIO12) are the UART: keep free for the boot log and debug.
 * ---------------------------------------------------------------------------
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ---------- pins (XIAO C5 silkscreen -> GPIO) ----------
//   D0=GPIO1  D1=GPIO0  D2=GPIO25* D3=GPIO7*  D4=GPIO23 D5=GPIO24
//   D6=GPIO11 D7=GPIO12 D8=GPIO8   D9=GPIO9   D10=GPIO10      (* = strapping)
// The silkscreen on the black 6+6-pin DRV8833 module is mangled English:
//   EEP = nSLEEP   ULT = nFAULT   IN1/IN2 -> OUT1/OUT2 = channel A
#define PIN_VSENSE   D0   // GPIO1,  ADC1_CH0 <- divider 200k/100k (Vpack/3) + 100 nF
#define PIN_NSLEEP   D1   // GPIO0  -> module EEP  (NOT wired: EEP is tied to 3V3)
#define PIN_IN1      D4   // GPIO23 -> module IN1  (PWM)
#define PIN_IN2      D5   // GPIO24 -> module IN2  (PWM)
#define PIN_NFAULT   D8   // GPIO8  <- module ULT  (NOT wired: ULT left floating)
#define PIN_LED      D9   // GPIO9  -> headlight (47R -> LED -> SIG ground)
#define PIN_BUZZ     D10  // GPIO10 -> passive buzzer (100R)

// ---------------------------------------------------------------------------
// BUILD SWITCHES for the black DRV8833 module
//
// On this board VCC is the same net as VM, so 8.4 V. Solder bridge J1 connects
// EEP to VCC, and there is a 47k pull-up from ULT to VCC. As long as those are
// present you must NOT connect EEP or ULT to a 3.3 V GPIO.
//
// CURRENTLY 0/0: EEP and ULT are not connected to a GPIO in this build.
//
// EEP must then be held HIGH, otherwise the internal 500k pulldown drags it low
// and the driver stays asleep. Do NOT do that with J1: it connects EEP to
// VCC = VM = 8.4 V, while the logic pins of the DRV8833 have an absolute
// maximum of 7 V. Instead, run a wire from the 3V3 pin of the XIAO to EEP
// (L17 in the wire list).
//
// Set these to 1 only after removing J1 / the 47k and running the wire to D1
// respectively D8.
#define HAVE_NSLEEP  0    // 1 = EEP on D1, only after J1 is removed
#define HAVE_NFAULT  0    // 1 = ULT on D8, only after the 47k is removed
// ---------------------------------------------------------------------------

// The on-board cell divider of the XIAO (GPIO6 via load switch GPIO26) measures
// the BAT pads. On the locomotive NOTHING is connected there — the board is fed
// through the 5V pin. So we explicitly disable that divider, otherwise the
// enable input floats. GPIO26 is a strapping pin: driving it after boot is fine,
// hanging an external resistor on it is not (and cannot be done anyway, it is
// not on the header).
#define PIN_BATSENSE_EN  BAT_VOLT_PIN_EN   // GPIO26

// ---------- tuning ----------
#define WIFI_CHANNEL     1
#define MAGIC            0xB210C0DEUL  // must match the remote
#define FAILSAFE_MS      400
#define MAX_DUTY         75            // % of 8.4 V -> ~6.3 V on the N20. Never above 80.
#define SLEW_PER_LOOP    40            // acceleration ramp, units per 20 ms
#define PWM_FREQ         20000         // 20 kHz, inaudible
#define PWM_RES          10            // bits -> duty 0..1023

// ---------- pack voltage measurement ----------
// External divider R3 200k (top) / R4 100k (bottom) on VBAT_SW, tap to D0,
// with C1 100 nF from tap to SIG ground. Source impedance 200k||100k = 67 kOhm,
// well above the <10 kOhm the SAR ADC wants to see: C1 supplies the sampling
// charge locally and we average over 16 readings.
#define VDIV_RATIO       3.0f          // (200k + 100k) / 100k
// Trim factor: measure VBAT_SW with a multimeter, compare against the printed
// line and set VDIV_CAL = (multimeter / printed). Leave at 1.000f until measured.
#define VDIV_CAL         1.000f
#define VSENSE_SAMPLES   16

// Low-battery threshold with hysteresis. 6600 mV = 3.3 V/cell.
// The buck (MP1584EN set to 5.0 V) holds its output down to about 5.4 V input,
// so this threshold sits well above the brownout limit.
#define LOW_BATT_TRIP_MV   6600
#define LOW_BATT_CLEAR_MV  6900
#define LOW_BATT_STRIKES   3

// ---------- fault handling ----------
#define WAKE_US          2000UL        // tWAKE of the DRV8833 is max 1 ms
#define FAULT_DEBOUNCE   3
#define FAULT_RESET_MS   1000UL
#define FAULT_MAX        5
#define FAULT_WINDOW_MS  10000UL

typedef struct __attribute__((packed)) {
  uint32_t magic;
  int16_t  speed;   // -1000..1000
  uint8_t  flags;   // bit0 = horn
  uint8_t  seq;
} Packet;

typedef struct __attribute__((packed)) {  // telemetry back to the remote
  uint32_t magic;
  uint16_t packMv;
  uint8_t  seq;
} Ack;

volatile int16_t  targetSpeed = 0;
volatile uint8_t  rxFlags     = 0;
volatile uint32_t lastRxMs    = 0;
volatile uint8_t  lastSeq     = 0;
volatile bool     needAck     = false;
volatile uint8_t  remoteMac[6] = {0};
bool     peerAdded    = false;

int16_t currentSpeed  = 0;
bool    lowBatt       = false;
bool    driverAwake   = false;
bool    faultShutdown = false;

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len);
void driverSleep();
void driverWake();
void driveMotor(int16_t speed);
uint32_t packMv();

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len != sizeof(Packet)) return;
  Packet p;
  memcpy(&p, data, sizeof(p));
  if (p.magic != MAGIC) return;
  targetSpeed = constrain(p.speed, (int16_t)-1000, (int16_t)1000);
  rxFlags     = p.flags;
  lastSeq     = p.seq;
  for (int i = 0; i < 6; i++) remoteMac[i] = info->src_addr[i];
  lastRxMs    = millis();     // last: only after this is the rest valid
  needAck     = true;
}

// ---------- driver on/off ----------
void driverSleep() {
  ledcWrite(PIN_IN1, 0);          // both inputs low = coast
  ledcWrite(PIN_IN2, 0);
#if HAVE_NSLEEP
  digitalWrite(PIN_NSLEEP, LOW);  // outputs high-Z, logic reset
  driverAwake = false;
#endif
}

void driverWake() {
  if (driverAwake) return;
  ledcWrite(PIN_IN1, 0);          // both inputs low first...
  ledcWrite(PIN_IN2, 0);
#if HAVE_NSLEEP
  digitalWrite(PIN_NSLEEP, HIGH); // ...only then wake it up
  delayMicroseconds(WAKE_US);
#endif
  driverAwake = true;
}

void driveMotor(int16_t speed) {
  if (!driverAwake) return;
  // PWM on one input, the other low: the bridge alternates between drive and
  // coast. That is FAST decay — more current ripple than slow decay, but it
  // works with two GPIOs and is perfectly fine on an N20 at 20 kHz.
  const int maxDuty = (1023 * MAX_DUTY) / 100;
  int duty = (int)((long)abs(speed) * maxDuty / 1000);
  if (speed > 0)      { ledcWrite(PIN_IN2, 0); ledcWrite(PIN_IN1, duty); }
  else if (speed < 0) { ledcWrite(PIN_IN1, 0); ledcWrite(PIN_IN2, duty); }
  else                { ledcWrite(PIN_IN1, 0); ledcWrite(PIN_IN2, 0);    }
}

uint32_t packMv() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < VSENSE_SAMPLES; i++) sum += analogReadMilliVolts(PIN_VSENSE);
  float pin = (float)sum / VSENSE_SAMPLES;      // voltage AT D0
  return (uint32_t)(pin * VDIV_RATIO * VDIV_CAL);
}

void setup() {
  Serial.begin(115200);

  // 1) driver hard off before anything else — the motor must not twitch at boot.
  // Without an nSLEEP wire that is not possible, but it is also not needed:
  // IN1/IN2 are high-Z at boot and the DRV8833 has internal pulldowns on them,
  // so both low = coast.
#if HAVE_NSLEEP
  pinMode(PIN_NSLEEP, OUTPUT);
  digitalWrite(PIN_NSLEEP, LOW);
  driverAwake = false;
#else
  driverAwake = true;                  // EEP is tied to VCC
#endif
#if HAVE_NFAULT
  pinMode(PIN_NFAULT, INPUT_PULLUP);   // an external 10k to 3V3 is fine, this is the backup
#endif

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  // Disable the on-board cell divider of the XIAO: on the locomotive nothing is
  // connected to the BAT pads, so that reading is meaningless and the enable
  // must not float.
  pinMode(PIN_BATSENSE_EN, OUTPUT);
  digitalWrite(PIN_BATSENSE_EN, LOW);

  // The order of ledcAttach determines channel and timer assignment:
  // IN1 and IN2 share a timer at 20 kHz, the buzzer gets its own timer.
  // Do not reorder: otherwise the horn drags the motor frequency along.
  ledcAttach(PIN_IN1, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_IN2, PWM_FREQ, PWM_RES);
  ledcWrite(PIN_IN1, 0);
  ledcWrite(PIN_IN2, 0);
  ledcAttach(PIN_BUZZ, 2000, 10);
  ledcWrite(PIN_BUZZ, 0);

  analogSetPinAttenuation(PIN_VSENSE, ADC_11db);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);                // modem sleep off: otherwise you miss packets
  // Both ends are now a C5 and therefore dual-band. Lock explicitly to 2.4 GHz,
  // otherwise channel 1 can end up on a different band on either side and they
  // never see each other. 2.4 GHz also penetrates wood and PETG better.
  esp_wifi_set_band_mode(WIFI_BAND_MODE_2G_ONLY);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_now_init();
  esp_now_register_recv_cb(onRecv);
  // the driver stays asleep until a valid packet with the stick at neutral arrives
}

void loop() {
  static uint32_t lastLoop = 0, lastVbat = 0, blink = 0;
  static uint32_t faultSince = 0, faultWindowStart = 0;
  static uint8_t  faultLow = 0, faultCount = 0, battStrikes = 0;
  static bool     armed = false, hornOn = false;

  uint32_t now = millis();
  if (now - lastLoop < 20) return;      // 50 Hz control loop
  lastLoop = now;

  bool linkAlive = (now - lastRxMs <= FAILSAFE_MS);

  // ---------- battery check with hysteresis, every 2 s ----------
  if (now - lastVbat > 2000) {
    lastVbat = now;
    uint32_t mv = packMv();
    if (!lowBatt) {
      if (mv < LOW_BATT_TRIP_MV) { if (++battStrikes >= LOW_BATT_STRIKES) lowBatt = true; }
      else battStrikes = 0;
    } else {
      if (mv > LOW_BATT_CLEAR_MV) { lowBatt = false; battStrikes = 0; }
    }
    // CALIBRATION LINE: put a multimeter on VBAT_SW and compare with this value.
    Serial.printf("pack %lu mV (D0 %lu mV, ratio %.3f x cal %.3f) | link %d armed %d fault %d\n",
                  (unsigned long)mv,
                  (unsigned long)(mv / (VDIV_RATIO * VDIV_CAL)),
                  VDIV_RATIO, VDIV_CAL, linkAlive, armed, faultShutdown);
  }

  // ---------- nFAULT ----------
  // Only valid while the driver is awake: right after driverWake() the chip
  // needs its wake-up time and the line says nothing about the bridge state yet.
#if HAVE_NFAULT
  if (driverAwake && digitalRead(PIN_NFAULT) == LOW) {
    if (faultLow < 255) faultLow++;
  } else {
    faultLow = 0;
  }
#endif

  if (faultLow >= FAULT_DEBOUNCE && !faultShutdown) {
    faultLow     = 0;
    currentSpeed = 0;
    driveMotor(0);
    driverSleep();                       // nSLEEP low clears the fault latch
    faultSince   = now;
    if (now - faultWindowStart > FAULT_WINDOW_MS) { faultWindowStart = now; faultCount = 0; }
    if (++faultCount >= FAULT_MAX) faultShutdown = true;   // stays off until power cycle
  }

  // ---------- neutral lockout ----------
  if (!linkAlive)                 armed = false;
  else if (targetSpeed == 0)      armed = true;

  int16_t tgt = targetSpeed;
  if (!linkAlive || !armed || lowBatt || faultShutdown) tgt = 0;

  // ---------- driver awake or asleep ----------
  bool mayDrive = linkAlive && armed && !lowBatt && !faultShutdown &&
                  (now - faultSince > FAULT_RESET_MS);
  // The target must be zero during the recovery window after a fault as well.
  // Without this line the ramp counter simply keeps running with HAVE_NSLEEP=0
  // (the driver is always awake then) and after that window the locomotive jumps
  // back to the last requested speed in a single loop.
  if (!mayDrive) tgt = 0;
  if (mayDrive)                              driverWake();
  else if (driverAwake && currentSpeed == 0) driverSleep();

  // ---------- slew rate limiting ----------
  // While the driver sleeps we cannot steer anything; the ramp counter is held
  // at zero. Otherwise it would run on during a fault towards the last requested
  // speed and the locomotive would jump to full duty in one loop on recovery —
  // exactly without the ramp that protects it.
  if (!driverAwake) {
    currentSpeed = 0;
  } else {
    if      (currentSpeed < tgt) currentSpeed = min((int)tgt, currentSpeed + SLEW_PER_LOOP);
    else if (currentSpeed > tgt) currentSpeed = max((int)tgt, currentSpeed - SLEW_PER_LOOP);
  }
  driveMotor(currentSpeed);

  // ---------- headlight ----------
  // fast blink = fault · slow blink = battery low · otherwise on while driving
  uint32_t blinkMs = faultShutdown ? 100 : (lowBatt ? 300 : 0);
  if (blinkMs) {
    if (now - blink > blinkMs) { blink = now; digitalWrite(PIN_LED, !digitalRead(PIN_LED)); }
  } else {
    digitalWrite(PIN_LED, currentSpeed != 0);
  }

  // ---------- horn ----------
  // ledcWriteTone() reconfigures the timer, so only call it on a change.
  bool wantHorn = linkAlive && (rxFlags & 0x01);
  if (wantHorn != hornOn) {
    hornOn = wantHorn;
    ledcWriteTone(PIN_BUZZ, hornOn ? 660 : 0);
  }

  // ---------- telemetry back to the remote ----------
  static uint32_t lastAck = 0;
  if (needAck && now - lastAck > 100) {
    lastAck = now;
    needAck = false;
    uint8_t mac[6];
    for (int i = 0; i < 6; i++) mac[i] = remoteMac[i];
    if (!peerAdded) {
      esp_now_peer_info_t peer = {};
      memcpy(peer.peer_addr, mac, 6);
      peer.channel = WIFI_CHANNEL;
      if (esp_now_add_peer(&peer) == ESP_OK) peerAdded = true;
    }
    Ack a = { MAGIC, (uint16_t)packMv(), lastSeq };
    esp_now_send(mac, (uint8_t *)&a, sizeof(a));
  }
}
