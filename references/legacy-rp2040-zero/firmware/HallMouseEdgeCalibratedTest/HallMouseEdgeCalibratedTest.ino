#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <EEPROM.h>
#include <math.h>
#include <string.h>

#if __has_include(<pico/bootrom.h>)
#include <pico/bootrom.h>
#define HAS_PICO_BOOTROM 1
#endif

// Hall mouse edge extrapolation test firmware.
//
// Mechanical model used by this test:
//   - H4/H3/H2/H1 are spaced by about 33.33 mm.
//   - The slider has about 17.5 mm of travel outside H4 and H1.
//
// The internal position coordinate intentionally stays compatible with the
// older good-feeling firmware:
//   H1 = +1.0, H2 = +0.333, H3 = -0.333, H4 = -1.0.
// The extra 17.5 mm outside H1/H4 is handled only by edge-ratio extrapolation.
// This preserves the center/near-center mouse gain while still letting the
// mechanical endpoints settle cleanly at +/-1.
//
// Read order is GP26, GP27, GP28, GP29.
// Physical left-to-right order is GP29, GP28, GP27, GP26.
// In logs the read order is named H1, H2, H3, H4:
//   H1 = right edge sensor on GP26
//   H4 = left edge sensor on GP29

constexpr uint8_t HALL_COUNT = 4;
constexpr uint8_t HALL_PINS[HALL_COUNT] = {26, 27, 28, 29};
constexpr uint8_t BUTTON_PINS[8] = {14, 13, 12, 3, 2, 1, 15, 4};
constexpr uint8_t BOOTLOADER_BUTTON_A = 0;
constexpr uint8_t BOOTLOADER_BUTTON_B = 7;
constexpr uint32_t BOOTLOADER_HOLD_MS = 2000;

constexpr float SENSOR_SPACING_MM = 33.33f;
constexpr float EDGE_OVERHANG_MM = 17.5f;
constexpr float SENSOR_ARRAY_MM = SENSOR_SPACING_MM * 3.0f;
constexpr float SLIDER_TRAVEL_MM = SENSOR_ARRAY_MM + EDGE_OVERHANG_MM * 2.0f;
constexpr float SENSOR_POS[HALL_COUNT] = {
  1.0f,
  0.3333f,
  -0.3333f,
  -1.0f
};

constexpr char FIRMWARE_BUILD[] = "hall-centroid-output-endboost-20260504-fix5";
constexpr uint32_t BASELINE_SAMPLE_COUNT = 180;
constexpr uint8_t ADC_AVERAGE_COUNT = 8;
constexpr uint32_t PRINT_INTERVAL_MS = 45;
constexpr float PRINT_POSITION_DELTA = 0.006f;
constexpr float PRINT_REMAINDER_DELTA = 2.5f;
constexpr uint32_t MOUSE_REPORT_INTERVAL_MS = 4;

constexpr float ACTIVATE_DEADBAND = 6.0f;
constexpr float ACTIVATION_SIGNAL_SMOOTHING = 0.42f;
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
constexpr float POSITION_SMOOTHING_SLOW = 0.028f;
constexpr float ADAPTIVE_FILTER_DELTA_REF = 0.090f;
constexpr float POSITION_NOISE_FLOOR = 0.0080f;

constexpr float MOUSE_GAIN = 930.0f;
constexpr float MAX_MOUSE_DELTA = 96.0f;
constexpr float LOW_SPEED_MAX_DX = 2.0f;
constexpr float MOUSE_MED_SPEED_MAX_DX = 8.0f;
constexpr float MOUSE_SMALL_CONFIRM_PIXELS = 3.5f;
constexpr uint8_t MOUSE_SMALL_CONFIRM_SAMPLES = 4;
constexpr uint8_t OUTPUT_STILL_CONFIRM_SAMPLES = 3;
constexpr float OUTPUT_STILL_RELEASE_PIXELS_DEFAULT = 5.5f;
constexpr float OUTPUT_STILL_HOLD_PIXELS_DEFAULT = 3.5f;
constexpr float EDGE_OUTPUT_STILL_RELEASE_PIXELS = 7.5f;
constexpr float EDGE_OUTPUT_STILL_HOLD_PIXELS = 4.2f;
constexpr float OUTPUT_MOVING_RELEASE_PIXELS = 1.6f;
constexpr float OUTPUT_MOVING_HOLD_PIXELS = 0.8f;
constexpr float MOUSE_REMAINDER_BLEED_ALPHA = 0.02f;
constexpr float EDGE_REMAINDER_BLEED_ALPHA = 0.012f;
constexpr float MOUSE_REMAINDER_BLEED_MOTION_DELTA = 0.012f;
constexpr float MOUSE_JITTER_MOTION_DELTA = 0.022f;

