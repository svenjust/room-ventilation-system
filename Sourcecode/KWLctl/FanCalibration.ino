/* Die für bestimmte Drehzahl erforderlichen PWM Werte werden ermittelt und abgespeichert.
   Dazu müssen die verschiedenen Drehzahlen für jeden Lüfter per pid-Regler angefahren werden.


   Das Ganze muss ohne Delay funktionieren

*/
// Gesamte Kalibierung
boolean       CalibrationStartet      = false;
unsigned long CalibrationStartMillis  = 0;
unsigned long timeoutCalibration      = 600000;   // zehn Minuten, sollte in der Zeit die Kalibrierung nicht abgelossen werden können wird trotzdem beendet

// Kalibrierung einer Stufe
boolean       CalibrationPwmStufeStartet      = false;
unsigned long CalibrationPwmStufeStartMillis  = 0;
unsigned long timeoutPwmStufeCalibration      = 180000;   // max 3 Minuten pro Stufe

int actKwlMode = 0;

int  tempPwmSetpointFan1[ModeCnt];                                    // Speichert die pwm-Werte für die verschiedenen Drehzahlen
int  tempPwmSetpointFan2[ModeCnt];

#define goodPwmsCnt 5
int  goodPwmsFan1[goodPwmsCnt];
int  goodPwmsFan2[goodPwmsCnt];
int  ActGoodPwmsFan1;
int  ActGoodPwmsFan2;


boolean SpeedCalibrationPwmStufe(int actKwlMode) {

  if (KwlModeFactor[actKwlMode] == 0) {
    // Faktor Null ist einfach
    tempPwmSetpointFan1[actKwlMode] = 0;
    tempPwmSetpointFan2[actKwlMode] = 0;
    return true;

  } else {
    // Faktor ungleich 0
    speedSetpointFan1 = StandardSpeedSetpointFan1 * KwlModeFactor[actKwlMode];
    speedSetpointFan2 = StandardSpeedSetpointFan2 * KwlModeFactor[actKwlMode];

    double gap1 = abs(speedSetpointFan1 - speedTachoFan1); //distance away from setpoint
    double gap2 = abs(speedSetpointFan2 - speedTachoFan2); //distance away from setpoint


    if ((gap1 < 20) && (ActGoodPwmsFan1 < goodPwmsCnt)) {
      // einen PWM Wert gefunden
      goodPwmsFan1[ActGoodPwmsFan1] = techSetpointFan1;
      ActGoodPwmsFan1++;
    }
    if ((gap2 < 20) && (ActGoodPwmsFan2 < goodPwmsCnt)) {
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
    // Lüfter ansteuern
    analogWrite(pwmPinFan1, techSetpointFan1 / 4);
    analogWrite(pwmPinFan2, techSetpointFan2 / 4);
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
  }
  if (CalibrationStartet && (millis() - CalibrationStartMillis >= timeoutCalibration)) {
    // Timeout, Kalibrierung abbrechen
    FanMode = FanMode_Normal;
    CalibrationStartet = false;
    CalibrationStartMillis = 0;
  } else {
    if (!CalibrationPwmStufeStartet) {
      // Erste Durchlauf der Kalibrierung
      Serial.println ("Erster Durchlauf für Stufe, CalibrationPwmStufeStartet");
      CalibrationPwmStufeStartet = true;
      CalibrationPwmStufeStartMillis = millis();
      ActGoodPwmsFan1 = 0;
      ActGoodPwmsFan2 = 0;
    }
    if (CalibrationPwmStufeStartet && (millis() - CalibrationPwmStufeStartMillis >= timeoutPwmStufeCalibration)) {
      // Einzelne Stufen kalibrieren
      if (SpeedCalibrationPwmStufe(actKwlMode)) {
        // true = Kalibrierung der Lüftungsstufe beendet
        if (actKwlMode == ModeCnt - 1) {
          // fertig mit allen Stufen!!!
          // Speichern in EEProm und Variablen
          for (int i = 0; ((i < ModeCnt)&&(i<10)); i++) {
            PwmSetpointFan1[i] = tempPwmSetpointFan1[i];
            PwmSetpointFan2[i] = tempPwmSetpointFan2[i];
            eeprom_write_int(20 + (i * 4), PwmSetpointFan1[i]);
            eeprom_write_int(22 + (i * 4), PwmSetpointFan2[i]);            

            Serial.print ("Stufe: ");
            Serial.print (i);
            Serial.print ("  PWM Fan 1: ");
            Serial.print (PwmSetpointFan1[i]);
            Serial.print ("  PWM Fan 2: ");
            Serial.print (PwmSetpointFan2[i]);
          }
          

        } else {
          // nächste Stufe
          CalibrationPwmStufeStartet = false;
          actKwlMode++;
        }
      }
    }
  }
}



