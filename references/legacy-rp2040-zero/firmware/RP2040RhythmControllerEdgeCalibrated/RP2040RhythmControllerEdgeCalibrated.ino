#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_TinyUSB.h>
#include <EEPROM.h>
#include <math.h>
#include <string.h>

#if __has_include(<pico/bootrom.h>)
#include <pico/bootrom.h>
#define HAS_PICO_BOOTROM 1
#endif
#if __has_include(<hardware/watchdog.h>)
#include <hardware/watchdog.h>
#define HAS_RP2040_WATCHDOG 1
#endif
#if __has_include(<hardware/adc.h>)
#include <hardware/adc.h>
#define HAS_PICO_ADC 1
#endif

#include "BoardConfig.h"
#include "EdgeResponse.h"

enum KeyLedEffect : uint8_t {
  KEY_LED_PRESS = 0,
  KEY_LED_SOLID = 1,
  KEY_LED_RAINBOW = 2,
};

enum AmbientLedEffect : uint8_t {
  AMBIENT_RAINBOW = 0,
  AMBIENT_FLOW = 1,
  AMBIENT_MARQUEE = 2,
};

enum ReportId : uint8_t {
  REPORT_ID_MOUSE = 1,
  REPORT_ID_KEYBOARD = 2,
};

constexpr uint8_t HALL_COUNT = 4;

struct RgbColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct KeyLedConfig {
  uint8_t effect;
  uint8_t brightness;
  RgbColor color;
};

struct AmbientLedConfig {
  uint8_t effect;
  uint8_t brightness;
  RgbColor color;
};

struct PersistedConfig {
  uint16_t magic;
  uint8_t version;
  uint8_t mode;
  uint8_t syncLeds;
  uint8_t keymap[8];
  KeyLedConfig keyLed;
  AmbientLedConfig ambientLed;
  uint16_t hallMin[4];
  uint16_t hallMax[4];
  uint16_t hallCenter[4];
  uint16_t reserved[16];
  uint16_t checksum;
};

struct ButtonState {
  uint8_t integrator;
  bool stablePressed;
  bool pressedEdge;
  bool releasedEdge;
};

struct HallRuntime {
  uint16_t raw[4];
  float rawActivation[4];
  float activation[4];
  float positionSignal[4];
  float filteredPosition;
  float candidatePosition;
  float centroidPosition;
  bool calibrationActive;
};

struct MouseReport {
  uint8_t buttons;
  int8_t x;
  int8_t y;
  int8_t wheel;
  int8_t pan;
} __attribute__((packed));

struct KeyboardNkroReport {
  uint8_t modifiers;
  uint8_t reserved;
  uint8_t bits[29];
} __attribute__((packed));

struct HallCalibrationConfig {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  int8_t polarity;       // +1 raw rises, -1 raw drops, 0 auto.
  uint8_t baselineValid;
  uint8_t reserved0[2];
  float baseline[HALL_COUNT];
  float sensorGain[HALL_COUNT];
  float sensorReference[HALL_COUNT];
  float endpointSignalRef;
  float centerSumRef;
  float leftEndpointSum;
  float rightEndpointSum;
  float leftEndpointGainMax;
  float rightEndpointGainMax;
  float leftEndpointRatio;
  float rightEndpointRatio;
  float leftHandoffRatio;
  float rightHandoffRatio;
  float centerOffset;
  float centerWarp;
  float stillVectorDelta;
  float stillPositionDelta;
  uint32_t crc;
};

struct EndpointScanState {
  bool active;
  uint32_t startedMs;
  float centerSumMax;
  float leftBestRatio;
  float rightBestRatio;
  float leftBestSum;
  float rightBestSum;
  float leftBestStrongest;
  float rightBestStrongest;
};

constexpr uint8_t DEFAULT_KEYMAP[8] = {
  HID_KEY_A, HID_KEY_S, HID_KEY_D, HID_KEY_J,
  HID_KEY_K, HID_KEY_L, HID_KEY_Z, HID_KEY_M
};

constexpr uint8_t LEGACY_DEFAULT_KEYMAP[8] = {
  HID_KEY_S, HID_KEY_D, HID_KEY_F, HID_KEY_J,
  HID_KEY_K, HID_KEY_L, HID_KEY_Z, HID_KEY_M
};

constexpr char FIRMWARE_BUILD[] = "hall-edge-trial-1-20260915";
constexpr uint32_t MOUSE_REPORT_INTERVAL_MS = 4;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 250;
constexpr uint32_t KEYBOARD_KEEPALIVE_MS = 20;

constexpr float MOUSE_POSITION_GAIN = 620.0f;
constexpr uint16_t DEFAULT_MOUSE_SCALE_PERMILLE = 1500;
constexpr uint16_t MIN_MOUSE_SCALE_PERMILLE = 500;
constexpr uint16_t MAX_MOUSE_SCALE_PERMILLE = 2500;
constexpr uint16_t DEFAULT_SLIDE_ENABLED = 1;

constexpr float SENSOR_SPACING_MM = 33.33f;
constexpr float EDGE_OVERHANG_MM = 17.5f;
constexpr float SENSOR_ARRAY_MM = SENSOR_SPACING_MM * 3.0f;
constexpr float SLIDER_TRAVEL_MM = SENSOR_ARRAY_MM + EDGE_OVERHANG_MM * 2.0f;
constexpr float HALL_SENSOR_POSITION[HALL_COUNT] = {1.0f, 0.3333f, -0.3333f, -1.0f};

constexpr uint32_t BASELINE_SAMPLE_COUNT = 180;
constexpr uint8_t ADC_AVERAGE_COUNT = 10;
constexpr uint8_t ADC_DISCARD_COUNT_AFTER_MUX = 1;
constexpr uint8_t ADC_MUX_SETTLE_US = 3;
constexpr float ACTIVATE_DEADBAND = 6.0f;
constexpr float ACTIVATION_SIGNAL_SMOOTHING = 0.38f;
constexpr float TRACK_ENTER_STRENGTH = 18.0f;
constexpr float TRACK_EXIT_STRENGTH = 8.0f;
constexpr float TRACK_ENTER_STRONGEST = 10.0f;
constexpr float MIN_USABLE_STRENGTH = 10.0f;
constexpr float TOO_CLOSE_STRONGEST = 260.0f;
constexpr float TOO_CLOSE_TOTAL = 420.0f;

constexpr float CENTROID_MIN_TOTAL_SIGNAL = 18.0f;
constexpr float MAX_POSITION_STEP_RIGHT = 0.42f;
constexpr float MAX_POSITION_STEP_LEFT = 0.42f;

constexpr float ENDPOINT_BOOST_MIN_STRONGEST = 45.0f;
constexpr float ENDPOINT_BOOST_MIN_TOTAL = 48.0f;
constexpr float ENDPOINT_BOOST_DOMINANCE = 3.8f;
constexpr float ENDPOINT_BOOST_POSITION_START = 0.88f;
constexpr float ENDPOINT_BOOST_RATIO_START = 0.24f;
constexpr float ENDPOINT_BOOST_RATIO_FULL = 0.07f;
constexpr float ENDPOINT_BOOST_MAX = 0.90f;
constexpr float SENSOR_CENTER_EQUALIZE_WARP = -0.055f;
constexpr float ENDPOINT_OUTPUT_STRETCH_START = 0.88f;
constexpr float ENDPOINT_OUTPUT_STRETCH_MAX = 0.75f;

constexpr float POSITION_SMOOTHING = 0.62f;
constexpr float POSITION_SMOOTHING_SLOW = 0.022f;
constexpr float ADAPTIVE_FILTER_DELTA_REF = 0.090f;
constexpr float POSITION_NOISE_FLOOR = 0.0100f;

constexpr float MAX_MOUSE_DELTA = 96.0f;
constexpr float LOW_SPEED_MAX_DX = 2.0f;
constexpr float MOUSE_MED_SPEED_MAX_DX = 8.0f;
constexpr float MOUSE_SMALL_CONFIRM_PIXELS = 3.5f;
constexpr uint8_t MOUSE_SMALL_CONFIRM_SAMPLES = 4;
constexpr uint8_t OUTPUT_STILL_CONFIRM_SAMPLES = 4;
constexpr float OUTPUT_STILL_RELEASE_PIXELS_DEFAULT = 6.0f;
constexpr float OUTPUT_STILL_HOLD_PIXELS_DEFAULT = 3.8f;
constexpr float EDGE_OUTPUT_STILL_RELEASE_PIXELS = 8.0f;
constexpr float EDGE_OUTPUT_STILL_HOLD_PIXELS = 4.6f;
constexpr float OUTPUT_MOVING_RELEASE_PIXELS = 1.6f;
constexpr float OUTPUT_MOVING_HOLD_PIXELS = 0.8f;
constexpr float MOUSE_REMAINDER_BLEED_ALPHA = 0.024f;
constexpr float EDGE_REMAINDER_BLEED_ALPHA = 0.015f;
constexpr float MOUSE_REMAINDER_BLEED_MOTION_DELTA = 0.012f;
constexpr float MOUSE_JITTER_MOTION_DELTA = 0.022f;

constexpr float DEFAULT_STILL_VECTOR_DELTA = 4.2f;
constexpr float DEFAULT_STILL_POSITION_DELTA = 0.014f;
constexpr float EDGE_STILL_VECTOR_DELTA = 5.6f;
constexpr float EDGE_STILL_POSITION_DELTA = 0.020f;

