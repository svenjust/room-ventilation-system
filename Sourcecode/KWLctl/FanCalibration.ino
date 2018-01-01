/* Die für bestimmte Drehzahl erforderlichen PWM Werte werden ermittelt und abgespeichert.
   Dazu müssen die verschiedenen Drehzahlen für jeden Lüfter per pid-Regler angefahren werden.


   Das Ganze muss ohne Delay funktionieren

*/
// Gesamte Kalibierung
boolean       CalibrationStartet      = false;
unsigned long CalibrationStartMillis  = 0;
unsigned long timeoutCalibration      = 600000;   // zehn Minuten, sollte in der Zeit die Kalibrierung nicht abgelossen werden können wird trotzdem beendet

int actFansCalculateSpeed = CalculateSpeed_PROP;

// Kalibrierung einer Stufe
boolean       CalibrationPwmStufeStartet      = false;
unsigned long CalibrationPwmStufeStartMillis  = 0;
unsigned long timeoutPwmStufeCalibration      = 300000;   // max 5 Minuten pro Stufe

int actKwlMode = 0;

int  tempPwmSetpointFan1[defStandardModeCnt];                                    // Speichert die pwm-Werte für die verschiedenen Drehzahlen
int  tempPwmSetpointFan2[defStandardModeCnt];

#define goodPwmsCnt 30
int  goodPwmsFan1[goodPwmsCnt];
int  goodPwmsFan2[goodPwmsCnt];
int  ActGoodPwmsFan1;
int  ActGoodPwmsFan2;


void SpeedCalibrationStart() {
  CalibrationPwmStufeStartet = false;
  CalibrationStartet = false;
  FanMode = FanMode_Calibration;
}



boolean SpeedCalibrationPwmStufe(int actKwlMode) {

  if (defStandardKwlModeFactor[actKwlMode] == 0) {
    // Faktor Null ist einfach
    tempPwmSetpointFan1[actKwlMode] = 0;
    tempPwmSetpointFan2[actKwlMode] = 0;
    return true;

  } else {
    // Faktor ungleich 0
    speedSetpointFan1 = StandardSpeedSetpointFan1 * defStandardKwlModeFactor[actKwlMode];
    speedSetpointFan2 = StandardSpeedSetpointFan2 * defStandardKwlModeFactor[actKwlMode];

    double gap1 = abs(speedSetpointFan1 - speedTachoFan1); //distance away from setpoint
    double gap2 = abs(speedSetpointFan2 - speedTachoFan2); //distance away from setpoint


    if ((gap1 < 10) && (ActGoodPwmsFan1 < goodPwmsCnt)) {
      // einen PWM Wert gefunden
      goodPwmsFan1[ActGoodPwmsFan1] = techSetpointFan1;
      ActGoodPwmsFan1++;
    }
    if ((gap2 < 10) && (ActGoodPwmsFan2 < goodPwmsCnt)) {
      // einen PWM Wert gefunden
      goodPwmsFan2[ActGoodPwmsFan2] = techSetpointFan2;
      ActGoodPwmsFan2++;
    }
    if ((ActGoodPwmsFan1 >= goodPwmsCnt) && (ActGoodPwmsFan2 >= goodPwmsCnt)) {
      //fertig, genug Werte gefunden
      // jetzt Durchschnitt bilden
      long SumPwmFan1 = 0;
      long SumPwmFan2 = 0;
      for (int i = 0; i < goodPwmsCnt; i++) {
        SumPwmFan1 += goodPwmsFan1[i];
        SumPwmFan2 += goodPwmsFan2[i];
      }
      tempPwmSetpointFan1[actKwlMode] = (int)(SumPwmFan1 / goodPwmsCnt);
      tempPwmSetpointFan2[actKwlMode] = (int)(SumPwmFan2 / goodPwmsCnt);

      return true;
    }
    // Noch nicht genug Werte, PID Regler muss nachregeln

    if (gap1 < 1000) {
      PidFan1.SetTunings(consKp, consKi, consKd);
    }  else   {
      PidFan1.SetTunings(aggKp, aggKi, aggKd);
    }
    if (gap2 < 1000) {
      PidFan2.SetTunings(consKp, consKi, consKd);
    }  else   {
      PidFan2.SetTunings(aggKp, aggKi, aggKd);
    }
    PidFan1.Compute();
    PidFan2.Compute();

    // !Kein PreHeating und keine Sicherheitsabfrage Temperatur

    if (mqttCmdSendAlwaysDebugFan1) {
      mqtt_debug_fan1();
    }
    if (mqttCmdSendAlwaysDebugFan2) {
      mqtt_debug_fan2();
    }

    if (1 == 1) {
      Serial.print ("Timestamp: ");
      Serial.println ((long)millis());
      Serial.print ("Fan 1: ");
      Serial.print ("\tGap: ");
      Serial.print (speedTachoFan1 - speedSetpointFan1);
      Serial.print ("\tspeedTachoFan1: ");
      Serial.print (speedTachoFan1);
      Serial.print ("\ttechSetpointFan1: ");
      Serial.print (techSetpointFan1);
      Serial.print ("\tspeedSetpointFan1: ");
      Serial.println(speedSetpointFan1);

      Serial.print ("Fan 2: ");
      Serial.print ("\tGap: ");
      Serial.print (speedTachoFan2 - speedSetpointFan2);
      Serial.print ("\tspeedTachoFan2: ");
      Serial.print (speedTachoFan2);
      Serial.print ("\ttechSetpointFan2: ");
      Serial.print (techSetpointFan2);
      Serial.print ("\tspeedSetpointFan2: ");
      Serial.println(speedSetpointFan2);
    }
    // Lüfter ansteuern
    analogWrite(pwmPinFan1, techSetpointFan1 / 4);
    analogWrite(pwmPinFan2, techSetpointFan2 / 4);
    
 // if (ControlFansDAC == 1) {
    // Setzen der Werte per DAC
    byte HBy;
    byte LBy;

    // FAN 1
    HBy = techSetpointFan1 / 256;        //HIGH-Byte berechnen
    LBy = techSetpointFan1 - HBy * 256;  //LOW-Byte berechnen
    Wire.beginTransmission(DAC_I2C_OUT_ADDR); // Start Übertragung zur ANALOG-OUT Karte
    Wire.write(DAC_CHANNEL_FAN1);             // FAN 1 schreiben
    Wire.write(LBy);                          // LOW-Byte schreiben
    Wire.write(HBy);                          // HIGH-Byte schreiben
    Wire.endTransmission();                   // Ende

    // FAN 2
    HBy = techSetpointFan2 / 256;        //HIGH-Byte berechnen
    LBy = techSetpointFan2 - HBy * 256;  //LOW-Byte berechnen
    Wire.beginTransmission(DAC_I2C_OUT_ADDR); // Start Übertragung zur ANALOG-OUT Karte
    Wire.write(DAC_CHANNEL_FAN2);             // FAN 2 schreiben
    Wire.write(LBy);                          // LOW-Byte schreiben
    Wire.write(HBy);                          // HIGH-Byte schreiben
    Wire.endTransmission();                   // Ende    
//  }
    
    return false;
  }
}