constexpr float DEFAULT_STILL_VECTOR_DELTA = 3.8f;
constexpr float DEFAULT_STILL_POSITION_DELTA = 0.012f;
constexpr float EDGE_STILL_VECTOR_DELTA = 5.2f;
constexpr float EDGE_STILL_POSITION_DELTA = 0.018f;

constexpr uint32_t EEPROM_SIZE = 1024;
constexpr uint32_t CAL_MAGIC = 0x484D4532UL;  // HME2
constexpr uint16_t CAL_VERSION = 3;
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

struct CalibrationConfig {
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

Adafruit_USBD_HID usbHid;

uint8_t const HID_REPORT_DESCRIPTOR[] = {
  TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(1))
};

CalibrationConfig cal;
EndpointScanState endpointScan = {};

float baseline[HALL_COUNT] = {};
uint16_t rawValues[HALL_COUNT] = {};
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
uint8_t rawPositionHistoryIndex = 0;
uint8_t rawPositionHistoryCount = 0;
float virtualMousePosition = 0.0f;
float mouseRemainder = 0.0f;
float lastPrintedPosition = 0.0f;
float lastPrintedRemainder = 0.0f;
float learnedVectorNoise = DEFAULT_STILL_VECTOR_DELTA * 0.25f;
float learnedPositionNoise = DEFAULT_STILL_POSITION_DELTA * 0.25f;
float learnedVectorDelta = DEFAULT_STILL_VECTOR_DELTA;
float learnedPositionDelta = DEFAULT_STILL_POSITION_DELTA;
float sensorGainSamples[HALL_COUNT] = {};

uint8_t dominantSensor = 0;
int8_t polarityScore = 0;
int8_t lastDx = 0;
int8_t smallDeltaSign = 0;
uint8_t smallDeltaStreak = 0;
uint32_t lastPrintMs = 0;
uint32_t lastMouseReportMs = 0;
uint32_t bootloaderShortcutStartedMs = 0;
uint32_t idleStartedMs = 0;
bool signalReliable = false;
bool strengthTooLow = true;
bool strengthTooHigh = false;
bool trackingActive = false;
bool mouseOutputReleased = false;
bool telemetryAlways = false;
bool hasPrintedStatus = false;
bool calibrationDirty = false;

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

uint32_t fnv1a32(const uint8_t *data, size_t length) {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < length; ++i) {
    hash ^= data[i];
    hash *= 16777619UL;
  }
  return hash;
}

uint32_t calibrationCrc(const CalibrationConfig &value) {
  return fnv1a32(reinterpret_cast<const uint8_t *>(&value),
                 sizeof(CalibrationConfig) - sizeof(value.crc));
}

void setDefaultCalibration() {
  memset(&cal, 0, sizeof(cal));
  cal.magic = CAL_MAGIC;
  cal.version = CAL_VERSION;
  cal.size = sizeof(CalibrationConfig);
  cal.polarity = 0;
  cal.baselineValid = 0;
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    cal.sensorGain[i] = 1.0f;
    cal.sensorReference[i] = 0.0f;
    cal.baseline[i] = 0.0f;
  }
  cal.endpointSignalRef = DEFAULT_ENDPOINT_SIGNAL_REF;
  cal.centerSumRef = DEFAULT_CENTER_SUM_REF;
  cal.leftEndpointSum = DEFAULT_ENDPOINT_SIGNAL_REF;
  cal.rightEndpointSum = DEFAULT_ENDPOINT_SIGNAL_REF;
  cal.leftEndpointGainMax = DEFAULT_LEFT_ENDPOINT_GAIN_MAX;
  cal.rightEndpointGainMax = DEFAULT_RIGHT_ENDPOINT_GAIN_MAX;
  cal.leftEndpointRatio = DEFAULT_LEFT_ENDPOINT_RATIO;
  cal.rightEndpointRatio = DEFAULT_RIGHT_ENDPOINT_RATIO;
  cal.leftHandoffRatio = DEFAULT_LEFT_HANDOFF_RATIO;
  cal.rightHandoffRatio = DEFAULT_RIGHT_HANDOFF_RATIO;
  cal.centerOffset = 0.0f;
  cal.centerWarp = 0.0f;
  cal.stillVectorDelta = DEFAULT_STILL_VECTOR_DELTA;
  cal.stillPositionDelta = DEFAULT_STILL_POSITION_DELTA;
  cal.crc = calibrationCrc(cal);
}