constexpr uint32_t HALL_CAL_MAGIC = 0x484D4532UL;  // HME2
constexpr uint16_t HALL_CAL_VERSION = 3;
constexpr float DEFAULT_ENDPOINT_SIGNAL_REF = 62.0f;
constexpr float DEFAULT_CENTER_SUM_REF = 76.0f;
constexpr float DEFAULT_LEFT_ENDPOINT_RATIO = 0.105f;
constexpr float DEFAULT_RIGHT_ENDPOINT_RATIO = 0.105f;
constexpr float DEFAULT_LEFT_HANDOFF_RATIO = 0.30f;
constexpr float DEFAULT_RIGHT_HANDOFF_RATIO = 0.30f;
constexpr float DEFAULT_LEFT_ENDPOINT_GAIN_MAX = 1.30f;
constexpr float DEFAULT_RIGHT_ENDPOINT_GAIN_MAX = 1.36f;
constexpr uint32_t ENDPOINT_SCAN_MS = 8000;
constexpr float FACTORY_SENSOR_REFERENCE = 64.0f;

constexpr float BASELINE_IDLE_TOTAL = 4.5f;
constexpr float BASELINE_IDLE_STRONGEST = 3.0f;
constexpr float BASELINE_TRACK_ALPHA = 0.0008f;
constexpr uint32_t BASELINE_IDLE_DELAY_MS = 10000;
constexpr float PI_F = 3.14159265358979323846f;

uint8_t const HID_REPORT_DESCRIPTOR[] = {
  TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(REPORT_ID_MOUSE)),
  0x05, 0x01, 0x09, 0x06, 0xA1, 0x01,
  0x85, REPORT_ID_KEYBOARD,
  0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
  0x15, 0x00, 0x25, 0x01,
  0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
  0x95, 0x01, 0x75, 0x08, 0x81, 0x03,
  0x19, 0x00, 0x29, 0xE7,
  0x15, 0x00, 0x25, 0x01,
  0x75, 0x01, 0x96, 0xE8, 0x00, 0x81, 0x02,
  0xC0
};

PersistedConfig config;
ButtonState buttons[8];
HallRuntime hallRuntime;
HallCalibrationConfig hallCal;
EndpointScanState endpointScan = {};
struct GainCaptureState {
  bool active;
  bool invalid;
  uint8_t sensor;
  uint32_t startedMs;
  uint32_t lastSampleMs;
  uint16_t count;
  float sum;
  float minimum;
  float maximum;
};
GainCaptureState gainCapture = {};


MouseReport mouseReport = {};
KeyboardNkroReport keyboardReport = {};
KeyboardNkroReport lastKeyboardReport = {};
Adafruit_USBD_HID usbHid;
Adafruit_NeoPixel keyLeds(BoardConfig::KEY_LED_COUNT, BoardConfig::KEY_LED_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel ambientLeds(BoardConfig::AMBIENT_LED_COUNT, BoardConfig::AMBIENT_LED_PIN, NEO_GRB + NEO_KHZ800);

constexpr int HALL_CALIBRATION_EEPROM_OFFSET = sizeof(PersistedConfig);
static_assert(HALL_CALIBRATION_EEPROM_OFFSET + sizeof(HallCalibrationConfig) <= BoardConfig::EEPROM_SIZE,
              "BoardConfig::EEPROM_SIZE is too small for config plus hall calibration");

void sendStatusJson(const char *message, bool needsReplug = false);

char serialBuffer[160];
size_t serialLength = 0;
bool telemetryEnabled = false;
bool hallSignalUsable = false;
bool mouseTrackingActive = false;
float virtualMousePosition = 0.0f;
float mouseRemainder = 0.0f;
float hallBaseline[HALL_COUNT] = {};
float rawActivation[HALL_COUNT] = {};
float activation[HALL_COUNT] = {};
float positionSignal[HALL_COUNT] = {};
float previousPositionSignal[HALL_COUNT] = {};
float sensorVectorMotion = 0.0f;
float totalSignal = 0.0f;
float strongestSignal = 0.0f;
float rawAbsTotal = 0.0f;
float rawAbsStrongest = 0.0f;
float candidatePosition = 0.0f;
float centroidPosition = 0.0f;
float filteredPosition = 0.0f;
float previousFilteredPosition = 0.0f;
float medianInputPosition = 0.0f;
float lastPositionDelta = 0.0f;
float rawPositionHistory[3] = {};
float learnedVectorNoise = DEFAULT_STILL_VECTOR_DELTA * 0.25f;
float learnedPositionNoise = DEFAULT_STILL_POSITION_DELTA * 0.25f;
float learnedVectorDelta = DEFAULT_STILL_VECTOR_DELTA;
float learnedPositionDelta = DEFAULT_STILL_POSITION_DELTA;
float sensorGainSamples[HALL_COUNT] = {};
uint8_t rawPositionHistoryIndex = 0;
uint8_t rawPositionHistoryCount = 0;
uint8_t hallDominantSensor = 0;
int8_t polarityScore = 0;
int8_t lastDx = 0;
int8_t smallDeltaSign = 0;
uint8_t smallDeltaStreak = 0;
uint32_t idleStartedMs = 0;
bool hallSignalReliable = false;
bool strengthTooLow = true;
bool strengthTooHigh = false;
bool mouseOutputReleased = false;
bool calibrationDirty = false;
uint32_t lastButtonScanUs = 0;
uint32_t lastLedFrameMs = 0;
uint32_t lastTelemetryMs = 0;
uint32_t lastMouseReportMs = 0;
uint32_t lastKeyboardReportMs = 0;

template <typename T>
T clampValue(T value, T low, T high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

float safeMax(float a, float b) {
  return a > b ? a : b;
}

float safeMin(float a, float b) {
  return a < b ? a : b;
}

float smoothStep01(float value) {
  const float x = clampValue<float>(value, 0.0f, 1.0f);
  return x * x * (3.0f - 2.0f * x);
}

float median3(float a, float b, float c) {
  if (a > b) {
    const float t = a;
    a = b;
    b = t;
  }
  if (b > c) {
    const float t = b;
    b = c;
    c = t;
  }
  if (a > b) {
    const float t = a;
    a = b;
    b = t;
  }
  return b;
}

float pushPositionMedian(float value) {
  rawPositionHistory[rawPositionHistoryIndex] = value;
  rawPositionHistoryIndex = (rawPositionHistoryIndex + 1) % 3;
  if (rawPositionHistoryCount < 3) {
    ++rawPositionHistoryCount;
  }
  if (rawPositionHistoryCount < 3) {
    return value;
  }
  return median3(rawPositionHistory[0], rawPositionHistory[1], rawPositionHistory[2]);
}

uint16_t checksumConfig(const PersistedConfig &value) {
  const uint8_t *raw = reinterpret_cast<const uint8_t *>(&value);
  uint16_t sum = 0;
  for (size_t i = 0; i < sizeof(PersistedConfig) - sizeof(value.checksum); ++i) {
    sum = static_cast<uint16_t>(sum + raw[i]);
  }
  return sum;
}

void setDefaultConfig() {
  memset(&config, 0, sizeof(config));
  config.magic = BoardConfig::CONFIG_MAGIC;
  config.version = BoardConfig::CONFIG_VERSION;
  config.mode = 0;
  memcpy(config.keymap, DEFAULT_KEYMAP, sizeof(DEFAULT_KEYMAP));
  config.keyLed = {KEY_LED_PRESS, 128, {0, 196, 255}};
  config.ambientLed = {AMBIENT_RAINBOW, 128, {64, 224, 208}};
  for (uint8_t i = 0; i < 4; ++i) {
    config.hallMin[i] = 4095;
    config.hallMax[i] = 0;
    config.hallCenter[i] = 0;
  }
  config.reserved[0] = DEFAULT_MOUSE_SCALE_PERMILLE;
  config.reserved[1] = 0;
  config.reserved[2] = DEFAULT_SLIDE_ENABLED;
  config.checksum = checksumConfig(config);
}

void saveConfig() {
  config.checksum = checksumConfig(config);
  EEPROM.put(0, config);
  EEPROM.commit();
}

void loadConfig() {
  EEPROM.begin(BoardConfig::EEPROM_SIZE);
  EEPROM.get(0, config);
  const bool validChecksum = config.checksum == checksumConfig(config);
  const bool needsMigration = config.magic == BoardConfig::CONFIG_MAGIC &&
                              config.version == 1 &&
                              validChecksum;
  if (config.magic != BoardConfig::CONFIG_MAGIC ||
      (!needsMigration && config.version != BoardConfig::CONFIG_VERSION) ||
      !validChecksum) {
    setDefaultConfig();
    saveConfig();
  }
  if (needsMigration) {
    config.version = BoardConfig::CONFIG_VERSION;
    config.reserved[2] = DEFAULT_SLIDE_ENABLED;
    if (memcmp(config.keymap, LEGACY_DEFAULT_KEYMAP, sizeof(LEGACY_DEFAULT_KEYMAP)) == 0) {
      memcpy(config.keymap, DEFAULT_KEYMAP, sizeof(DEFAULT_KEYMAP));
    }
    saveConfig();
  }
  if (config.reserved[0] < MIN_MOUSE_SCALE_PERMILLE ||
      config.reserved[0] > MAX_MOUSE_SCALE_PERMILLE) {
    config.reserved[0] = DEFAULT_MOUSE_SCALE_PERMILLE;
    saveConfig();
  }
  if (config.reserved[2] > 1) {
    config.reserved[2] = DEFAULT_SLIDE_ENABLED;
    saveConfig();
  }
  if (config.reserved[3] != 0) {
    config.reserved[3] = 0;
    saveConfig();
  }
}

uint16_t getMouseScalePermille() {
  return clampValue<uint16_t>(config.reserved[0], MIN_MOUSE_SCALE_PERMILLE, MAX_MOUSE_SCALE_PERMILLE);
}

float getEffectiveMouseGain() {
  return MOUSE_POSITION_GAIN * static_cast<float>(getMouseScalePermille()) / 1000.0f;
}

bool isSlideEnabled() {
  return config.reserved[2] != 0;
}

uint32_t fnv1a32(const uint8_t *data, size_t length) {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < length; ++i) {
    hash ^= data[i];
    hash *= 16777619UL;
  }
  return hash;
}

uint32_t hallCalibrationCrc(const HallCalibrationConfig &value) {
  return fnv1a32(reinterpret_cast<const uint8_t *>(&value),
                 sizeof(HallCalibrationConfig) - sizeof(value.crc));
}

void setDefaultHallCalibration() {
  memset(&hallCal, 0, sizeof(hallCal));
  hallCal.magic = HALL_CAL_MAGIC;
  hallCal.version = HALL_CAL_VERSION;
  hallCal.size = sizeof(HallCalibrationConfig);
  hallCal.polarity = 0;
  hallCal.baselineValid = 0;
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    hallCal.baseline[i] = 0.0f;
    hallCal.sensorGain[i] = 1.0f;
    hallCal.sensorReference[i] = 0.0f;
  }
  hallCal.endpointSignalRef = DEFAULT_ENDPOINT_SIGNAL_REF;
  hallCal.centerSumRef = DEFAULT_CENTER_SUM_REF;
  hallCal.leftEndpointSum = DEFAULT_ENDPOINT_SIGNAL_REF;
  hallCal.rightEndpointSum = DEFAULT_ENDPOINT_SIGNAL_REF;
  hallCal.leftEndpointGainMax = DEFAULT_LEFT_ENDPOINT_GAIN_MAX;
  hallCal.rightEndpointGainMax = DEFAULT_RIGHT_ENDPOINT_GAIN_MAX;
  hallCal.leftEndpointRatio = DEFAULT_LEFT_ENDPOINT_RATIO;
  hallCal.rightEndpointRatio = DEFAULT_RIGHT_ENDPOINT_RATIO;
  hallCal.leftHandoffRatio = DEFAULT_LEFT_HANDOFF_RATIO;
  hallCal.rightHandoffRatio = DEFAULT_RIGHT_HANDOFF_RATIO;
  hallCal.centerOffset = 0.0f;
  hallCal.centerWarp = 0.0f;
  hallCal.stillVectorDelta = DEFAULT_STILL_VECTOR_DELTA;
  hallCal.stillPositionDelta = DEFAULT_STILL_POSITION_DELTA;
  hallCal.crc = hallCalibrationCrc(hallCal);
}

