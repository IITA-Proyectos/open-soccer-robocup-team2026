/*!
 * @file Adafruit_VL53L7CX.cpp
 *
 * Arduino library for the ST VL53L7CX 8x8 Time-of-Flight sensor
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 * Written by Limor 'ladyada' Fried with assistance from Claude Code
 *
 * MIT license, all text above must be included in any redistribution
 */

#include "Adafruit_VL53L7CX.h"

/*!
 * @brief Constructor
 */
Adafruit_VL53L7CX::Adafruit_VL53L7CX() {}

/*!
 * @brief Destructor
 */
Adafruit_VL53L7CX::~Adafruit_VL53L7CX() {
  if (_i2c_dev) {
    delete _i2c_dev;
  }
}

/*!
 * @brief Initialize the sensor
 * @param address I2C address (default 0x29)
 * @param wire Pointer to Wire instance
 * @param i2c_clock I2C clock speed in Hz (default 400000)
 * @return true on success, false on failure
 */
bool Adafruit_VL53L7CX::begin(uint8_t address, TwoWire* wire,
                              uint32_t i2c_clock) {
  if (_i2c_dev) {
    delete _i2c_dev;
  }

  _i2c_dev = new Adafruit_I2CDevice(address, wire);
  if (!_i2c_dev->begin()) {
    return false;
  }

  wire->setClock(i2c_clock);

  // Set up the platform struct for ST driver
  _config.platform.address = address;
  _config.platform.i2c_dev = _i2c_dev;

  // Check sensor is alive
  uint8_t isAlive = 0;
  uint8_t status = vl53l7cx_is_alive(&_config, &isAlive);
  if (status != VL53L7CX_STATUS_OK || !isAlive) {
    return false;
  }

  // Initialize the sensor (loads firmware, takes ~10 seconds!)
  status = vl53l7cx_init(&_config);
  if (status != VL53L7CX_STATUS_OK) {
    return false;
  }

  _initialized = true;
  return true;
}

/*!
 * @brief Start ranging
 * @return true on success
 */
bool Adafruit_VL53L7CX::startRanging(void) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_start_ranging(&_config) == VL53L7CX_STATUS_OK);
}

/*!
 * @brief Stop ranging
 * @return true on success
 */
bool Adafruit_VL53L7CX::stopRanging(void) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_stop_ranging(&_config) == VL53L7CX_STATUS_OK);
}

/*!
 * @brief Check if new data is ready
 * @return true if data is ready to read
 */
bool Adafruit_VL53L7CX::isDataReady(void) {
  if (!_initialized) {
    return false;
  }
  uint8_t ready = 0;
  if (vl53l7cx_check_data_ready(&_config, &ready) != VL53L7CX_STATUS_OK) {
    return false;
  }
  return (ready != 0);
}

/*!
 * @brief Get ranging data from the sensor
 * @param results Pointer to results structure to fill
 * @return true on success
 */
bool Adafruit_VL53L7CX::getRangingData(VL53L7CX_ResultsData* results) {
  if (!_initialized || !results) {
    return false;
  }
  return (vl53l7cx_get_ranging_data(&_config, results) == VL53L7CX_STATUS_OK);
}

/*!
 * @brief Set resolution (16 for 4x4 or 64 for 8x8)
 * @param resolution 16 or 64
 * @return true on success
 */
bool Adafruit_VL53L7CX::setResolution(uint8_t resolution) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_set_resolution(&_config, resolution) == VL53L7CX_STATUS_OK);
}

/*!
 * @brief Get current resolution
 * @return Resolution (16 or 64), or 0 on error
 */
uint8_t Adafruit_VL53L7CX::getResolution(void) {
  if (!_initialized) {
    return 0;
  }
  uint8_t resolution = 0;
  if (vl53l7cx_get_resolution(&_config, &resolution) != VL53L7CX_STATUS_OK) {
    return 0;
  }
  return resolution;
}

/*!
 * @brief Set ranging frequency in Hz
 * @param frequency_hz Frequency (1-60 Hz depending on resolution)
 * @return true on success
 */