bool loadCalibration() {
  EEPROM.get(0, cal);
  const bool valid =
    cal.magic == CAL_MAGIC &&
    cal.version == CAL_VERSION &&
    cal.size == sizeof(CalibrationConfig) &&
    cal.crc == calibrationCrc(cal);
  if (!valid) {
    setDefaultCalibration();
    return false;
  }
  if (cal.leftHandoffRatio <= cal.leftEndpointRatio + 0.03f) {
    cal.leftHandoffRatio = DEFAULT_LEFT_HANDOFF_RATIO;
  }
  if (cal.rightHandoffRatio <= cal.rightEndpointRatio + 0.03f) {
    cal.rightHandoffRatio = DEFAULT_RIGHT_HANDOFF_RATIO;
  }
  return true;
}

void saveCalibration() {
  cal.magic = CAL_MAGIC;
  cal.version = CAL_VERSION;
  cal.size = sizeof(CalibrationConfig);
  cal.crc = calibrationCrc(cal);
  EEPROM.put(0, cal);
  EEPROM.commit();
  calibrationDirty = false;
  Serial.println("CAL saved to EEPROM.");
}

void printCalibration() {
  Serial.println();
  Serial.println("=== Hall Edge Calibrated Test ===");
  Serial.print("Firmware: ");
  Serial.println(FIRMWARE_BUILD);
  Serial.print("Slider mm: ");
  Serial.print(SLIDER_TRAVEL_MM, 1);
  Serial.print("    Sensor array mm: ");
  Serial.println(SENSOR_ARRAY_MM, 1);
  Serial.print("Sensor-space norm H1..H4: ");
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    if (i) Serial.print(", ");
    Serial.print(SENSOR_POS[i], 3);
  }
  Serial.println();
  Serial.print("Polarity: ");
  Serial.println(cal.polarity > 0 ? "RAW_RISE" : (cal.polarity < 0 ? "RAW_DROP" : "AUTO"));
  Serial.print("Baseline: ");
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    if (i) Serial.print(", ");
    Serial.print(baseline[i], 1);
  }
  Serial.println();
  Serial.print("Sensor gain: ");
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    if (i) Serial.print(", ");
    Serial.print(cal.sensorGain[i], 3);
  }
  Serial.println();
  Serial.print("Endpoint ratio L/R: ");
  Serial.print(cal.leftEndpointRatio, 3);
  Serial.print(" / ");
  Serial.print(cal.rightEndpointRatio, 3);
  Serial.print("    Handoff L/R: ");
  Serial.print(cal.leftHandoffRatio, 3);
  Serial.print(" / ");
  Serial.println(cal.rightHandoffRatio, 3);
  Serial.print("Endpoint SUM L/R/ref: ");
  Serial.print(cal.leftEndpointSum, 1);
  Serial.print(" / ");
  Serial.print(cal.rightEndpointSum, 1);
  Serial.print(" / ");
  Serial.println(cal.endpointSignalRef, 1);
  Serial.print("Endpoint gain max L/R: ");
  Serial.print(cal.leftEndpointGainMax, 3);
  Serial.print(" / ");
  Serial.println(cal.rightEndpointGainMax, 3);
  Serial.print("Center sum/ref offset/warp: ");
  Serial.print(cal.centerSumRef, 1);
  Serial.print(" / ");
  Serial.print(cal.centerOffset, 4);
  Serial.print(" / ");
  Serial.println(cal.centerWarp, 4);
  Serial.print("Still vec/pos: ");
  Serial.print(cal.stillVectorDelta, 3);
  Serial.print(" / ");
  Serial.println(cal.stillPositionDelta, 5);
  Serial.println("Serial: ? help, Z zero+save, A auto polarity, O flip polarity, E endpoint scan, G sensor gain sample, C center, S save, D defaults, T telemetry, B bootloader.");
  Serial.println();
}

uint16_t readAverage(uint8_t pin) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < ADC_AVERAGE_COUNT; ++i) {
    sum += analogRead(pin);
  }
  return static_cast<uint16_t>(sum / ADC_AVERAGE_COUNT);
}

void captureBaseline() {
  uint32_t sum[HALL_COUNT] = {};
  for (uint32_t n = 0; n < BASELINE_SAMPLE_COUNT; ++n) {
    for (uint8_t i = 0; i < HALL_COUNT; ++i) {
      sum[i] += analogRead(HALL_PINS[i]);
    }
    delay(2);
  }
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    baseline[i] = static_cast<float>(sum[i]) / BASELINE_SAMPLE_COUNT;
    cal.baseline[i] = baseline[i];
  }
  cal.baselineValid = 1;
  calibrationDirty = true;
}