bool loadHallCalibration() {
  EEPROM.get(HALL_CALIBRATION_EEPROM_OFFSET, hallCal);
  const bool valid =
    hallCal.magic == HALL_CAL_MAGIC &&
    hallCal.version == HALL_CAL_VERSION &&
    hallCal.size == sizeof(HallCalibrationConfig) &&
    hallCal.crc == hallCalibrationCrc(hallCal);
  if (!valid) {
    setDefaultHallCalibration();
    return false;
  }
  if (hallCal.leftHandoffRatio <= hallCal.leftEndpointRatio + 0.03f) {
    hallCal.leftHandoffRatio = DEFAULT_LEFT_HANDOFF_RATIO;
  }
  if (hallCal.rightHandoffRatio <= hallCal.rightEndpointRatio + 0.03f) {
    hallCal.rightHandoffRatio = DEFAULT_RIGHT_HANDOFF_RATIO;
  }
  return true;
}

void saveHallCalibration() {
  hallCal.magic = HALL_CAL_MAGIC;
  hallCal.version = HALL_CAL_VERSION;
  hallCal.size = sizeof(HallCalibrationConfig);
  hallCal.crc = hallCalibrationCrc(hallCal);
  EEPROM.put(HALL_CALIBRATION_EEPROM_OFFSET, hallCal);
  EEPROM.commit();
  calibrationDirty = false;
}

bool isHallAdcPin(uint8_t pin) {
  return pin >= 26 && pin <= 29;
}

void configureHallAdc() {
  analogReadResolution(12);
  #if HAS_PICO_ADC
  adc_init();
  adc_run(false);
  adc_set_round_robin(0);
  adc_set_temp_sensor_enabled(false);
  adc_fifo_setup(false, false, 0, false, false);
  adc_fifo_drain();
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    if (isHallAdcPin(BoardConfig::HALL_PINS[i])) {
      adc_gpio_init(BoardConfig::HALL_PINS[i]);
    }
  }
  #endif
}

uint16_t readHallAverage(uint8_t pin) {
  uint32_t sum = 0;
  #if HAS_PICO_ADC
  if (isHallAdcPin(pin)) {
    adc_run(false);
    adc_set_round_robin(0);
    adc_set_temp_sensor_enabled(false);
    adc_select_input(pin - 26);
    delayMicroseconds(ADC_MUX_SETTLE_US);
    for (uint8_t i = 0; i < ADC_DISCARD_COUNT_AFTER_MUX; ++i) {
      (void)adc_read();
    }
    for (uint8_t i = 0; i < ADC_AVERAGE_COUNT; ++i) {
      sum += adc_read();
    }
    return static_cast<uint16_t>(sum / ADC_AVERAGE_COUNT);
  }
  #endif
  for (uint8_t i = 0; i < ADC_AVERAGE_COUNT; ++i) {
    sum += analogRead(pin);
  }
  return static_cast<uint16_t>(sum / ADC_AVERAGE_COUNT);
}

void captureHallBaseline() {
  uint32_t sum[HALL_COUNT] = {};
  hallRuntime.calibrationActive = true;
  for (uint32_t n = 0; n < BASELINE_SAMPLE_COUNT; ++n) {
    #if HAS_RP2040_WATCHDOG
    watchdog_update();
    #endif
    for (uint8_t i = 0; i < HALL_COUNT; ++i) {
      sum[i] += readHallAverage(BoardConfig::HALL_PINS[i]);
    }
    delay(2);
  }
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    hallBaseline[i] = static_cast<float>(sum[i]) / BASELINE_SAMPLE_COUNT;
    hallCal.baseline[i] = hallBaseline[i];
  }
  hallCal.baselineValid = 1;
  calibrationDirty = true;
  hallRuntime.calibrationActive = false;
}

void resetHallTracking() {
  gainCapture.active = false;
  endpointScan.active = false;
  hallSignalReliable = false;
  hallSignalUsable = false;
  mouseTrackingActive = false;
  mouseOutputReleased = false;
  smallDeltaSign = 0;
  smallDeltaStreak = 0;
  sensorVectorMotion = 0.0f;
  totalSignal = 0.0f;
  strongestSignal = 0.0f;
  rawAbsTotal = 0.0f;
  rawAbsStrongest = 0.0f;
  candidatePosition = 0.0f;
  centroidPosition = 0.0f;
  filteredPosition = 0.0f;
  previousFilteredPosition = 0.0f;
  medianInputPosition = 0.0f;
  lastPositionDelta = 0.0f;
  rawPositionHistoryIndex = 0;
  rawPositionHistoryCount = 0;
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    hallRuntime.raw[i] = 0;
    hallRuntime.rawActivation[i] = 0.0f;
    hallRuntime.activation[i] = 0.0f;
    hallRuntime.positionSignal[i] = 0.0f;
    rawActivation[i] = 0.0f;
    activation[i] = 0.0f;
    positionSignal[i] = 0.0f;
    previousPositionSignal[i] = 0.0f;
  }
  for (uint8_t i = 0; i < 3; ++i) {
    rawPositionHistory[i] = 0.0f;
  }
  virtualMousePosition = 0.0f;
  mouseRemainder = 0.0f;
  hallDominantSensor = 0;
  lastDx = 0;
  strengthTooLow = true;
  strengthTooHigh = false;
  mouseReport = {};
}

void beginCalibration() {
  resetHallTracking();
  captureHallBaseline();
  saveHallCalibration();
  resetHallTracking();
}

void initializeHallCalibration() {
  const bool loaded = loadHallCalibration();
  if (loaded && hallCal.baselineValid) {
    for (uint8_t i = 0; i < HALL_COUNT; ++i) {
      hallBaseline[i] = hallCal.baseline[i];
      sensorGainSamples[i] = hallCal.sensorReference[i];
    }
  } else {
    delay(700);
    captureHallBaseline();
    saveHallCalibration();
  }
  resetHallTracking();
}

bool equalsIgnoreCase(const char *left, const char *right) {
  if (!left || !right) return false;
  while (*left && *right) {
    char a = *left++;
    char b = *right++;
    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
    if (a != b) return false;
  }
  return *left == '\0' && *right == '\0';
}

uint32_t packColor(RgbColor color, uint8_t brightness) {
  return Adafruit_NeoPixel::Color(
    static_cast<uint8_t>((static_cast<uint16_t>(color.r) * brightness) / 255),
    static_cast<uint8_t>((static_cast<uint16_t>(color.g) * brightness) / 255),
    static_cast<uint8_t>((static_cast<uint16_t>(color.b) * brightness) / 255)
  );
}

