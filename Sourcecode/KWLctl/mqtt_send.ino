/*
  ################################################################
  #
  #   Copyright notice
  #
  #   Control software for a Room Ventilation System
  #   https://github.com/svenjust/room-ventilation-system
  #
  #   Copyright (C) 2018  Sven Just (sven@familie-just.de)
  #
  #   This program is free software: you can redistribute it and/or modify
  #   it under the terms of the GNU General Public License as published by
  #   the Free Software Foundation, either version 3 of the License, or
  #   (at your option) any later version.
  #
  #   This program is distributed in the hope that it will be useful,
  #   but WITHOUT ANY WARRANTY; without even the implied warranty of
  #   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  #   GNU General Public License for more details.
  #
  #   You should have received a copy of the GNU General Public License
  #   along with this program.  If not, see <https://www.gnu.org/licenses/>.
  #
  #   This copyright notice MUST APPEAR in all copies of the script!
  #
  ################################################################
*/

// loopMqtt... senden Werte an den mqtt-Server.

void loopMqttSendConfig() {
  // Senden der aktuellen Konfiguration, die im EEPROM gespeichert wird.
  // Bedingung: a)  mqttCmdSendConfig == true
  if (mqttCmdSendConfig) {
    mqttCmdSendConfig = false;
    // Zusammensuchen der Konfig

  }
}

void loopMqttSendBypass() {
  // Senden der Bypass - Einstellung per Mqtt
  // Bedingung: a) alle x Sekunden
  //            b) mqttCmdSendBypassState == true
  currentMillis = millis();
  if ((currentMillis - previousMillisMqttBypassState > intervalMqttBypassState) || mqttCmdSendBypassState || mqttCmdSendBypassAllValues) {
    previousMillisMqttBypassState = currentMillis;
    mqttCmdSendBypassState = false;

    switch (bypassFlapState) {

      case bypassFlapState_Open:
        mqttClient.publish(MQTTTopic::KwlBypassState, "open");
        if (KWLConfig::serialDebug == 1) {
          Serial.println(F("MQTTTopic::KwlBypassState: open"));
        }
        break;
      case bypassFlapState_Close:
        mqttClient.publish(MQTTTopic::KwlBypassState, "closed");
        if (KWLConfig::serialDebug == 1) {
          Serial.println(F("MQTTTopic::KwlBypassState: closed"));
        }
        break;
      case bypassFlapState_Unknown:
        mqttClient.publish(MQTTTopic::KwlBypassState, "unknown");
        if (KWLConfig::serialDebug == 1) {
          Serial.println(F("MQTTTopic::KwlBypassState: unknown"));
        }
        break;
    }

    // In MqttSendBypassAllValues() wird geprüft, ob alle Werte gesendet werden sollen.
    MqttSendBypassAllValues();
  }
}


void MqttSendBypassAllValues() {
  // Senden der Bypass - Einstellung per Mqtt
  // Bedingung: a) mqttCmdSendBypassAllValues == true
  if (mqttCmdSendBypassAllValues) {
    mqttCmdSendBypassAllValues = false;

    if (bypassMode == bypassMode_Auto) {
      mqttClient.publish(MQTTTopic::KwlBypassMode, "auto");
      if (KWLConfig::serialDebug == 1) {
        Serial.println(F("MQTTTopic::KwlBypassMode: auto"));
      }
    } else if (bypassMode == bypassMode_Manual) {
      mqttClient.publish(MQTTTopic::KwlBypassMode, "manual");
      if (KWLConfig::serialDebug == 1) {
        Serial.println(F("MQTTTopic::KwlBypassMode: manual"));
      }
    }
    itoa(bypassTempAbluftMin, buffer, 10);
    mqttClient.publish(MQTTTopic::KwlBypassTempAbluftMin, buffer);
    if (KWLConfig::serialDebug == 1) {
      Serial.println("MQTTTopic::KwlBypassTempAbluftMin: " + String(bypassTempAbluftMin));
    }
    itoa(bypassTempAussenluftMin, buffer, 10);
    mqttClient.publish(MQTTTopic::KwlBypassTempAussenluftMin, buffer);
    if (KWLConfig::serialDebug == 1) {
      Serial.println("MQTTTopic::KwlBypassTempAussenluftMin: " + String(bypassTempAussenluftMin));
    }
    itoa(bypassHystereseMinutes, buffer, 10);
    mqttClient.publish(MQTTTopic::KwlBypassHystereseMinutes, buffer);
    if (KWLConfig::serialDebug == 1) {
      Serial.println("MQTTTopic::KwlBypassHystereseMinutes: " + String(bypassHystereseMinutes));
    }
  }
}