void resetRuntimeTracking() {
  signalReliable = false;
  strengthTooLow = true;
  strengthTooHigh = false;
  trackingActive = false;
  mouseOutputReleased = false;
  smallDeltaSign = 0;
  smallDeltaStreak = 0;
  sensorVectorMotion = 0.0f;
  totalSignal = 0.0f;
  strongestSignal = 0.0f;
  candidatePosition = 0.0f;
  centroidPosition = 0.0f;
  filteredPosition = 0.0f;
  previousFilteredPosition = 0.0f;
  medianInputPosition = 0.0f;
  lastPositionDelta = 0.0f;
  rawPositionHistoryIndex = 0;
  rawPositionHistoryCount = 0;
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
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
  dominantSensor = 0;
}

void updateAutoPolarity(const float deviations[HALL_COUNT]) {
  if (cal.polarity != 0) return;

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
    cal.polarity = 1;
    calibrationDirty = true;
    Serial.println("POL auto locked RAW_RISE.");
  } else if (polarityScore <= -4) {
    cal.polarity = -1;
    calibrationDirty = true;
    Serial.println("POL auto locked RAW_DROP.");
  }
}

void updateBaselineWhenIdle(const float deviations[HALL_COUNT]) {
  const bool idleCandidate = !signalReliable &&
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
    baseline[i] += deviations[i] * BASELINE_TRACK_ALPHA;
    cal.baseline[i] = baseline[i];
  }
  cal.baselineValid = 1;
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
    weighted += SENSOR_POS[i] * weight;
    total += weight;
  }
  if (total <= 0.0f) {
    return filteredPosition;
  }
  return clampValue<float>(weighted / total, -1.0f, 1.0f);
}

float rightEdgePosition(float centroid) {
  const float h1 = positionSignal[0];
  const float h2 = positionSignal[1];
  if (centroid < ENDPOINT_BOOST_POSITION_START ||
      h1 < ENDPOINT_BOOST_MIN_STRONGEST ||
      totalSignal < ENDPOINT_BOOST_MIN_TOTAL ||
      h1 < safeMax(8.0f, h2) * ENDPOINT_BOOST_DOMINANCE) {
    return centroid;
  }

  const float ratio = h2 / safeMax(1.0f, h1);
  if (ratio >= ENDPOINT_BOOST_RATIO_START) {
    return centroid;
  }

  const float ratioAmount = smoothStep01((ENDPOINT_BOOST_RATIO_START - ratio) /
                                         (ENDPOINT_BOOST_RATIO_START - ENDPOINT_BOOST_RATIO_FULL));
  const float positionAmount = smoothStep01((centroid - ENDPOINT_BOOST_POSITION_START) /
                                            (1.0f - ENDPOINT_BOOST_POSITION_START));
  const float correction = ratioAmount * positionAmount * ENDPOINT_BOOST_MAX;
  const float edgePosition = centroid + (1.0f - centroid) * correction;
  return clampValue<float>(safeMax(centroid, edgePosition), -1.0f, 1.0f);
}

float leftEdgePosition(float centroid) {
  const float h4 = positionSignal[3];
  const float h3 = positionSignal[2];
  if (centroid > -ENDPOINT_BOOST_POSITION_START ||
      h4 < ENDPOINT_BOOST_MIN_STRONGEST ||
      totalSignal < ENDPOINT_BOOST_MIN_TOTAL ||
      h4 < safeMax(8.0f, h3) * ENDPOINT_BOOST_DOMINANCE) {
    return centroid;
  }

  const float ratio = h3 / safeMax(1.0f, h4);
  if (ratio >= ENDPOINT_BOOST_RATIO_START) {
    return centroid;
  }

  const float ratioAmount = smoothStep01((ENDPOINT_BOOST_RATIO_START - ratio) /
                                         (ENDPOINT_BOOST_RATIO_START - ENDPOINT_BOOST_RATIO_FULL));
  const float positionAmount = smoothStep01((fabsf(centroid) - ENDPOINT_BOOST_POSITION_START) /
                                            (1.0f - ENDPOINT_BOOST_POSITION_START));
  const float correction = ratioAmount * positionAmount * ENDPOINT_BOOST_MAX;
  const float edgePosition = centroid + (-1.0f - centroid) * correction;
  return clampValue<float>(safeMin(centroid, edgePosition), -1.0f, 1.0f);
}

float applyCalibrationShape(float position) {
  float shaped = position + cal.centerOffset;
  if (cal.centerWarp != 0.0f) {
    shaped += cal.centerWarp * sinf(3.0f * PI_F * shaped);
  }
  return clampValue<float>(shaped, -1.0f, 1.0f);
}