bool Adafruit_VL53L7CX::setRangingFrequency(uint8_t frequency_hz) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_set_ranging_frequency_hz(&_config, frequency_hz) ==
          VL53L7CX_STATUS_OK);
}

/*!
 * @brief Get current ranging frequency
 * @return Frequency in Hz, or 0 on error
 */
uint8_t Adafruit_VL53L7CX::getRangingFrequency(void) {
  if (!_initialized) {
    return 0;
  }
  uint8_t freq = 0;
  if (vl53l7cx_get_ranging_frequency_hz(&_config, &freq) !=
      VL53L7CX_STATUS_OK) {
    return 0;
  }
  return freq;
}

/*!
 * @brief Set integration time in milliseconds
 * @param time_ms Integration time (2-1000ms)
 * @return true on success
 */
bool Adafruit_VL53L7CX::setIntegrationTime(uint32_t time_ms) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_set_integration_time_ms(&_config, time_ms) ==
          VL53L7CX_STATUS_OK);
}

/*!
 * @brief Get current integration time
 * @return Integration time in ms, or 0 on error
 */
uint32_t Adafruit_VL53L7CX::getIntegrationTime(void) {
  if (!_initialized) {
    return 0;
  }
  uint32_t time_ms = 0;
  if (vl53l7cx_get_integration_time_ms(&_config, &time_ms) !=
      VL53L7CX_STATUS_OK) {
    return 0;
  }
  return time_ms;
}

/*!
 * @brief Set sharpener percentage (0=off, 1-99=amount)
 * @param percent Sharpener percentage
 * @return true on success
 */
bool Adafruit_VL53L7CX::setSharpenerPercent(uint8_t percent) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_set_sharpener_percent(&_config, percent) ==
          VL53L7CX_STATUS_OK);
}

/*!
 * @brief Get current sharpener percentage
 * @return Sharpener percent, or 0xFF on error
 */
uint8_t Adafruit_VL53L7CX::getSharpenerPercent(void) {
  if (!_initialized) {
    return 0xFF;
  }
  uint8_t percent = 0;
  if (vl53l7cx_get_sharpener_percent(&_config, &percent) !=
      VL53L7CX_STATUS_OK) {
    return 0xFF;
  }
  return percent;
}

/*!
 * @brief Set target order (closest or strongest first)
 * @param order VL53L7CX_TARGET_ORDER_CLOSEST (1) or
 *              VL53L7CX_TARGET_ORDER_STRONGEST (2)
 * @return true on success
 */
bool Adafruit_VL53L7CX::setTargetOrder(uint8_t order) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_set_target_order(&_config, order) == VL53L7CX_STATUS_OK);
}

/*!
 * @brief Get current target order
 * @return VL53L7CX_TARGET_ORDER_CLOSEST (1) or
 *         VL53L7CX_TARGET_ORDER_STRONGEST (2), or 0 on error
 */
uint8_t Adafruit_VL53L7CX::getTargetOrder(void) {
  if (!_initialized) {
    return 0;
  }
  uint8_t order = 0;
  if (vl53l7cx_get_target_order(&_config, &order) != VL53L7CX_STATUS_OK) {
    return 0;
  }
  return order;
}

/*!
 * @brief Set ranging mode (continuous or autonomous)
 * @param mode VL53L7CX_RANGING_MODE_CONTINUOUS (1) or
 *             VL53L7CX_RANGING_MODE_AUTONOMOUS (3)
 * @return true on success
 */
bool Adafruit_VL53L7CX::setRangingMode(uint8_t mode) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_set_ranging_mode(&_config, mode) == VL53L7CX_STATUS_OK);
}

/*!
 * @brief Get current ranging mode
 * @return VL53L7CX_RANGING_MODE_CONTINUOUS (1) or
 *         VL53L7CX_RANGING_MODE_AUTONOMOUS (3), or 0 on error
 */
uint8_t Adafruit_VL53L7CX::getRangingMode(void) {
  if (!_initialized) {
    return 0;
  }
  uint8_t mode = 0;
  if (vl53l7cx_get_ranging_mode(&_config, &mode) != VL53L7CX_STATUS_OK) {
    return 0;
  }
  return mode;
}