RgbColor wheel(uint8_t pos) {
  pos = 255 - pos;
  if (pos < 85) return {static_cast<uint8_t>(255 - pos * 3), 0, static_cast<uint8_t>(pos * 3)};
  if (pos < 170) {
    pos -= 85;
    return {0, static_cast<uint8_t>(pos * 3), static_cast<uint8_t>(255 - pos * 3)};
  }
  pos -= 170;
  return {static_cast<uint8_t>(pos * 3), static_cast<uint8_t>(255 - pos * 3), 0};
}

const char *keyLedEffectName(uint8_t effect) {
  if (effect == KEY_LED_SOLID) return "solid";
  if (effect == KEY_LED_RAINBOW) return "rainbow";
  return "press";
}

const char *ambientLedEffectName(uint8_t effect) {
  if (effect == AMBIENT_FLOW) return "flow";
  if (effect == AMBIENT_MARQUEE) return "marquee";
  return "rainbow";
}

void setupUsb() {
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.setID(0x0CA3, 0x0021);
    TinyUSBDevice.setManufacturerDescriptor("SEGA");
    TinyUSBDevice.setProductDescriptor("RP2040 Rhythm Controller");
    TinyUSBDevice.setSerialDescriptor("RP2040-RHYTHM-001");
    TinyUSBDevice.begin(0);
  }

  usbHid.setPollInterval(1);
  usbHid.setReportDescriptor(HID_REPORT_DESCRIPTOR, sizeof(HID_REPORT_DESCRIPTOR));
  usbHid.setStringDescriptor("Rhythm HID");
  usbHid.begin();

  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
}

void setupPins() {
  for (uint8_t i = 0; i < 8; ++i) {
    pinMode(BoardConfig::BUTTON_PINS[i], INPUT_PULLUP);
  }
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    pinMode(BoardConfig::HALL_PINS[i], INPUT);
  }
  configureHallAdc();
}

void setupLeds() {
  keyLeds.begin();
  ambientLeds.begin();
  keyLeds.show();
  ambientLeds.show();
}

void enterBootloaderMode() {
  if (Serial) {
    sendStatusJson("entering UF2 bootloader");
    Serial.flush();
  }

  keyLeds.clear();
  ambientLeds.clear();
  keyLeds.show();
  ambientLeds.show();
  delay(50);

  #if HAS_PICO_BOOTROM
  reset_usb_boot(0, 0);
  #else
  if (Serial) {
    sendStatusJson("bootloader jump is not available in this RP2040 core");
  }
  #endif
}

void updateButtons() {
  const uint32_t now = micros();
  if (now - lastButtonScanUs < BoardConfig::BUTTON_SCAN_US) return;
  lastButtonScanUs = now;

  for (uint8_t i = 0; i < 8; ++i) {
    ButtonState &button = buttons[i];
    button.pressedEdge = false;
    button.releasedEdge = false;
    const bool pressed = digitalRead(BoardConfig::BUTTON_PINS[i]) == LOW;
    if (pressed) {
      if (button.integrator < 4) ++button.integrator;
    } else if (button.integrator > 0) {
      --button.integrator;
    }
    const bool debounced = button.integrator >= 3;
    if (debounced != button.stablePressed) {
      button.stablePressed = debounced;
      button.pressedEdge = debounced;
      button.releasedEdge = !debounced;
    }
  }
}

void syncHallRuntime() {
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    hallRuntime.rawActivation[i] = rawActivation[i];
    hallRuntime.activation[i] = activation[i];
    hallRuntime.positionSignal[i] = positionSignal[i];
  }
  hallRuntime.filteredPosition = filteredPosition;
  hallRuntime.candidatePosition = candidatePosition;
  hallRuntime.centroidPosition = centroidPosition;
}

void updateAutoPolarity(const float deviations[HALL_COUNT]) {
  if (hallCal.polarity != 0) return;

  float strongestAbs = 0.0f;
  int8_t strongestSign = 0;
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    const float absDev = fabsf(deviations[i]);
    if (absDev > strongestAbs) {
      strongestAbs = absDev;
      strongestSign = deviations[i] >= 0.0f ? 1 : -1;
    }
  }
  if (strongestAbs < 14.0f) return;

  polarityScore = clampValue<int8_t>(polarityScore + strongestSign, -8, 8);
  if (polarityScore >= 4) {
    hallCal.polarity = 1;
    calibrationDirty = true;
    sendStatusJson("hall polarity auto-locked to RAW_RISE");
  } else if (polarityScore <= -4) {
    hallCal.polarity = -1;
    calibrationDirty = true;
    sendStatusJson("hall polarity auto-locked to RAW_DROP");
  }
}

void updateBaselineWhenIdle(const float deviations[HALL_COUNT]) {
  const bool idleCandidate = !hallSignalReliable &&
                             rawAbsTotal < BASELINE_IDLE_TOTAL &&
                             rawAbsStrongest < BASELINE_IDLE_STRONGEST;
  const uint32_t now = millis();
  if (!idleCandidate) {
    idleStartedMs = 0;
    return;
  }
  if (idleStartedMs == 0) {
    idleStartedMs = now;
    return;
  }
  if (now - idleStartedMs < BASELINE_IDLE_DELAY_MS) {
    return;
  }
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    hallBaseline[i] += deviations[i] * BASELINE_TRACK_ALPHA;
    hallCal.baseline[i] = hallBaseline[i];
  }
  hallCal.baselineValid = 1;
  calibrationDirty = true;
}

float plainCentroidPosition() {
  if (totalSignal < CENTROID_MIN_TOTAL_SIGNAL || strongestSignal <= 0.0f) {
    return filteredPosition;
  }

  float weighted = 0.0f;
  float total = 0.0f;
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    const float weight = positionSignal[i];
    weighted += HALL_SENSOR_POSITION[i] * weight;
    total += weight;
  }
  if (total <= 0.0f) {
    return filteredPosition;
  }
  return clampValue<float>(weighted / total, -1.0f, 1.0f);
}

// Keep the existing centroid, but use a continuous, monotonic edge map.
// Live amplitude/dominance thresholds must not switch position corrections.
float rightEdgePosition(float centroid) {
  if (centroid <= 0.0f) return centroid;
  return EdgeResponse::map(centroid, hallCal.rightEndpointRatio, hallCal.rightHandoffRatio);
}

float leftEdgePosition(float centroid) {
  if (centroid >= 0.0f) return centroid;
  return -EdgeResponse::map(-centroid, hallCal.leftEndpointRatio, hallCal.leftHandoffRatio);
}

float applyCalibrationShape(float position) {
  float shaped = position + hallCal.centerOffset;
  if (hallCal.centerWarp != 0.0f) {
    shaped += hallCal.centerWarp * sinf(3.0f * PI_F * shaped);
  }
  return clampValue<float>(shaped, -1.0f, 1.0f);
}

float estimateSliderPosition() {
  centroidPosition = plainCentroidPosition();
  float position = applyCalibrationShape(centroidPosition);
  position = rightEdgePosition(position);
  position = leftEdgePosition(position);

  const float delta = position - filteredPosition;
  const float maxStep = delta < 0.0f ? MAX_POSITION_STEP_LEFT : MAX_POSITION_STEP_RIGHT;
  if (delta > maxStep) {
    position = filteredPosition + maxStep;
  } else if (delta < -maxStep) {
    position = filteredPosition - maxStep;
  }
  return clampValue<float>(position, -1.0f, 1.0f);
}

float adaptivePositionAlpha(float delta) {
  const float motion = clampValue<float>((fabsf(delta) - POSITION_NOISE_FLOOR) / ADAPTIVE_FILTER_DELTA_REF,
                                         0.0f,
                                         1.0f);
  return POSITION_SMOOTHING_SLOW + (POSITION_SMOOTHING - POSITION_SMOOTHING_SLOW) * motion;
}

float equalizeSensorCenterPosition(float position) {
  // Keep the existing interior correction; avoid a second endpoint stretch.
  const float shaped = position + SENSOR_CENTER_EQUALIZE_WARP * sinf(3.0f * PI_F * position);
  return clampValue<float>(shaped, -1.0f, 1.0f);
}

float mousePixelsForPosition(float position) {
  return equalizeSensorCenterPosition(position) * getEffectiveMouseGain();
}

bool edgeRegionActive() {
  return fabsf(filteredPosition) >= 0.86f || hallDominantSensor == 1 || hallDominantSensor == 4;
}

float effectiveStillVectorDelta() {
  const float base = edgeRegionActive() ? safeMax(EDGE_STILL_VECTOR_DELTA, learnedVectorDelta) : learnedVectorDelta;
  return clampValue<float>(base, 2.8f, 8.0f);
}

float effectiveStillPositionDelta() {
  const float base = edgeRegionActive() ? safeMax(EDGE_STILL_POSITION_DELTA, learnedPositionDelta) : learnedPositionDelta;
  return clampValue<float>(base, 0.006f, 0.030f);
}

void learnStillNoise(bool outputStill) {
  if (!outputStill || fabsf(mouseRemainder) > EDGE_OUTPUT_STILL_RELEASE_PIXELS || lastDx != 0) {
    return;
  }
  learnedVectorNoise += (sensorVectorMotion - learnedVectorNoise) * 0.015f;
  learnedPositionNoise += (fabsf(lastPositionDelta) - learnedPositionNoise) * 0.015f;
  learnedVectorDelta = clampValue<float>(safeMax(hallCal.stillVectorDelta, learnedVectorNoise * 4.0f + 0.35f), 2.8f, 8.0f);
  learnedPositionDelta = clampValue<float>(safeMax(hallCal.stillPositionDelta, learnedPositionNoise * 4.0f + 0.0015f),
                                           0.006f,
                                           0.030f);
}

