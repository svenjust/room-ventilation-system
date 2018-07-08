/*
 * Copyright (C) 2018 Sven Just (sven@familie-just.de)
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

#include "NetworkClient.h"
#include "MessageHandler.h"
#include "Scheduler.h"
#include "KWLConfig.h"
#include "MQTTTopic.hpp"

#include <Ethernet.h>
#include <PubSubClient.h>

/// Do checks for new messages every 75ms.
static constexpr unsigned long NETWORK_SCHEDULE_INTERVAL = 75000;

/// Interval for checking LAN network OK (10 seconds).
static constexpr unsigned long LAN_CHECK_INTERVAL = 10000000;

/// Interval for reconnecting MQTT (5 seconds).
static constexpr unsigned long MQTT_RECONNECT_INTERVAL = 5000000;

/// MQTT heartbeat period.
static constexpr unsigned long MQTT_HEARTBEAT_PERIOD = KWLConfig::HeartbeatPeriod * 1000000UL;

PubSubClient* NetworkClient::s_client_ = nullptr;

NetworkClient::NetworkClient() :
  Task("NetworkClient"),
  mqtt_client_(eth_client_)
{}

void NetworkClient::start(Scheduler& scheduler, Print& initTracer)
{
  initTracer.println(F("Initialisierung Ethernet"));
  initEthernet();
  delay(1500);  // to give Ethernet link time to start
  last_lan_reconnect_attempt_time_ = micros();
  lan_ok_ = true;

  initTracer.println(F("Initialisierung MQTT"));
  mqtt_client_.setServer(KWLConfig::NetworkMQTTBroker, KWLConfig::NetworkMQTTPort);
  mqtt_client_.setCallback(MessageHandler::mqttMessageReceived);
  last_mqtt_reconnect_attempt_time_ = micros();
  mqtt_ok_ = true;
  s_client_ = &mqtt_client_;
  run();  // first run call here to connect MQTT

  scheduler.addRepeated(*this, NETWORK_SCHEDULE_INTERVAL);
}

void NetworkClient::initEthernet()
{
  uint8_t mac[6];
  KWLConfig::NetworkMACAddress.copy_to(mac);
  Ethernet.begin(mac, KWLConfig::NetworkIPAddress, KWLConfig::NetworkDNSServer, KWLConfig::NetworkGateway, KWLConfig::NetworkSubnetMask);
}

bool NetworkClient::mqttConnect()
{
  Serial.print(F("MQTT connect start at "));
  Serial.println(micros());
  bool rc = mqtt_client_.connect("arduinoClientKwl", KWLConfig::NetworkMQTTUsername, KWLConfig::NetworkMQTTPassword, MQTTTopic::Heartbeat.load(), 0, true, "offline");
  if (rc) {
    // subscribe
    mqtt_client_.subscribe(MQTTTopic::Command.load());
    mqtt_client_.subscribe(MQTTTopic::CommandDebug.load());
  }
  last_mqtt_reconnect_attempt_time_ = micros();
  force_heartbeat_ = true;
  Serial.print(F("MQTT connect end at "));
  Serial.print(last_mqtt_reconnect_attempt_time_);
  if (mqtt_client_.connected()) {
    Serial.println(F(" [successful]"));
    return true;
  } else {
    Serial.println(F(" [failed]"));
    return false;
  }
}

void NetworkClient::run()
{
  Ethernet.maintain();
  if (lan_ok_) {
    if (Ethernet.localIP()[0] == 0) {
      Serial.println(F("LAN disconnected, attempting to connect"));
      lan_ok_ = false;
      initEthernet(); // nothing more to do now
      last_lan_reconnect_attempt_time_ = getScheduleTime();
      return;
    }
    // have Ethernet, do other checks
  } else {
    // no Ethernet previously, check if now connected
    if (Ethernet.localIP()[0] != 0) {
      Serial.print(F("LAN connected, IP: "));
      Serial.println(Ethernet.localIP());
      lan_ok_ = true;
      mqtt_ok_ = true; // to force check and immediate reconnect
    } else {
      // still no Ethernet
      if (getScheduleTime() - last_lan_reconnect_attempt_time_ >= LAN_CHECK_INTERVAL) {
        // try reconnecting
        initEthernet();
        last_lan_reconnect_attempt_time_ = getScheduleTime();
      }
      return;
    }
  }

  if (mqtt_ok_) {
    if (!mqtt_client_.connected()) {
      Serial.println(F("MQTT disconnected, attempting to connect"));
      mqtt_ok_ = mqttConnect();
      if (!mqtt_ok_)
        return; // couldn't connect now, cannot continue
    }
    // have MQTT receive messages
  } else {
    // no MQTT previously, check if now connected
    if (getScheduleTime() - last_mqtt_reconnect_attempt_time_ >= MQTT_RECONNECT_INTERVAL) {
      // new reconnect attempt
      mqtt_ok_ = mqttConnect();
      if (!mqtt_ok_)
        return;
    } else {
      return; // not connected
    }
  }

  if (getScheduleTime() - last_heartbeat_time_ >= MQTT_HEARTBEAT_PERIOD)
    force_heartbeat_ = true;
  if (force_heartbeat_) {
    // once connected or after timeout, publish an announcement
    last_heartbeat_time_ = getScheduleTime();
    force_heartbeat_ = !MessageHandler::publish(MQTTTopic::Heartbeat, F("online"), true);
  }

  // now MQTT messages can be received
  mqtt_client_.loop();
}