void SpeedCalibrationPwm() {
  Serial.println ("SpeedCalibrationPwm startet");
  if (!CalibrationStartet) {
    // Erste Durchlauf der Kalibrierung
    Serial.println ("Erster Durchlauf");
    CalibrationStartet = true;
    CalibrationStartMillis = millis();
    actKwlMode = 0;
    actFansCalculateSpeed = FansCalculateSpeed;
    FansCalculateSpeed = CalculateSpeed_PROP;
  }
  if (CalibrationStartet && (millis() - CalibrationStartMillis >= timeoutCalibration)) {
    // Timeout, Kalibrierung abbrechen
    FanMode = FanMode_Normal;
    CalibrationStartet = false;
    CalibrationStartMillis = 0;
    Serial.println ("Error: Kalibrierung NICHT erfolgreich");
  } else {
    if (!CalibrationPwmStufeStartet) {
      // Erste Durchlauf der Kalibrierung
      Serial.println ("Erster Durchlauf für Stufe, CalibrationPwmStufeStartet");
      CalibrationPwmStufeStartet = true;
      CalibrationPwmStufeStartMillis = millis();
      ActGoodPwmsFan1 = 0;
      ActGoodPwmsFan2 = 0;
    }
    if (CalibrationPwmStufeStartet && (millis() - CalibrationPwmStufeStartMillis <= timeoutPwmStufeCalibration)) {
      // Einzelne Stufen kalibrieren
      if (SpeedCalibrationPwmStufe(actKwlMode)) {
        // true = Kalibrierung der Lüftungsstufe beendet
        if (actKwlMode == defStandardModeCnt - 1) {
          // fertig mit allen Stufen!!!
          // Speichern in EEProm und Variablen
          for (int i = 0; ((i < defStandardModeCnt) && (i < 10)); i++) {
            PwmSetpointFan1[i] = tempPwmSetpointFan1[i];
            PwmSetpointFan2[i] = tempPwmSetpointFan2[i];
            eeprom_write_int(20 + (i * 4), PwmSetpointFan1[i]);
            eeprom_write_int(22 + (i * 4), PwmSetpointFan2[i]);
            Serial.print ("Stufe: ");
            Serial.print (i);
            Serial.print ("  PWM Fan 1: ");
            Serial.print (PwmSetpointFan1[i]);
            Serial.print ("  PWM Fan 2: ");
            Serial.println (PwmSetpointFan2[i]);
          }
          FanMode = FanMode_Normal;
          CalibrationStartet = false;
          CalibrationStartMillis = 0;
          Serial.println ("Kalibrierung erfolgreich beendet.");
        } else {
          // nächste Stufe
          CalibrationPwmStufeStartet = false;
          actKwlMode++;
        }
      }
    }
  }
}