void updateHall() {
  float deviations[HALL_COUNT] = {};
  rawAbsTotal = 0.0f;
  rawAbsStrongest = 0.0f;
  totalSignal = 0.0f;
  strongestSignal = 0.0f;
  hallDominantSensor = 0;

  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    hallRuntime.raw[i] = readHallAverage(BoardConfig::HALL_PINS[i]);
    deviations[i] = static_cast<float>(hallRuntime.raw[i]) - hallBaseline[i];
    const float absDev = fabsf(deviations[i]);
    rawAbsTotal += absDev;
    rawAbsStrongest = safeMax(rawAbsStrongest, absDev);
  }

  updateAutoPolarity(deviations);

  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    float field = 0.0f;
    if (hallCal.polarity == 0) {
      field = fabsf(deviations[i]);
    } else {
      field = static_cast<float>(hallCal.polarity) * deviations[i];
      if (field < 0.0f) field = 0.0f;
    }
    rawActivation[i] = field > ACTIVATE_DEADBAND ? field - ACTIVATE_DEADBAND : 0.0f;
    activation[i] = rawActivation[i] * hallCal.sensorGain[i];
    positionSignal[i] += (activation[i] - positionSignal[i]) * ACTIVATION_SIGNAL_SMOOTHING;

    totalSignal += positionSignal[i];
    if (positionSignal[i] > strongestSignal) {
      strongestSignal = positionSignal[i];
      hallDominantSensor = i + 1;
    }
  }

  sensorVectorMotion = 0.0f;
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    sensorVectorMotion += fabsf(positionSignal[i] - previousPositionSignal[i]);
    previousPositionSignal[i] = positionSignal[i];
  }

  const bool wasReliable = hallSignalReliable;
  if (hallSignalReliable) {
    hallSignalReliable = totalSignal >= TRACK_EXIT_STRENGTH;
  } else {
    hallSignalReliable = totalSignal >= TRACK_ENTER_STRENGTH && strongestSignal >= TRACK_ENTER_STRONGEST;
    if (hallSignalReliable) {
      rawPositionHistoryIndex = 0;
      rawPositionHistoryCount = 0;
      virtualMousePosition = mousePixelsForPosition(filteredPosition);
      mouseRemainder = 0.0f;
    }
  }

  strengthTooLow = totalSignal < MIN_USABLE_STRENGTH || !hallSignalReliable;
  strengthTooHigh = strongestSignal > TOO_CLOSE_STRONGEST || totalSignal > TOO_CLOSE_TOTAL;
  hallSignalUsable = !strengthTooLow;

  updateBaselineWhenIdle(deviations);

  if (!hallSignalReliable) {
    candidatePosition = filteredPosition;
    centroidPosition = filteredPosition;
    medianInputPosition = filteredPosition;
    lastPositionDelta = 0.0f;
    syncHallRuntime();
    return;
  }

  if (!wasReliable) {
    centroidPosition = plainCentroidPosition();
    filteredPosition = leftEdgePosition(rightEdgePosition(applyCalibrationShape(centroidPosition)));
    previousFilteredPosition = filteredPosition;
    virtualMousePosition = mousePixelsForPosition(filteredPosition);
    mouseTrackingActive = false;
    mouseOutputReleased = false;
  }
  candidatePosition = estimateSliderPosition();
  medianInputPosition = pushPositionMedian(candidatePosition);
  previousFilteredPosition = filteredPosition;
  const float delta = medianInputPosition - filteredPosition;
  const float alpha = adaptivePositionAlpha(delta);
  filteredPosition += delta * alpha;
  filteredPosition = clampValue<float>(filteredPosition, -1.0f, 1.0f);
  lastPositionDelta = filteredPosition - previousFilteredPosition;
  syncHallRuntime();
}

void rebuildKeyboardReport() {
  memset(&keyboardReport, 0, sizeof(keyboardReport));
  for (uint8_t i = 0; i < 8; ++i) {
    if (!buttons[i].stablePressed) continue;
    const uint8_t code = config.keymap[i];
    if (code < 232) keyboardReport.bits[code / 8] |= 1u << (code % 8);
  }
}

void updateMouseReport() {
  lastDx = 0;
  mouseReport = {};
  if (!isSlideEnabled() || gainCapture.active || endpointScan.active) {
    mouseTrackingActive = false;
    mouseOutputReleased = false;
    virtualMousePosition = mousePixelsForPosition(filteredPosition);
    mouseRemainder = 0.0f;
    smallDeltaSign = 0;
    smallDeltaStreak = 0;
    return;
  }
  const uint32_t now = millis();
  if (now - lastMouseReportMs < MOUSE_REPORT_INTERVAL_MS) return;
  lastMouseReportMs = now;

  const float currentPixels = mousePixelsForPosition(filteredPosition);
  if (!hallSignalUsable) {
    mouseTrackingActive = false;
    mouseOutputReleased = false;
    virtualMousePosition = currentPixels;
    mouseRemainder = 0.0f;
    smallDeltaSign = 0;
    smallDeltaStreak = 0;
    return;
  }

  if (!mouseTrackingActive) {
    mouseTrackingActive = true;
    mouseOutputReleased = false;
    virtualMousePosition = currentPixels;
    mouseRemainder = 0.0f;
    smallDeltaSign = 0;
    smallDeltaStreak = 0;
    return;
  }

  const float error = currentPixels - virtualMousePosition;
  mouseRemainder = error;

  const bool outputStill = sensorVectorMotion < effectiveStillVectorDelta() &&
                           fabsf(lastPositionDelta) < effectiveStillPositionDelta();
  const bool edge = edgeRegionActive();
  learnStillNoise(outputStill);

  const float releasePixels = outputStill
                                ? (edge ? EDGE_OUTPUT_STILL_RELEASE_PIXELS : OUTPUT_STILL_RELEASE_PIXELS_DEFAULT)
                                : OUTPUT_MOVING_RELEASE_PIXELS;
  const float holdPixels = outputStill
                             ? (edge ? EDGE_OUTPUT_STILL_HOLD_PIXELS : OUTPUT_STILL_HOLD_PIXELS_DEFAULT)
                             : OUTPUT_MOVING_HOLD_PIXELS;
  const float absError = fabsf(error);

  if (!mouseOutputReleased) {
    if (absError < releasePixels) {
      if (fabsf(lastPositionDelta) < MOUSE_REMAINDER_BLEED_MOTION_DELTA) {
        const float bleedAlpha = edge ? EDGE_REMAINDER_BLEED_ALPHA : MOUSE_REMAINDER_BLEED_ALPHA;
        virtualMousePosition += error * bleedAlpha;
        mouseRemainder = currentPixels - virtualMousePosition;
      }
      return;
    }
    mouseOutputReleased = true;
  } else if (absError < holdPixels && fabsf(lastPositionDelta) < MOUSE_JITTER_MOTION_DELTA) {
    mouseOutputReleased = false;
    smallDeltaSign = 0;
    smallDeltaStreak = 0;
    return;
  }

  float outputDelta = 0.0f;
  if (error >= 1.0f) {
    outputDelta = floorf(error);
  } else if (error <= -1.0f) {
    outputDelta = ceilf(error);
  } else {
    return;
  }

  const float absDelta = fabsf(outputDelta);
  if (absDelta <= MOUSE_SMALL_CONFIRM_PIXELS) {
    const uint8_t confirmSamples = outputStill ? OUTPUT_STILL_CONFIRM_SAMPLES : MOUSE_SMALL_CONFIRM_SAMPLES;
    const int8_t direction = outputDelta > 0.0f ? 1 : -1;
    if (direction == smallDeltaSign) {
      if (smallDeltaStreak < confirmSamples) {
        ++smallDeltaStreak;
      }
    } else {
      smallDeltaSign = direction;
      smallDeltaStreak = 1;
    }
    if (smallDeltaStreak < confirmSamples) {
      return;
    }
  } else {
    smallDeltaSign = 0;
    smallDeltaStreak = 0;
  }

  const float shapedLimit = absDelta <= 8.0f
                              ? LOW_SPEED_MAX_DX
                              : (absDelta <= 32.0f ? MOUSE_MED_SPEED_MAX_DX : MAX_MOUSE_DELTA);
  outputDelta = clampValue<float>(outputDelta, -shapedLimit, shapedLimit);
  outputDelta = clampValue<float>(outputDelta, -MAX_MOUSE_DELTA, MAX_MOUSE_DELTA);

  mouseReport.x = static_cast<int8_t>(outputDelta);
  if (usbHid.ready() && mouseReport.x != 0) {
    if (usbHid.mouseMove(REPORT_ID_MOUSE, mouseReport.x, mouseReport.y)) {
      virtualMousePosition += mouseReport.x;
    }
  }
  lastDx = mouseReport.x;
  mouseRemainder = currentPixels - virtualMousePosition;
}

const char *hallPolarityName() {
  if (hallCal.polarity > 0) return "raw_rise";
  if (hallCal.polarity < 0) return "raw_drop";
  return "auto";
}

void setHallPolarityAuto() {
  hallCal.polarity = 0;
  polarityScore = 0;
  calibrationDirty = true;
  saveHallCalibration();
  sendStatusJson("hall polarity set to auto");
}

void flipHallPolarity() {
  if (hallCal.polarity == 0) {
    hallCal.polarity = -1;
  } else {
    hallCal.polarity = static_cast<int8_t>(-hallCal.polarity);
  }
  polarityScore = hallCal.polarity > 0 ? 4 : -4;
  calibrationDirty = true;
  saveHallCalibration();
  sendStatusJson(hallCal.polarity > 0 ? "hall polarity forced to RAW_RISE" : "hall polarity forced to RAW_DROP");
}