float estimateSliderPosition() {
  centroidPosition = plainCentroidPosition();
  float position = centroidPosition;
  position = applyCalibrationShape(position);

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
  float shaped = position + SENSOR_CENTER_EQUALIZE_WARP * sinf(3.0f * PI_F * position);
  const float absPosition = fabsf(position);
  if (absPosition > ENDPOINT_OUTPUT_STRETCH_START) {
    const float edgeAmount = smoothStep01((absPosition - ENDPOINT_OUTPUT_STRETCH_START) /
                                          (1.0f - ENDPOINT_OUTPUT_STRETCH_START));
    const float sign = position >= 0.0f ? 1.0f : -1.0f;
    shaped += (sign - shaped) * edgeAmount * ENDPOINT_OUTPUT_STRETCH_MAX;
  }
  return clampValue<float>(shaped, -1.0f, 1.0f);
}

void learnStillNoise(bool outputStill) {
  if (!outputStill || fabsf(mouseRemainder) > EDGE_OUTPUT_STILL_RELEASE_PIXELS || lastDx != 0) {
    return;
  }
  learnedVectorNoise += (sensorVectorMotion - learnedVectorNoise) * 0.015f;
  learnedPositionNoise += (fabsf(lastPositionDelta) - learnedPositionNoise) * 0.015f;
  learnedVectorDelta = clampValue<float>(safeMax(cal.stillVectorDelta, learnedVectorNoise * 4.0f + 0.35f), 2.8f, 8.0f);
  learnedPositionDelta = clampValue<float>(safeMax(cal.stillPositionDelta, learnedPositionNoise * 4.0f + 0.0015f),
                                           0.006f,
                                           0.030f);
}

void sampleHall() {
  float deviations[HALL_COUNT] = {};
  rawAbsTotal = 0.0f;
  rawAbsStrongest = 0.0f;
  totalSignal = 0.0f;
  strongestSignal = 0.0f;
  dominantSensor = 0;

  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    rawValues[i] = readAverage(HALL_PINS[i]);
    deviations[i] = static_cast<float>(rawValues[i]) - baseline[i];
    const float absDev = fabsf(deviations[i]);
    rawAbsTotal += absDev;
    rawAbsStrongest = safeMax(rawAbsStrongest, absDev);
  }

  updateAutoPolarity(deviations);

  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    float field = 0.0f;
    if (cal.polarity == 0) {
      field = fabsf(deviations[i]);
    } else {
      field = static_cast<float>(cal.polarity) * deviations[i];
      if (field < 0.0f) field = 0.0f;
    }
    rawActivation[i] = field > ACTIVATE_DEADBAND ? field - ACTIVATE_DEADBAND : 0.0f;
    activation[i] = rawActivation[i] * cal.sensorGain[i];
    positionSignal[i] += (activation[i] - positionSignal[i]) * ACTIVATION_SIGNAL_SMOOTHING;

    totalSignal += positionSignal[i];
    if (positionSignal[i] > strongestSignal) {
      strongestSignal = positionSignal[i];
      dominantSensor = i + 1;
    }
  }

  sensorVectorMotion = 0.0f;
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    sensorVectorMotion += fabsf(positionSignal[i] - previousPositionSignal[i]);
    previousPositionSignal[i] = positionSignal[i];
  }

  if (signalReliable) {
    signalReliable = totalSignal >= TRACK_EXIT_STRENGTH;
  } else {
    signalReliable = totalSignal >= TRACK_ENTER_STRENGTH && strongestSignal >= TRACK_ENTER_STRONGEST;
    if (signalReliable) {
      rawPositionHistoryIndex = 0;
      rawPositionHistoryCount = 0;
      virtualMousePosition = mousePixelsForPosition(filteredPosition);
      mouseRemainder = 0.0f;
    }
  }

  strengthTooLow = totalSignal < MIN_USABLE_STRENGTH || !signalReliable;
  strengthTooHigh = strongestSignal > TOO_CLOSE_STRONGEST || totalSignal > TOO_CLOSE_TOTAL;

  updateBaselineWhenIdle(deviations);

  if (!signalReliable) {
    candidatePosition = filteredPosition;
    centroidPosition = filteredPosition;
    medianInputPosition = filteredPosition;
    lastPositionDelta = 0.0f;
    return;
  }

  candidatePosition = estimateSliderPosition();
  medianInputPosition = pushPositionMedian(candidatePosition);
  previousFilteredPosition = filteredPosition;
  const float delta = medianInputPosition - filteredPosition;
  const float alpha = adaptivePositionAlpha(delta);
  filteredPosition += delta * alpha;
  filteredPosition = clampValue<float>(filteredPosition, -1.0f, 1.0f);
  lastPositionDelta = filteredPosition - previousFilteredPosition;
}

float mousePixelsForPosition(float position) {
  return equalizeSensorCenterPosition(position) * MOUSE_GAIN;
}

bool edgeRegionActive() {
  return fabsf(filteredPosition) >= 0.86f || dominantSensor == 1 || dominantSensor == 4;
}

