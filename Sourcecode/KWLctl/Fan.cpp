/*
 * Copyright (C) 2018 Ivan Schréter (schreter@gmx.net)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This copyright notice MUST APPEAR in all copies of the software!
 */

#include "Fan.h"
#include "MessageHandler.h"
#include "MQTTTopic.hpp"

#include <Wire.h>

// Define the aggressive and conservative Tuning Parameters
// Nenndrehzahl Lüfter 3200, Stellwert 0..1000 entspricht 0-10V
static constexpr double aggKp  = 0.5,  aggKi = 0.1, aggKd  = 0.001;
static constexpr double consKp = 0.1, consKi = 0.1, consKd = 0.001;


Fan::Fan(uint8_t id, uint8_t powerPin, uint8_t pwmPin, uint8_t tachoPin) :
  power_(powerPin),
  pwm_pin_(pwmPin),
  tacho_pin_(tachoPin),
  fan_id_(id),
  pid_(&current_speed_, &tech_setpoint_, &speed_setpoint_, consKp, consKi, consKd, P_ON_M, DIRECT)
{}

void Fan::begin(void (*countUp)(), unsigned standardSpeed)
{
  standard_speed_ = standardSpeed;

  pid_.SetOutputLimits(0, 1000);
  pid_.SetMode(AUTOMATIC);
  pid_.SetSampleTime(1000); // TODO use constant slightly less than in fan interval?

  // Lüfter Speed
  pinMode(pwm_pin_, OUTPUT);
  digitalWrite(pwm_pin_, LOW);

  // Lüfter Tacho Interrupt
  pinMode(tacho_pin_, INPUT_PULLUP);
  auto intr = uint8_t(digitalPinToInterrupt(tacho_pin_));
  attachInterrupt(intr, countUp, KWLConfig::TachoSamplingMode);

  Serial.print(F("Fan pins(tacho/PWM), interrupt, std speed:\t"));
  Serial.print(tacho_pin_);
  Serial.print('\t');
  Serial.print(pwm_pin_);
  Serial.print('\t');
  Serial.print(intr);
  Serial.print('\t');
  Serial.println(standardSpeed);

  // Turn on power
  power_.on();
}

void Fan::computeSpeed(int ventMode, FanCalculateSpeedMode calcMode)
{
  speed_setpoint_ = standard_speed_ * KWLConfig::StandardKwlModeFactor[ventMode];

  if (ventMode == 0) {
    tech_setpoint_ = 0 ;  // Lüfungsstufe 0 alles ausschalten
    return;
  }

  double gap = abs(speed_setpoint_ - current_speed_); //distance away from setpoint

  // Das PWM-Signal kann entweder per PID-Regler oder unten per Dreisatz berechnen werden.
  // TODO above comment seems invalid now
  if (calcMode == FanCalculateSpeedMode::PID) {
    if (gap < 1000) {
      pid_.SetTunings(consKp, consKi, consKd);
    } else {
      pid_.SetTunings(aggKp, aggKi, aggKd);
    }
    pid_.Compute();
  } else if (calcMode == FanCalculateSpeedMode::PROP) {
    tech_setpoint_ = pwm_setpoint_[ventMode];
  }

  // Grenzwertbehandlung: Max- / Min-Werte
  if (tech_setpoint_ < 0)
    tech_setpoint_ = 0;
  if (tech_setpoint_ > 1000)
    tech_setpoint_ = 1000;
}