void forceHallPolarity(int8_t polarity) {
  hallCal.polarity = polarity > 0 ? 1 : -1;
  polarityScore = hallCal.polarity > 0 ? 4 : -4;
  calibrationDirty = true;
  saveHallCalibration();
  sendStatusJson(hallCal.polarity > 0 ? "hall polarity forced to RAW_RISE" : "hall polarity forced to RAW_DROP");
}

void startEndpointScan() {
  if (gainCapture.active || endpointScan.active) {
    sendStatusJson("calibration busy; wait for the current capture");
    return;
  }
  endpointScan.active = true;
  endpointScan.startedMs = millis();
  endpointScan.centerSumMax = 0.0f;
  endpointScan.leftBestRatio = 999.0f;
  endpointScan.rightBestRatio = 999.0f;
  endpointScan.leftBestSum = 0.0f;
  endpointScan.rightBestSum = 0.0f;
  endpointScan.leftBestStrongest = 0.0f;
  endpointScan.rightBestStrongest = 0.0f;
  sendStatusJson("hall endpoint scan started; sweep slowly to both mechanical endpoints for 8 seconds");
}

void finishEndpointScan() {
  endpointScan.active = false;
  bool updated = false;
  if (endpointScan.leftBestRatio >= 0.32f || endpointScan.rightBestRatio >= 0.32f ||
      endpointScan.leftBestSum < 20.0f || endpointScan.rightBestSum < 20.0f) {
    sendStatusJson("endpoint scan rejected: both outer sensor regions needed; old calibration kept");
    return;
  }

  if (endpointScan.centerSumMax >= 30.0f) {
    hallCal.centerSumRef = endpointScan.centerSumMax;
    updated = true;
  }
  if (endpointScan.leftBestRatio < 1.0f && endpointScan.leftBestSum >= 20.0f) {
    hallCal.leftEndpointRatio = clampValue<float>(endpointScan.leftBestRatio, 0.02f, 0.32f);
    hallCal.leftEndpointSum = endpointScan.leftBestSum;
    hallCal.leftEndpointGainMax = clampValue<float>(hallCal.centerSumRef / safeMax(18.0f, hallCal.leftEndpointSum), 1.0f, 1.65f);
    updated = true;
  }
  if (endpointScan.rightBestRatio < 1.0f && endpointScan.rightBestSum >= 20.0f) {
    hallCal.rightEndpointRatio = clampValue<float>(endpointScan.rightBestRatio, 0.02f, 0.32f);
    hallCal.rightEndpointSum = endpointScan.rightBestSum;
    hallCal.rightEndpointGainMax = clampValue<float>(hallCal.centerSumRef / safeMax(18.0f, hallCal.rightEndpointSum), 1.0f, 1.65f);
    updated = true;
  }
  if (updated) {
    hallCal.endpointSignalRef = (hallCal.leftEndpointSum + hallCal.rightEndpointSum) * 0.5f;
    calibrationDirty = true;
    saveHallCalibration();
    resetHallTracking();
    sendStatusJson("hall endpoint scan saved");
  } else {
    sendStatusJson("hall endpoint scan finished without enough endpoint signal");
  }
}

void serviceEndpointScan() {
  if (!endpointScan.active) return;

  if (hallSignalReliable) {
    if (fabsf(centroidPosition) < 0.14f) {
      endpointScan.centerSumMax = safeMax(endpointScan.centerSumMax, totalSignal);
    }

    const float h1 = positionSignal[0];
    const float h2 = positionSignal[1];
    const float h3 = positionSignal[2];
    const float h4 = positionSignal[3];
    if (h1 >= ENDPOINT_BOOST_MIN_STRONGEST && h1 >= safeMax(8.0f, h2) * 1.8f) {
      const float ratio = h2 / safeMax(1.0f, h1);
      if (ratio < endpointScan.rightBestRatio) {
        endpointScan.rightBestRatio = ratio;
        endpointScan.rightBestSum = totalSignal;
        endpointScan.rightBestStrongest = h1;
      }
    }
    if (h4 >= ENDPOINT_BOOST_MIN_STRONGEST && h4 >= safeMax(8.0f, h3) * 1.8f) {
      const float ratio = h3 / safeMax(1.0f, h4);
      if (ratio < endpointScan.leftBestRatio) {
        endpointScan.leftBestRatio = ratio;
        endpointScan.leftBestSum = totalSignal;
        endpointScan.leftBestStrongest = h4;
      }
    }
  }

  if (millis() - endpointScan.startedMs >= ENDPOINT_SCAN_MS) {
    finishEndpointScan();
  }
}

bool captureSensorGainSample() {
  if (gainCapture.active || endpointScan.active) {
    sendStatusJson("calibration busy; wait for the current capture");
    return false;
  }
  if (!hallSignalReliable || hallDominantSensor == 0 ||
      rawActivation[hallDominantSensor - 1] < 20.0f) {
    sendStatusJson("gain capture ignored; place magnet over one sensor at normal height");
    return false;
  }
  gainCapture = {};
  gainCapture.active = true;
  gainCapture.sensor = hallDominantSensor - 1;
  gainCapture.startedMs = millis();
  gainCapture.lastSampleMs = millis();
  gainCapture.minimum = 4096.0f;
  sendStatusJson("gain capture started; keep magnet still for 0.5 seconds");
  return true;
}

void serviceGainCapture() {
  if (!gainCapture.active) return;
  const uint32_t now = millis();
  const uint8_t idx = gainCapture.sensor;
  if (now - gainCapture.lastSampleMs >= 2) {
    gainCapture.lastSampleMs = now;
    const float sample = rawActivation[idx];
    if (!hallSignalReliable || hallDominantSensor != idx + 1 || sample < 20.0f ||
        hallRuntime.raw[idx] <= 8 || hallRuntime.raw[idx] >= 4087) {
      gainCapture.invalid = true;
    }
    gainCapture.sum += sample;
    gainCapture.minimum = safeMin(gainCapture.minimum, sample);
    gainCapture.maximum = safeMax(gainCapture.maximum, sample);
    ++gainCapture.count;
  }
  if (now - gainCapture.startedMs < 500) return;
  gainCapture.active = false;
  const float mean = gainCapture.count ? gainCapture.sum / gainCapture.count : 0.0f;
  if (gainCapture.invalid || gainCapture.count < 50 ||
      gainCapture.maximum - gainCapture.minimum > safeMax(8.0f, mean * 0.15f)) {
    sendStatusJson("gain capture rejected: weak, moving or clipped signal; old calibration kept");
    return;
  }
  // Trim the two extreme readings and REPLACE this sensor's previous capture.
  sensorGainSamples[idx] = (gainCapture.sum - gainCapture.minimum - gainCapture.maximum) /
                           (gainCapture.count - 2);
  hallCal.sensorReference[idx] = sensorGainSamples[idx];
  bool allSeen = true;
  float sum = 0.0f;
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    allSeen = allSeen && hallCal.sensorReference[i] > 0.0f;
    sum += hallCal.sensorReference[i];
  }
  const float reference = allSeen ? sum / HALL_COUNT : FACTORY_SENSOR_REFERENCE;
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    if (hallCal.sensorReference[i] > 0.0f) {
      hallCal.sensorGain[i] = clampValue<float>(reference / hallCal.sensorReference[i], 0.55f, 1.85f);
    }
  }
  // Ratios measured with previous gains are no longer comparable.
  hallCal.leftEndpointRatio = DEFAULT_LEFT_ENDPOINT_RATIO;
  hallCal.rightEndpointRatio = DEFAULT_RIGHT_ENDPOINT_RATIO;
  hallCal.leftHandoffRatio = DEFAULT_LEFT_HANDOFF_RATIO;
  hallCal.rightHandoffRatio = DEFAULT_RIGHT_HANDOFF_RATIO;
  saveHallCalibration();
  resetHallTracking();
  sendStatusJson(allSeen ? "gain saved; all four sensors available; repeat endpoint scan after gains"
                        : "gain saved; capture the remaining sensors before endpoint scan");
}

bool captureCenterCalibration() {
  if (!hallSignalReliable) {
    sendStatusJson("center calibration ignored; no reliable magnet signal");
    return false;
  }
  hallCal.centerOffset = clampValue<float>(hallCal.centerOffset - filteredPosition, -0.12f, 0.12f);
  hallCal.centerSumRef = safeMax(hallCal.centerSumRef, totalSignal);
  calibrationDirty = true;
  saveHallCalibration();
  sendStatusJson("hall center offset saved");
  return true;
}

void resetHallCalibrationDefaults() {
  setDefaultHallCalibration();
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    hallBaseline[i] = hallCal.baseline[i];
    sensorGainSamples[i] = 0.0f;
  }
  calibrationDirty = true;
  saveHallCalibration();
  resetHallTracking();
  sendStatusJson("hall calibration defaults restored");
}

void sendKeyboardAndMouse() {
  updateMouseReport();
  rebuildKeyboardReport();
  const uint32_t now = millis();
  const bool changed = memcmp(&keyboardReport, &lastKeyboardReport, sizeof(keyboardReport)) != 0;
  const bool keepalive = now - lastKeyboardReportMs >= KEYBOARD_KEEPALIVE_MS;
  if ((changed || keepalive) && usbHid.ready()) {
    if (usbHid.sendReport(REPORT_ID_KEYBOARD, &keyboardReport, sizeof(keyboardReport))) {
      memcpy(&lastKeyboardReport, &keyboardReport, sizeof(keyboardReport));
      lastKeyboardReportMs = now;
    }
  }
}