float effectiveStillVectorDelta() {
  const float base = edgeRegionActive() ? safeMax(EDGE_STILL_VECTOR_DELTA, learnedVectorDelta) : learnedVectorDelta;
  return clampValue<float>(base, 2.8f, 8.0f);
}

float effectiveStillPositionDelta() {
  const float base = edgeRegionActive() ? safeMax(EDGE_STILL_POSITION_DELTA, learnedPositionDelta) : learnedPositionDelta;
  return clampValue<float>(base, 0.006f, 0.030f);
}

void updateMouse() {
  lastDx = 0;
  const uint32_t now = millis();
  if (now - lastMouseReportMs < MOUSE_REPORT_INTERVAL_MS) {
    return;
  }
  lastMouseReportMs = now;

  const float currentPixels = mousePixelsForPosition(filteredPosition);
  if (strengthTooLow) {
    trackingActive = false;
    mouseOutputReleased = false;
    virtualMousePosition = currentPixels;
    mouseRemainder = 0.0f;
    smallDeltaSign = 0;
    smallDeltaStreak = 0;
    return;
  }

  if (!trackingActive) {
    trackingActive = true;
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

  lastDx = static_cast<int8_t>(outputDelta);
  if (usbHid.ready() && lastDx != 0) {
    usbHid.mouseMove(1, lastDx, 0);
    virtualMousePosition += lastDx;
  }
  mouseRemainder = currentPixels - virtualMousePosition;
}

void startEndpointScan() {
  endpointScan.active = true;
  endpointScan.startedMs = millis();
  endpointScan.centerSumMax = 0.0f;
  endpointScan.leftBestRatio = 999.0f;
  endpointScan.rightBestRatio = 999.0f;
  endpointScan.leftBestSum = 0.0f;
  endpointScan.rightBestSum = 0.0f;
  endpointScan.leftBestStrongest = 0.0f;
  endpointScan.rightBestStrongest = 0.0f;
  Serial.println("CAL endpoint scan started. Slowly sweep to both mechanical endpoints for 8 seconds.");
}

void finishEndpointScan() {
  endpointScan.active = false;
  bool updated = false;

  if (endpointScan.centerSumMax >= 30.0f) {
    cal.centerSumRef = endpointScan.centerSumMax;
    updated = true;
  }
  if (endpointScan.leftBestRatio < 1.0f && endpointScan.leftBestSum >= 20.0f) {
    cal.leftEndpointRatio = clampValue<float>(endpointScan.leftBestRatio, 0.02f, 0.32f);
    cal.leftEndpointSum = endpointScan.leftBestSum;
    cal.leftEndpointGainMax = clampValue<float>(cal.centerSumRef / safeMax(18.0f, cal.leftEndpointSum), 1.0f, 1.65f);
    updated = true;
  }
  if (endpointScan.rightBestRatio < 1.0f && endpointScan.rightBestSum >= 20.0f) {
    cal.rightEndpointRatio = clampValue<float>(endpointScan.rightBestRatio, 0.02f, 0.32f);
    cal.rightEndpointSum = endpointScan.rightBestSum;
    cal.rightEndpointGainMax = clampValue<float>(cal.centerSumRef / safeMax(18.0f, cal.rightEndpointSum), 1.0f, 1.65f);
    updated = true;
  }
  if (updated) {
    cal.endpointSignalRef = (cal.leftEndpointSum + cal.rightEndpointSum) * 0.5f;
    calibrationDirty = true;
    saveCalibration();
  }

  Serial.print("CAL endpoint scan done. L ratio/sum ");
  Serial.print(cal.leftEndpointRatio, 3);
  Serial.print(" / ");
  Serial.print(cal.leftEndpointSum, 1);
  Serial.print("    R ratio/sum ");
  Serial.print(cal.rightEndpointRatio, 3);
  Serial.print(" / ");
  Serial.println(cal.rightEndpointSum, 1);
}

void serviceEndpointScan() {
  if (!endpointScan.active) return;

  if (signalReliable) {
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

void captureSensorGainSample() {
  if (!signalReliable || strongestSignal < 20.0f || dominantSensor == 0) {
    Serial.println("GAIN sample ignored: place standard magnet over one sensor.");
    return;
  }
  const uint8_t idx = dominantSensor - 1;
  sensorGainSamples[idx] = safeMax(sensorGainSamples[idx], rawActivation[idx]);
  cal.sensorReference[idx] = sensorGainSamples[idx];

  bool allSeen = true;
  float sum = 0.0f;
  uint8_t count = 0;
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    if (sensorGainSamples[i] <= 0.0f && cal.sensorReference[i] > 0.0f) {
      sensorGainSamples[i] = cal.sensorReference[i];
    }
    if (sensorGainSamples[i] <= 0.0f) {
      allSeen = false;
    } else {
      sum += sensorGainSamples[i];
      ++count;
    }
  }

  if (count > 0) {
    const float reference = allSeen ? (sum / HALL_COUNT) : FACTORY_SENSOR_REFERENCE;
    for (uint8_t i = 0; i < HALL_COUNT; ++i) {
      if (sensorGainSamples[i] > 0.0f) {
        cal.sensorGain[i] = clampValue<float>(reference / sensorGainSamples[i], 0.55f, 1.85f);
      }
    }
    calibrationDirty = true;
  }

  Serial.print("GAIN sampled H");
  Serial.print(dominantSensor);
  Serial.print(" rawAct ");
  Serial.print(rawActivation[idx], 1);
  Serial.print(" gain ");
  Serial.println(cal.sensorGain[idx], 3);
  if (allSeen) {
    saveCalibration();
  }
}

void captureCenterCalibration() {
  if (!signalReliable) {
    Serial.println("CENTER ignored: no reliable magnet signal.");
    return;
  }
  cal.centerOffset = clampValue<float>(cal.centerOffset - filteredPosition, -0.12f, 0.12f);
  cal.centerSumRef = safeMax(cal.centerSumRef, totalSignal);
  calibrationDirty = true;
  saveCalibration();
  Serial.print("CENTER captured. Offset now ");
  Serial.println(cal.centerOffset, 5);
}

void enterBootloaderMode() {
  if (Serial) {
    Serial.println("Entering UF2 bootloader...");
    Serial.flush();
  }
  delay(50);

  #if HAS_PICO_BOOTROM
  reset_usb_boot(0, 0);
  #else
  if (Serial) {
    Serial.println("Bootloader jump is not available in this RP2040 core.");
  }
  #endif
}

void serviceBootloaderShortcut() {
  const bool held = digitalRead(BUTTON_PINS[BOOTLOADER_BUTTON_A]) == LOW &&
                    digitalRead(BUTTON_PINS[BOOTLOADER_BUTTON_B]) == LOW;
  if (!held) {
    bootloaderShortcutStartedMs = 0;
    return;
  }

  const uint32_t now = millis();
  if (bootloaderShortcutStartedMs == 0) {
    bootloaderShortcutStartedMs = now;
    return;
  }
  if (now - bootloaderShortcutStartedMs >= BOOTLOADER_HOLD_MS) {
    enterBootloaderMode();
  }
}

void printHelp() {
  Serial.println("Commands:");
  Serial.println("  ? print help");
  Serial.println("  Z capture no-magnet baseline and save");
  Serial.println("  A set polarity to auto");
  Serial.println("  O flip/force polarity");
  Serial.println("  E start 8s endpoint scan");
  Serial.println("  G capture standard magnet sensor gain sample");
  Serial.println("  C capture physical center offset");
  Serial.println("  S save current calibration");
  Serial.println("  D reset calibration defaults");
  Serial.println("  T toggle continuous telemetry");
  Serial.println("  P print calibration");
  Serial.println("  B enter UF2 bootloader");
}

void serviceSerial() {
  while (Serial.available()) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '?') {
      printHelp();
    } else if (ch == 'z' || ch == 'Z') {
      resetRuntimeTracking();
      captureBaseline();
      saveCalibration();
      Serial.println("ZERO baseline captured.");
    } else if (ch == 'a' || ch == 'A') {
      cal.polarity = 0;
      polarityScore = 0;
      calibrationDirty = true;
      Serial.println("POL set to AUTO.");
    } else if (ch == 'o' || ch == 'O') {
      if (cal.polarity == 0) cal.polarity = -1;
      else cal.polarity = static_cast<int8_t>(-cal.polarity);
      polarityScore = cal.polarity > 0 ? 4 : -4;
      calibrationDirty = true;
      Serial.print("POL forced to ");
      Serial.println(cal.polarity > 0 ? "RAW_RISE" : "RAW_DROP");
    } else if (ch == 'e' || ch == 'E') {
      startEndpointScan();
    } else if (ch == 'g' || ch == 'G') {
      captureSensorGainSample();
    } else if (ch == 'c' || ch == 'C') {
      captureCenterCalibration();
    } else if (ch == 's' || ch == 'S') {
      saveCalibration();
    } else if (ch == 'd' || ch == 'D') {
      setDefaultCalibration();
      for (uint8_t i = 0; i < HALL_COUNT; ++i) {
        baseline[i] = cal.baseline[i];
        sensorGainSamples[i] = 0.0f;
      }
      calibrationDirty = true;
      saveCalibration();
      resetRuntimeTracking();
      Serial.println("CAL defaults restored.");
    } else if (ch == 't' || ch == 'T') {
      telemetryAlways = !telemetryAlways;
      Serial.print("Telemetry ");
      Serial.println(telemetryAlways ? "ON" : "OFF");
    } else if (ch == 'p' || ch == 'P') {
      printCalibration();
    } else if (ch == 'b' || ch == 'B') {
      enterBootloaderMode();
    }
  }
}