void Fan::setSpeed(int id, uint8_t pwmPin, uint8_t dacChannel)
{
  if (KWLConfig::serialDebugFan) {
    Serial.print(F("Fan "));
    Serial.print(id);
    Serial.print(F(": \tgap: "));
    Serial.print(current_speed_ - speed_setpoint_);
    Serial.print(F("\tspeedTacho: "));
    Serial.print(current_speed_);
    Serial.print(F("\ttechSetpoint: "));
    Serial.print(tech_setpoint_);
    Serial.print(F("\tspeedSetpoint: "));
    Serial.println(speed_setpoint_);
    rpm_.dump();
  }

  // Setzen per PWM
  // Ausgabewert für Lüftersteuerung darf zwischen 0-10V liegen, dies entspricht 0..1023, vereinfacht 0..1000
  // 0..1000 muss umgerechnet werden auf 0..255 also durch 4 geteilt werden
  // max. Lüfterdrehzahl bei Papstlüfter 3200 U/min
  int tech = int(tech_setpoint_);
  analogWrite(pwmPin, tech / 4);

  // Setzen der Werte per DAC
  if (KWLConfig::ControlFansDAC) {
    byte HBy;
    byte LBy;

    HBy = byte(tech >> 8);   //HIGH-Byte berechnen
    LBy = byte(tech & 255);  //LOW-Byte berechnen
    Wire.beginTransmission(KWLConfig::DacI2COutAddr); // Start Übertragung zur ANALOG-OUT Karte
    Wire.write(dacChannel);                   // Fan channel schreiben
    Wire.write(LBy);                          // LOW-Byte schreiben
    Wire.write(HBy);                          // HIGH-Byte schreiben
    Wire.endTransmission();                   // Ende
  }
}

void Fan::debugSet(int ventMode, int techSetpoint) {
  if (techSetpoint < 0)
    techSetpoint = 0;
  else if (techSetpoint > 1000)
    techSetpoint = 1000;
  pwm_setpoint_[ventMode] = techSetpoint;
}

bool Fan::speedCalibrationStep(int mode)
{
  if (abs(KWLConfig::StandardKwlModeFactor[mode]) < 0.01) {
    // Faktor Null ist einfach
    calibration_pwm_setpoint_[mode] = 0;
    return true;
  } else {
    // Faktor ungleich 0
    speed_setpoint_ = standard_speed_ * KWLConfig::StandardKwlModeFactor[mode];

    double gap = abs(speed_setpoint_ - current_speed_); //distance away from setpoint
    if ((gap < 10) && (good_pwm_setpoint_count_ < REQUIRED_GOOD_PWM_COUNT)) {
      // einen PWM Wert gefunden
      good_pwm_setpoint_[good_pwm_setpoint_count_] = int(tech_setpoint_);
      good_pwm_setpoint_count_++;
    }
    if (good_pwm_setpoint_count_ >= REQUIRED_GOOD_PWM_COUNT) {
      // fertig, genug Werte gefunden, jetzt Durchschnitt bilden
      long sum_pwm = 0;
      for (unsigned i = 0; i < REQUIRED_GOOD_PWM_COUNT; i++) {
        sum_pwm += good_pwm_setpoint_[i];
      }
      calibration_pwm_setpoint_[mode] = int(sum_pwm / REQUIRED_GOOD_PWM_COUNT);
      return true;
    }

    // Noch nicht genug Werte, PID Regler muss nachregeln
    if (gap < 1000)
      pid_.SetTunings(consKp, consKi, consKd);
    else
      pid_.SetTunings(aggKp, aggKi, aggKd);
    pid_.Compute();

    // !Kein PreHeating und keine Sicherheitsabfrage Temperatur
    return false;
  }
}

void Fan::finishCalibration()
{
  for (unsigned i = 0; (i < KWLConfig::StandardModeCnt) && (i < MAX_FAN_MODE_CNT); ++i)
    pwm_setpoint_[i] = calibration_pwm_setpoint_[i];
}

void Fan::sendMQTTDebug(int id, unsigned long ts, MessageHandler& h)
{
  if (!mqtt_send_debug_)
    return;

  char buffer[100];
  snprintf(buffer, sizeof(buffer), "Fan%d - M: %lu, gap: %ld, tsf: %ld, ssf: %ld, rpm: %ld",
           id, ts,
           long(current_speed_ - speed_setpoint_),
           long(tech_setpoint_), long(speed_setpoint_), long(current_speed_));
  h.publish((id == 1) ? MQTTTopic::KwlDebugstateFan1 : MQTTTopic::KwlDebugstateFan2, buffer, false);
}