/*!
 * @brief Set power mode (sleep or wakeup)
 * @param mode VL53L7CX_POWER_MODE_SLEEP (0) or
 *             VL53L7CX_POWER_MODE_WAKEUP (1)
 * @return true on success
 */
bool Adafruit_VL53L7CX::setPowerMode(uint8_t mode) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_set_power_mode(&_config, mode) == VL53L7CX_STATUS_OK);
}

/*!
 * @brief Get current power mode
 * @return VL53L7CX_POWER_MODE_SLEEP (0) or
 *         VL53L7CX_POWER_MODE_WAKEUP (1), or 0xFF on error
 */
uint8_t Adafruit_VL53L7CX::getPowerMode(void) {
  if (!_initialized) {
    return 0xFF;
  }
  uint8_t mode = 0;
  if (vl53l7cx_get_power_mode(&_config, &mode) != VL53L7CX_STATUS_OK) {
    return 0xFF;
  }
  return mode;
}

/*!
 * @brief Change the I2C address of the sensor
 * @param new_address New 7-bit I2C address
 * @return true on success
 */
bool Adafruit_VL53L7CX::setAddress(uint8_t new_address) {
  if (!_initialized) {
    return false;
  }
  // Bypass vl53l7cx_set_i2c_address() — the ST driver updates
  // platform.address mid-sequence but our BusIO i2c_dev still
  // points to the old address, so the final page-select write fails.
  // Do the register writes directly with the OLD device first.
  uint8_t buf[3];
  // Set page 0
  buf[0] = 0x7F;
  buf[1] = 0xFF;
  buf[2] = 0x00;
  if (!_i2c_dev->write(buf, 3)) {
    return false;
  }
  // Write new 7-bit address to register 0x0004
  buf[0] = 0x00;
  buf[1] = 0x04;
  buf[2] = new_address;
  if (!_i2c_dev->write(buf, 3)) {
    return false;
  }
  // Sensor is now at new_address — switch our I2C device
  delete _i2c_dev;
  _i2c_dev = new Adafruit_I2CDevice(new_address, &Wire);
  _config.platform.address = new_address;
  _config.platform.i2c_dev = _i2c_dev;
  if (!_i2c_dev->begin()) {
    return false;
  }
  // Set page 2 at the new address
  buf[0] = 0x7F;
  buf[1] = 0xFF;
  buf[2] = 0x02;
  return _i2c_dev->write(buf, 3);
}

/**************************************************************************/
/*                      Detection Thresholds                              */
/**************************************************************************/

/*!
 * @brief Enable or disable detection thresholds
 * @param enable true to enable, false to disable
 * @return true on success
 */
bool Adafruit_VL53L7CX::setDetectionThresholdsEnable(bool enable) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_set_detection_thresholds_enable(&_config, enable ? 1 : 0) ==
          VL53L7CX_STATUS_OK);
}

/*!
 * @brief Check if detection thresholds are enabled
 * @return true if enabled, false if disabled or on error
 */
bool Adafruit_VL53L7CX::getDetectionThresholdsEnable(void) {
  if (!_initialized) {
    return false;
  }
  uint8_t enabled = 0;
  if (vl53l7cx_get_detection_thresholds_enable(&_config, &enabled) !=
      VL53L7CX_STATUS_OK) {
    return false;
  }
  return (enabled == 1);
}

/*!
 * @brief Set detection thresholds configuration
 * @param thresholds Pointer to array of 64 VL53L7CX_DetectionThresholds
 * @return true on success
 */
bool Adafruit_VL53L7CX::setDetectionThresholds(
    VL53L7CX_DetectionThresholds* thresholds) {
  if (!_initialized || !thresholds) {
    return false;
  }
  return (vl53l7cx_set_detection_thresholds(&_config, thresholds) ==
          VL53L7CX_STATUS_OK);
}

/*!
 * @brief Get detection thresholds configuration
 * @param thresholds Pointer to array of 64 VL53L7CX_DetectionThresholds to fill
 * @return true on success
 */