uint8_t statusCode() {
  if (strengthTooLow) return 1;
  if (strengthTooHigh) return 2;
  return 0;
}

void printStatus() {
  const uint32_t now = millis();
  if (now - lastPrintMs < PRINT_INTERVAL_MS) {
    return;
  }

  const uint8_t state = statusCode();
  const bool shouldPrint =
    telemetryAlways ||
    !hasPrintedStatus ||
    lastDx != 0 ||
    fabsf(filteredPosition - lastPrintedPosition) >= PRINT_POSITION_DELTA ||
    fabsf(mouseRemainder - lastPrintedRemainder) >= PRINT_REMAINDER_DELTA ||
    endpointScan.active;
  if (!shouldPrint) {
    return;
  }

  Serial.print("RAW ");
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    if (i) Serial.print(" | ");
    Serial.print(rawValues[i]);
  }

  Serial.print("    ACT ");
  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    if (i) Serial.print(" | ");
    Serial.print(activation[i], 1);
  }

  const float rightRatio = positionSignal[1] / safeMax(1.0f, positionSignal[0]);
  const float leftRatio = positionSignal[2] / safeMax(1.0f, positionSignal[3]);

  Serial.print("    POS ");
  Serial.print(filteredPosition, 3);
  Serial.print("    CAND ");
  Serial.print(candidatePosition, 3);
  Serial.print("    CENT ");
  Serial.print(centroidPosition, 3);
  Serial.print("    DX ");
  Serial.print(lastDx);
  Serial.print("    REM ");
  Serial.print(mouseRemainder, 2);
  Serial.print("    SUM ");
  Serial.print(totalSignal, 1);
  Serial.print("    VEC ");
  Serial.print(sensorVectorMotion, 2);
  Serial.print("    RATIO L/R ");
  Serial.print(leftRatio, 3);
  Serial.print(" / ");
  Serial.print(rightRatio, 3);
  Serial.print("    POL ");
  Serial.print(cal.polarity > 0 ? "+" : (cal.polarity < 0 ? "-" : "A"));
  Serial.print("    DOM H");
  Serial.print(dominantSensor);
  Serial.print("    STATE ");
  if (strengthTooLow) {
    Serial.println("TOO_FAR");
  } else if (strengthTooHigh) {
    Serial.println("TOO_CLOSE");
  } else {
    Serial.println("GOOD");
  }

  lastPrintMs = now;
  hasPrintedStatus = true;
  lastPrintedPosition = filteredPosition;
  lastPrintedRemainder = mouseRemainder;
  (void)state;
}