void loopMqttSendDHT() {
  // Senden der DHT Temperaturen und Humidity per Mqtt
  // Bedingung: a) alle x Sekunden, wenn Differenz Temperatur > 0.5
  //            b) alle intervalMqttTempOversampling/1000 Sekunden (Standard 5 Minuten)
  //            c) mqttCmdSendDht == true
  currentMillis = millis();
  if ((currentMillis - previousMillisMqttDht > intervalMqttTemp) || mqttCmdSendDht) {
    previousMillisMqttDht = currentMillis;
    if ((abs(DHT1Temp - SendMqttDHT1Temp) > 0.5)
        || (abs(DHT2Temp - SendMqttDHT2Temp) > 0.5)
        || (abs(DHT1Hum - SendMqttDHT1Hum) > 1)
        || (abs(DHT2Hum - SendMqttDHT2Hum) > 1)
        || (currentMillis - previousMillisMqttDhtOversampling > intervalMqttTempOversampling)
        || mqttCmdSendDht)  {

      mqttCmdSendDht = false;
      SendMqttDHT1Temp = DHT1Temp;
      SendMqttDHT2Temp = DHT2Temp;
      SendMqttDHT1Hum = DHT1Hum;
      SendMqttDHT2Hum = DHT2Hum;
      previousMillisMqttDhtOversampling = currentMillis;

      if (DHT1IsAvailable) {
        dtostrf(DHT1Temp, 6, 2, buffer);
        mqttClient.publish(MQTTTopic::KwlDHT1Temperatur, buffer);
        if (KWLConfig::serialDebug == 1) {
          Serial.println("MQTTTopic::KwlDHT1Temperatur: " + String(DHT1Temp));
        }
        dtostrf(DHT1Hum, 6, 2, buffer);
        mqttClient.publish(MQTTTopic::KwlDHT1Humidity, buffer);
        if (KWLConfig::serialDebug == 1) {
          Serial.println("MQTTTopic::KwlDHT1Humidity: " + String(DHT1Hum));
        }
      }
      if (DHT2IsAvailable) {
        dtostrf(DHT2Temp, 6, 2, buffer);
        mqttClient.publish(MQTTTopic::KwlDHT2Temperatur, buffer);
        if (KWLConfig::serialDebug == 1) {
          Serial.println("MQTTTopic::KwlDHT2Temperatur: " + String(DHT2Temp));
        }

        dtostrf(DHT2Hum, 6, 2, buffer);
        mqttClient.publish(MQTTTopic::KwlDHT2Humidity, buffer);
        if (KWLConfig::serialDebug == 1) {
          Serial.println("MQTTTopic::KwlDHT2Humidity: " + String(DHT2Hum));
        }
      }
    }
  }
}

void loopMqttSendCo2() {
  // Senden der DHT Temperaturen und Humidity per Mqtt
  // Bedingung: a) alle x Sekunden, wenn Differenz Sensor > 20
  //            b) alle intervalMqttMHZ14Oversampling/1000 Sekunden (Standard 10 Minuten)
  //            c) mqttCmdSendMHZ14 == true
  currentMillis = millis();
  if ((currentMillis - previousMillisMqttMHZ14 > intervalMqttMHZ14) || mqttCmdSendMHZ14) {
    previousMillisMqttMHZ14 = currentMillis;
    if ((abs(MHZ14_CO2_ppm - SendMqttMHZ14CO2) > 20)
        || (currentMillis - previousMillisMqttMHZ14Oversampling > intervalMqttMHZ14Oversampling)
        || mqttCmdSendMHZ14)  {

      mqttCmdSendMHZ14 = false;
      SendMqttMHZ14CO2 = MHZ14_CO2_ppm;

      intervalMqttMHZ14Oversampling = currentMillis;

      if (MHZ14IsAvailable) {
        dtostrf(MHZ14_CO2_ppm, 6, 0, buffer);
        mqttClient.publish(MQTTTopic::KwlCO2Abluft, buffer);
        if (KWLConfig::serialDebug == 1) {
          Serial.println("MQTTTopic::KwlCO2Abluft: " + String(MHZ14_CO2_ppm));
        }
      }
    }
  }
}

void loopMqttHeartbeat() {
  if (millis() - previousMillisMqttHeartbeat > 30000) {
    previousMillisMqttHeartbeat = millis();
    mqttClient.publish(MQTTTopic::Heartbeat, "online");
  }
}

void loopNetworkConnection() {
  // LAN CONNECTION
  if (millis() - lastLanReconnectAttempt > 10000) {
    if (Ethernet.localIP()[0] == 0) {
      // KEIN NETZVERBINDUNG
      Serial.println(F("LAN not connected"));
      bLanOk = false;
      uint8_t mac[6];
      KWLConfig::mac.copy_to(mac);
      Ethernet.begin(mac, KWLConfig::ip, KWLConfig::DnServer, KWLConfig::gateway, KWLConfig::subnet);
    } else {
      bLanOk = true;
    }
    lastLanReconnectAttempt = millis();
  }

  // mqtt Client connected?
  if (!mqttClient.connected()) {
    long now = millis();
    if (now - lastMqttReconnectAttempt > 5000) {
      //if (KWLConfig::serialDebug == 1) {
      Serial.println(F("Mqtt Client not connected"));
      bMqttOk = false;
      //}
      lastMqttReconnectAttempt = now;
      // Attempt to reconnect
      if (mqttReconnect()) {
        bMqttOk = true;
        lastMqttReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    bMqttOk = true;
    mqttClient.loop();
  }
}

// LOOP SCHLEIFE Ende
