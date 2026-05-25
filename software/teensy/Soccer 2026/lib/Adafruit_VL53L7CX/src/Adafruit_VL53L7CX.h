/*!
 * @file Adafruit_VL53L7CX.h
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

#ifndef ADAFRUIT_VL53L7CX_H
#define ADAFRUIT_VL53L7CX_H

#include <Adafruit_BusIO_Register.h>
#include <Adafruit_I2CDevice.h>
#include <Arduino.h>
#include <Wire.h>

#include "vl53l7cx_api.h"
#include "vl53l7cx_plugin_detection_thresholds.h"
#include "vl53l7cx_plugin_motion_indicator.h"
#include "vl53l7cx_plugin_xtalk.h"

#define VL53L7CX_DEFAULT_ADDRESS 0x29 ///< Default I2C address (7-bit)

/*!
 * @brief Class for VL53L7CX ToF sensor
 */
class Adafruit_VL53L7CX {
 public:
  Adafruit_VL53L7CX();
  ~Adafruit_VL53L7CX();

  bool begin(uint8_t address = VL53L7CX_DEFAULT_ADDRESS, TwoWire* wire = &Wire,
             uint32_t i2c_clock = 400000);

  bool startRanging(void);
  bool stopRanging(void);

  bool isDataReady(void);
  bool getRangingData(VL53L7CX_ResultsData* results);

  bool setResolution(uint8_t resolution);
  uint8_t getResolution(void);

  bool setRangingFrequency(uint8_t frequency_hz);
  uint8_t getRangingFrequency(void);

  bool setIntegrationTime(uint32_t time_ms);
  uint32_t getIntegrationTime(void);

  bool setSharpenerPercent(uint8_t percent);
  uint8_t getSharpenerPercent(void);

  bool setTargetOrder(uint8_t order);
  uint8_t getTargetOrder(void);

  bool setRangingMode(uint8_t mode);
  uint8_t getRangingMode(void);

  bool setPowerMode(uint8_t mode);
  uint8_t getPowerMode(void);

  bool setAddress(uint8_t new_address);

  // Detection Thresholds
  bool setDetectionThresholdsEnable(bool enable);
  bool getDetectionThresholdsEnable(void);
  bool setDetectionThresholds(VL53L7CX_DetectionThresholds* thresholds);
  bool getDetectionThresholds(VL53L7CX_DetectionThresholds* thresholds);

  // Motion Indicator
  bool initMotionIndicator(uint8_t resolution);
  bool setMotionDistance(uint16_t min_mm, uint16_t max_mm);
  bool setMotionResolution(uint8_t resolution);

  // Xtalk Calibration
  bool calibrateXtalk(uint16_t reflectance_percent, uint8_t nb_samples,
                      uint16_t distance_mm);
  bool getXtalkCalData(uint8_t* data);
  bool setXtalkCalData(uint8_t* data);
  bool getXtalkMargin(uint32_t* margin);
  bool setXtalkMargin(uint32_t margin);

  VL53L7CX_Configuration* getConfig(void) {
    return &_config;
  }

 private:
  Adafruit_I2CDevice* _i2c_dev = nullptr;
  VL53L7CX_Configuration _config;
  bool _initialized = false;
  VL53L7CX_Motion_Configuration _motion_config;
};

#endif // ADAFRUIT_VL53L7CX_H