void setupUsb() {
  if (!TinyUSBDevice.isInitialized()) {
    TinyUSBDevice.setProductDescriptor("Hall Edge Calibrated Test");
    TinyUSBDevice.begin(0);
  }

  usbHid.setPollInterval(1);
  usbHid.setReportDescriptor(HID_REPORT_DESCRIPTOR, sizeof(HID_REPORT_DESCRIPTOR));
  usbHid.setStringDescriptor("Hall Edge Mouse");
  usbHid.begin();

  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
}

void setup() {
  setupUsb();
  Serial.begin(115200);
  analogReadResolution(12);
  EEPROM.begin(EEPROM_SIZE);

  for (uint8_t i = 0; i < HALL_COUNT; ++i) {
    pinMode(HALL_PINS[i], INPUT);
  }
  for (uint8_t i = 0; i < 8; ++i) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  }

  const bool loaded = loadCalibration();
  if (loaded && cal.baselineValid) {
    for (uint8_t i = 0; i < HALL_COUNT; ++i) {
      baseline[i] = cal.baseline[i];
      sensorGainSamples[i] = cal.sensorReference[i];
    }
  } else {
    delay(700);
    captureBaseline();
  }
  resetRuntimeTracking();

  Serial.println();
  Serial.println("=== Hall Edge Calibrated Test ===");
  Serial.print("Firmware: ");
  Serial.println(FIRMWARE_BUILD);
  Serial.println(loaded ? "Calibration loaded." : "Calibration defaults active.");
  printCalibration();
}

void loop() {
  #ifdef TINYUSB_NEED_POLLING_TASK
  TinyUSBDevice.task();
  #endif

  sampleHall();
  serviceEndpointScan();
  updateMouse();
  printStatus();
  serviceSerial();
  serviceBootloaderShortcut();
}