bool Adafruit_VL53L7CX::getDetectionThresholds(
    VL53L7CX_DetectionThresholds* thresholds) {
  if (!_initialized || !thresholds) {
    return false;
  }
  return (vl53l7cx_get_detection_thresholds(&_config, thresholds) ==
          VL53L7CX_STATUS_OK);
}

/**************************************************************************/
/*                        Motion Indicator                                */
/**************************************************************************/

/*!
 * @brief Initialize motion indicator with specified resolution
 * @param resolution VL53L7CX_RESOLUTION_4X4 (16) or VL53L7CX_RESOLUTION_8X8
 * (64)
 * @return true on success
 */
bool Adafruit_VL53L7CX::initMotionIndicator(uint8_t resolution) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_motion_indicator_init(&_config, &_motion_config,
                                         resolution) == VL53L7CX_STATUS_OK);
}

/*!
 * @brief Set motion indicator distance range
 * @param min_mm Minimum distance in mm (400-4000)
 * @param max_mm Maximum distance in mm (400-4000)
 * @return true on success
 */
bool Adafruit_VL53L7CX::setMotionDistance(uint16_t min_mm, uint16_t max_mm) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_motion_indicator_set_distance_motion(
              &_config, &_motion_config, min_mm, max_mm) == VL53L7CX_STATUS_OK);
}

/*!
 * @brief Set motion indicator resolution
 * @param resolution VL53L7CX_RESOLUTION_4X4 (16) or VL53L7CX_RESOLUTION_8X8
 * (64)
 * @return true on success
 */
bool Adafruit_VL53L7CX::setMotionResolution(uint8_t resolution) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_motion_indicator_set_resolution(
              &_config, &_motion_config, resolution) == VL53L7CX_STATUS_OK);
}

// --- Xtalk Calibration ---

/*!
 * @brief Run xtalk calibration. Requires a target at a known distance
 *   with known reflectance. Ranging must be stopped before calling.
 * @param reflectance_percent Target reflectance 1-99% (ST recommends 3%)
 * @param nb_samples Number of samples 1-16 (higher = more accurate)
 * @param distance_mm Target distance 600-3000mm
 * @return true on success
 */
bool Adafruit_VL53L7CX::calibrateXtalk(uint16_t reflectance_percent,
                                       uint8_t nb_samples,
                                       uint16_t distance_mm) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_calibrate_xtalk(&_config, reflectance_percent, nb_samples,
                                   distance_mm) == VL53L7CX_STATUS_OK);
}

/*!
 * @brief Get xtalk calibration data buffer (776 bytes)
 * @param data Buffer of VL53L7CX_XTALK_BUFFER_SIZE bytes
 * @return true on success
 */
bool Adafruit_VL53L7CX::getXtalkCalData(uint8_t* data) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_get_caldata_xtalk(&_config, data) == VL53L7CX_STATUS_OK);
}

/*!
 * @brief Set xtalk calibration data buffer (776 bytes)
 * @param data Buffer of VL53L7CX_XTALK_BUFFER_SIZE bytes
 * @return true on success
 */
bool Adafruit_VL53L7CX::setXtalkCalData(uint8_t* data) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_set_caldata_xtalk(&_config, data) == VL53L7CX_STATUS_OK);
}

/*!
 * @brief Get xtalk margin in kcps/spads (default 50)
 * @param margin Pointer to store margin value
 * @return true on success
 */
bool Adafruit_VL53L7CX::getXtalkMargin(uint32_t* margin) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_get_xtalk_margin(&_config, margin) == VL53L7CX_STATUS_OK);
}

/*!
 * @brief Set xtalk margin in kcps/spads (0-10000, default 50)
 * @param margin New margin value
 * @return true on success
 */
bool Adafruit_VL53L7CX::setXtalkMargin(uint32_t margin) {
  if (!_initialized) {
    return false;
  }
  return (vl53l7cx_set_xtalk_margin(&_config, margin) == VL53L7CX_STATUS_OK);
}