void renderKeyLeds() {
  KeyLedConfig effective = config.keyLed;
  if (config.syncLeds) {
    effective.brightness = config.ambientLed.brightness;
    effective.color = config.ambientLed.color;
    effective.effect = config.ambientLed.effect == AMBIENT_RAINBOW ? KEY_LED_RAINBOW : KEY_LED_SOLID;
  }
  const uint32_t tick = millis();
  for (uint8_t i = 0; i < BoardConfig::KEY_LED_COUNT; ++i) {
    const uint8_t pixel = BoardConfig::KEY_LED_PIXEL_FOR_BUTTON[i];
    uint32_t color = 0;
    if (effective.effect == KEY_LED_PRESS) {
      color = buttons[i].stablePressed ? packColor(effective.color, effective.brightness) : 0;
    } else if (effective.effect == KEY_LED_SOLID) {
      color = packColor(effective.color, effective.brightness);
    } else {
      color = packColor(wheel(static_cast<uint8_t>(tick / 8 + i * 24)), effective.brightness);
    }
    keyLeds.setPixelColor(pixel, color);
  }
  keyLeds.show();
}

void renderAmbientLeds() {
  const uint32_t tick = millis();
  for (uint8_t i = 0; i < BoardConfig::AMBIENT_LED_COUNT; ++i) {
    uint32_t color = 0;
    if (config.ambientLed.effect == AMBIENT_FLOW) {
      const float phase = (static_cast<float>((tick / 8 + i * 20) % 255) / 255.0f) * 6.28318f;
      const float wave = (sinf(phase) + 1.0f) * 0.5f;
      color = packColor(config.ambientLed.color, static_cast<uint8_t>(config.ambientLed.brightness * wave));
    } else if (config.ambientLed.effect == AMBIENT_MARQUEE) {
      color = ((tick / 120) % BoardConfig::AMBIENT_LED_COUNT) == i
        ? packColor(config.ambientLed.color, config.ambientLed.brightness)
        : 0;
    } else {
      color = packColor(wheel(static_cast<uint8_t>(tick / 10 + i * 16)), config.ambientLed.brightness);
    }
    ambientLeds.setPixelColor(i, color);
  }
  ambientLeds.show();
}

void updateLeds() {
  const uint32_t now = millis();
  if (now - lastLedFrameMs < BoardConfig::LED_FRAME_MS) return;
  lastLedFrameMs = now;
  renderKeyLeds();
  renderAmbientLeds();
}

void sendStatusJson(const char *message, bool needsReplug) {
  Serial.print("{\"type\":\"status\",\"message\":\"");
  Serial.print(message);
  Serial.print("\",\"needsReplug\":");
  Serial.print(needsReplug ? "true" : "false");
  Serial.println("}");
}

void sendConfigJson(const char *eventName = "config") {
  Serial.print("{\"type\":\"");
  Serial.print(eventName);
  Serial.print("\",\"firmware\":\"");
  Serial.print(FIRMWARE_BUILD);
  Serial.print("\",\"mouseScale\":");
  Serial.print(getMouseScalePermille());
  Serial.print(",\"slideEnabled\":");
  Serial.print(isSlideEnabled() ? "true" : "false");
  Serial.print(",\"sync\":");
  Serial.print(config.syncLeds ? "true" : "false");
  Serial.print(",\"keyCodes\":[");
  for (uint8_t i = 0; i < 8; ++i) {
    if (i) Serial.print(',');
    Serial.print(config.keymap[i]);
  }
  Serial.print("],\"keyLed\":{\"effect\":\"");
  Serial.print(keyLedEffectName(config.keyLed.effect));
  Serial.print("\",\"brightness\":");
  Serial.print(config.keyLed.brightness);
  Serial.print(",\"color\":[");
  Serial.print(config.keyLed.color.r); Serial.print(',');
  Serial.print(config.keyLed.color.g); Serial.print(',');
  Serial.print(config.keyLed.color.b);
  Serial.print("]},\"ambientLed\":{\"effect\":\"");
  Serial.print(ambientLedEffectName(config.ambientLed.effect));
  Serial.print("\",\"brightness\":");
  Serial.print(config.ambientLed.brightness);
  Serial.print(",\"color\":[");
  Serial.print(config.ambientLed.color.r); Serial.print(',');
  Serial.print(config.ambientLed.color.g); Serial.print(',');
  Serial.print(config.ambientLed.color.b);
  Serial.print("]},\"hall\":{\"model\":\"edge_calibrated\",\"raw\":[");
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    if (i) Serial.print(',');
    Serial.print(hallRuntime.raw[i]);
  }
  Serial.print("],\"min\":[");
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    if (i) Serial.print(',');
    Serial.print(hallBaseline[i], 1);
  }
  Serial.print("],\"max\":[");
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    if (i) Serial.print(',');
    Serial.print(static_cast<int>(activation[i]));
  }
  Serial.print("],\"activation\":[");
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    if (i) Serial.print(',');
    Serial.print(activation[i], 1);
  }
  Serial.print("],\"rawActivation\":[");
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    if (i) Serial.print(',');
    Serial.print(rawActivation[i], 1);
  }
  Serial.print("],\"position\":");
  Serial.print(filteredPosition, 3);
  Serial.print(",\"candidate\":");
  Serial.print(candidatePosition, 3);
  Serial.print(",\"centroid\":");
  Serial.print(centroidPosition, 3);
  Serial.print(",\"dx\":");
  Serial.print(static_cast<int>(lastDx));
  Serial.print(",\"rem\":");
  Serial.print(mouseRemainder, 2);
  Serial.print(",\"strength\":");
  Serial.print(totalSignal, 1);
  Serial.print(",\"vector\":");
  Serial.print(sensorVectorMotion, 2);
  Serial.print(",\"dominant\":");
  Serial.print(hallDominantSensor);
  Serial.print(",\"tooClose\":");
  Serial.print(strengthTooHigh ? "true" : "false");
  Serial.print(",\"usable\":");
  Serial.print(hallSignalUsable ? "true" : "false");
  Serial.print(",\"baselineValid\":");
  Serial.print(hallCal.baselineValid ? "true" : "false");
  Serial.print(",\"calibrating\":");
  Serial.print((hallRuntime.calibrationActive || gainCapture.active) ? "true" : "false");
  Serial.print(",\"dirty\":");
  Serial.print(calibrationDirty ? "true" : "false");
  Serial.print(",\"polarity\":\"");
  Serial.print(hallPolarityName());
  Serial.print("\",\"endpointScan\":{\"active\":");
  Serial.print(endpointScan.active ? "true" : "false");
  Serial.print(",\"leftRatio\":");
  Serial.print(endpointScan.leftBestRatio, 3);
  Serial.print(",\"rightRatio\":");
  Serial.print(endpointScan.rightBestRatio, 3);
  Serial.print(",\"centerSumMax\":");
  Serial.print(endpointScan.centerSumMax, 1);
  Serial.print("},\"calibration\":{\"sensorGain\":[");
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    if (i) Serial.print(',');
    Serial.print(hallCal.sensorGain[i], 3);
  }
  Serial.print("],\"sensorReference\":[");
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    if (i) Serial.print(',');
    Serial.print(hallCal.sensorReference[i], 1);
  }
  Serial.print("],\"leftEndpointRatio\":");
  Serial.print(hallCal.leftEndpointRatio, 3);
  Serial.print(",\"rightEndpointRatio\":");
  Serial.print(hallCal.rightEndpointRatio, 3);
  Serial.print(",\"leftEndpointSum\":");
  Serial.print(hallCal.leftEndpointSum, 1);
  Serial.print(",\"rightEndpointSum\":");
  Serial.print(hallCal.rightEndpointSum, 1);
  Serial.print(",\"endpointSignalRef\":");
  Serial.print(hallCal.endpointSignalRef, 1);
  Serial.print(",\"centerSumRef\":");
  Serial.print(hallCal.centerSumRef, 1);
  Serial.print(",\"centerOffset\":");
  Serial.print(hallCal.centerOffset, 5);
  Serial.print(",\"centerWarp\":");
  Serial.print(hallCal.centerWarp, 5);
  Serial.print(",\"stillVectorDelta\":");
  Serial.print(hallCal.stillVectorDelta, 3);
  Serial.print(",\"stillPositionDelta\":");
  Serial.print(hallCal.stillPositionDelta, 5);
  Serial.print("}");
  Serial.println("}}");
}

void maybeSendTelemetry() {
  if (!telemetryEnabled || !Serial) return;
  const uint32_t now = millis();
  if (now - lastTelemetryMs < TELEMETRY_INTERVAL_MS) return;
  if (Serial.availableForWrite() < 128) return;
  lastTelemetryMs = now;
  sendConfigJson("telemetry");
}

long parseLong(const char *value) {
  return strtol(value, nullptr, 10);
}

void handleSetCommand(char *field, char *a, char *b, char *c) {
  if (!field) {
    sendStatusJson("missing field");
    return;
  }
  if (equalsIgnoreCase(field, "KEYMAP") && a && b) {
    const long index = parseLong(a);
    const long keycode = parseLong(b);
    if (index >= 0 && index < 8 && keycode >= 0 && keycode < 232) {
      config.keymap[index] = static_cast<uint8_t>(keycode);
      saveConfig();
      sendConfigJson();
      return;
    }
  }
  if (equalsIgnoreCase(field, "SYNC") && a) {
    config.syncLeds = parseLong(a) ? 1 : 0;
    saveConfig();
    sendConfigJson();
    return;
  }
  if (equalsIgnoreCase(field, "MOUSE_SCALE") && a) {
    config.reserved[0] = static_cast<uint16_t>(clampValue<long>(parseLong(a), MIN_MOUSE_SCALE_PERMILLE, MAX_MOUSE_SCALE_PERMILLE));
    virtualMousePosition = mousePixelsForPosition(filteredPosition);
    mouseRemainder = 0.0f;
    saveConfig();
    sendConfigJson();
    return;
  }
  if ((equalsIgnoreCase(field, "SLIDE") || equalsIgnoreCase(field, "SLIDE_ENABLED")) && a) {
    config.reserved[2] = parseLong(a) ? 1 : 0;
    resetHallTracking();
    saveConfig();
    sendConfigJson();
    return;
  }
  if ((equalsIgnoreCase(field, "HALL_POLARITY") || equalsIgnoreCase(field, "POLARITY")) && a) {
    if (equalsIgnoreCase(a, "AUTO") || equalsIgnoreCase(a, "0")) {
      setHallPolarityAuto();
    } else if (equalsIgnoreCase(a, "RISE") || equalsIgnoreCase(a, "RAW_RISE") || equalsIgnoreCase(a, "1")) {
      forceHallPolarity(1);
    } else if (equalsIgnoreCase(a, "DROP") || equalsIgnoreCase(a, "RAW_DROP") || equalsIgnoreCase(a, "-1")) {
      forceHallPolarity(-1);
    } else if (equalsIgnoreCase(a, "FLIP")) {
      flipHallPolarity();
    } else {
      sendStatusJson("unsupported hall polarity");
    }
    sendConfigJson();
    return;
  }
  if (equalsIgnoreCase(field, "HALL_ENDPOINT_SCAN") || equalsIgnoreCase(field, "ENDPOINT_SCAN")) {
    startEndpointScan();
    sendConfigJson();
    return;
  }
  if (equalsIgnoreCase(field, "HALL_SENSOR_GAIN") || equalsIgnoreCase(field, "SENSOR_GAIN")) {
    captureSensorGainSample();
    sendConfigJson();
    return;
  }
  if (equalsIgnoreCase(field, "HALL_CENTER") || equalsIgnoreCase(field, "CENTER")) {
    captureCenterCalibration();
    sendConfigJson();
    return;
  }
  if (equalsIgnoreCase(field, "HALL_CAL_SAVE") || equalsIgnoreCase(field, "CAL_SAVE")) {
    saveHallCalibration();
    sendStatusJson("hall calibration saved");
    sendConfigJson();
    return;
  }
  if (equalsIgnoreCase(field, "HALL_CAL_DEFAULTS") || equalsIgnoreCase(field, "CAL_DEFAULTS")) {
    resetHallCalibrationDefaults();
    sendConfigJson();
    return;
  }
  if (equalsIgnoreCase(field, "KEYLED_EFFECT") && a) {
    config.keyLed.effect = equalsIgnoreCase(a, "SOLID") ? KEY_LED_SOLID : (equalsIgnoreCase(a, "RAINBOW") ? KEY_LED_RAINBOW : KEY_LED_PRESS);
    saveConfig();
    sendConfigJson();
    return;
  }
  if (equalsIgnoreCase(field, "KEYLED_BRIGHTNESS") && a) {
    config.keyLed.brightness = static_cast<uint8_t>(clampValue<long>(parseLong(a), 0, 255));
    saveConfig();
    sendConfigJson();
    return;
  }
  if (equalsIgnoreCase(field, "KEYLED_COLOR") && a && b && c) {
    config.keyLed.color = {
      static_cast<uint8_t>(clampValue<long>(parseLong(a), 0, 255)),
      static_cast<uint8_t>(clampValue<long>(parseLong(b), 0, 255)),
      static_cast<uint8_t>(clampValue<long>(parseLong(c), 0, 255))
    };
    saveConfig();
    sendConfigJson();
    return;
  }
  if (equalsIgnoreCase(field, "AMBIENT_EFFECT") && a) {
    config.ambientLed.effect = equalsIgnoreCase(a, "FLOW") ? AMBIENT_FLOW : (equalsIgnoreCase(a, "MARQUEE") ? AMBIENT_MARQUEE : AMBIENT_RAINBOW);
    saveConfig();
    sendConfigJson();
    return;
  }
  if (equalsIgnoreCase(field, "AMBIENT_BRIGHTNESS") && a) {
    config.ambientLed.brightness = static_cast<uint8_t>(clampValue<long>(parseLong(a), 0, 255));
    saveConfig();
    sendConfigJson();
    return;
  }
  if (equalsIgnoreCase(field, "AMBIENT_COLOR") && a && b && c) {
    config.ambientLed.color = {
      static_cast<uint8_t>(clampValue<long>(parseLong(a), 0, 255)),
      static_cast<uint8_t>(clampValue<long>(parseLong(b), 0, 255)),
      static_cast<uint8_t>(clampValue<long>(parseLong(c), 0, 255))
    };
    saveConfig();
    sendConfigJson();
    return;
  }
  if (equalsIgnoreCase(field, "CALIBRATE")) {
    beginCalibration();
    sendStatusJson("hall baseline captured; keep magnet away while calibrating");
    sendConfigJson();
    return;
  }
  sendStatusJson("unsupported command");
}

void handleCommand(char *line) {
  if (line[0] != '\0' && line[1] == '\0') {
    const char ch = line[0];
    if (ch == '?') {
      sendStatusJson("commands: GET, SET CALIBRATE, SET HALL_POLARITY AUTO|RISE|DROP|FLIP, SET HALL_ENDPOINT_SCAN, SET HALL_SENSOR_GAIN, SET HALL_CENTER, SET HALL_CAL_SAVE, SET HALL_CAL_DEFAULTS");
      sendConfigJson();
      return;
    }
    if (ch == 'z' || ch == 'Z') {
      telemetryEnabled = false;
      beginCalibration();
      sendStatusJson("hall baseline captured");
      sendConfigJson();
      return;
    }
    if (ch == 'a' || ch == 'A') {
      setHallPolarityAuto();
      sendConfigJson();
      return;
    }
    if (ch == 'o' || ch == 'O') {
      flipHallPolarity();
      sendConfigJson();
      return;
    }
    if (ch == 'e' || ch == 'E') {
      startEndpointScan();
      sendConfigJson();
      return;
    }
    if (ch == 'g' || ch == 'G') {
      captureSensorGainSample();
      sendConfigJson();
      return;
    }
    if (ch == 'c' || ch == 'C') {
      captureCenterCalibration();
      sendConfigJson();
      return;
    }
    if (ch == 's' || ch == 'S') {
      saveHallCalibration();
      sendStatusJson("hall calibration saved");
      sendConfigJson();
      return;
    }
    if (ch == 'd' || ch == 'D') {
      resetHallCalibrationDefaults();
      sendConfigJson();
      return;
    }
    if (ch == 't' || ch == 'T') {
      telemetryEnabled = !telemetryEnabled;
      sendStatusJson(telemetryEnabled ? "telemetry enabled" : "telemetry disabled");
      sendConfigJson();
      return;
    }
    if (ch == 'p' || ch == 'P') {
      sendConfigJson();
      return;
    }
    if (ch == 'b' || ch == 'B') {
      enterBootloaderMode();
      return;
    }
  }

  char *cmd = strtok(line, " ");
  if (!cmd) return;
  if (equalsIgnoreCase(cmd, "GET")) {
    telemetryEnabled = true;
    sendConfigJson();
    return;
  }
  if (equalsIgnoreCase(cmd, "TELEMETRY")) {
    char *arg = strtok(nullptr, " ");
    telemetryEnabled = !(arg && (equalsIgnoreCase(arg, "OFF") || equalsIgnoreCase(arg, "0")));
    sendStatusJson(telemetryEnabled ? "telemetry enabled" : "telemetry disabled");
    return;
  }
  if (equalsIgnoreCase(cmd, "BOOTLOADER") || equalsIgnoreCase(cmd, "BOOT") || equalsIgnoreCase(cmd, "UF2")) {
    enterBootloaderMode();
    return;
  }
  if (equalsIgnoreCase(cmd, "SET")) {
    char *field = strtok(nullptr, " ");
    char *a = strtok(nullptr, " ");
    char *b = strtok(nullptr, " ");
    char *c = strtok(nullptr, " ");
    handleSetCommand(field, a, b, c);
    return;
  }
  sendStatusJson("unknown command");
}

void serviceSerialProtocol() {
  while (Serial.available()) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\r') continue;
    if (ch == '\n') {
      serialBuffer[serialLength] = '\0';
      handleCommand(serialBuffer);
      serialLength = 0;
      continue;
    }
    if (serialLength < sizeof(serialBuffer) - 1) serialBuffer[serialLength++] = ch;
  }
}

void setup() {
  #if HAS_RP2040_WATCHDOG
  watchdog_enable(2000, true);
  watchdog_update();
  #endif
  loadConfig();
  setupUsb();
  Serial.begin(BoardConfig::CONFIG_BAUD);
  setupPins();
  setupLeds();
  delay(150);
  initializeHallCalibration();
}

void loop() {
  #if HAS_RP2040_WATCHDOG
  watchdog_update();
  #endif
  #ifdef TINYUSB_NEED_POLLING_TASK
  TinyUSBDevice.task();
  #endif

  updateButtons();
  updateHall();
  serviceEndpointScan();
  serviceGainCapture();
  sendKeyboardAndMouse();
  updateLeds();
  serviceSerialProtocol();
  maybeSendTelemetry();
}
