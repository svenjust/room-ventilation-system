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
#include "KWLConfig.h"
#include "MQTTTopic.hpp"

#include <MicroNTP.h>

/// Interval for checking LAN network OK (10 seconds).
static constexpr unsigned long LAN_CHECK_INTERVAL = 10000000;

/// Interval for reconnecting MQTT (5 seconds).
static constexpr unsigned long MQTT_RECONNECT_INTERVAL = 5000000;

/// MQTT heartbeat period.
static constexpr unsigned long MQTT_HEARTBEAT_PERIOD = KWLConfig::HeartbeatPeriod * 1000000UL;

NetworkClient::NetworkClient(KWLPersistentConfig& config, MicroNTP& ntp) :
  mqtt_client_(eth_client_),
  config_(config),
  ntp_(ntp),
  stats_(F("NetworkClient")),
  timer_task_(stats_, &NetworkClient::run, *this),
  poll_stats_(F("NetworkClient")),
  poll_task_(poll_stats_, &NetworkClient::loop, *this)
{}

void NetworkClient::begin(Print& initTracer)
{
  initEthernet(initTracer);
  delay(1500);  // to give Ethernet link time to start
  last_lan_reconnect_attempt_time_ = micros();
  lan_ok_ = true;

  initTracer.print(F("Initialisierung MQTT, broker IP "));
  initTracer.println(IPAddress(KWLConfig::NetworkMQTTBroker));
  mqtt_client_.setServer(KWLConfig::NetworkMQTTBroker, KWLConfig::NetworkMQTTPort);
  MessageHandler::begin(mqtt_client_, KWLConfig::serialDebug);
  last_mqtt_reconnect_attempt_time_ = micros();
  mqtt_ok_ = true;
  loop();  // first run call here to connect MQTT
}

void NetworkClient::initEthernet(Print& initTracer)
{
  initTracer.print(F("Initialisierung Ethernet, IP "));
  IPAddress ip = KWLConfig::NetworkIPAddress;
  IPAddress gw = KWLConfig::NetworkGateway;
  IPAddress subnet = KWLConfig::NetworkSubnetMask;
  IPAddress dns = KWLConfig::NetworkDNSServer;
  IPAddress ntp = KWLConfig::NetworkNTPServer;
  initTracer.print(ip);
  Serial.print('/');
  Serial.print(subnet);
  Serial.print(F(" gw "));
  Serial.print(gw);
  Serial.print(F(" dns "));
  Serial.print(dns);
  Serial.print(F(" ntp "));
  Serial.print(ntp);
  initTracer.println();
  uint8_t mac[6];
  KWLConfig::NetworkMACAddress.copy_to(mac);
  Ethernet.begin(mac, ip, dns, gw, subnet);
}

bool NetworkClient::mqttConnect()
{
  Serial.print(F("MQTT connect start at "));
  Serial.println(micros());
  static constexpr auto NAME = makeFlashStringLiteral("arduinoClientKwl");
  static constexpr auto WILL_MESSAGE = makeFlashStringLiteral("offline");
  bool rc = mqtt_client_.connect(NAME.load(),
                                 KWLConfig::NetworkMQTTUsername, KWLConfig::NetworkMQTTPassword,
                                 MQTTTopic::Heartbeat.load(), 0, true, WILL_MESSAGE.load());
  if (rc) {
    // subscribe
    subscribed_command_ = mqtt_client_.subscribe(MQTTTopic::Command.load());
    subscribed_debug_ = mqtt_client_.subscribe(MQTTTopic::CommandDebug.load());
    timer_task_.runRepeated(1, MQTT_HEARTBEAT_PERIOD); // next run should send heartbeat
  }
  last_mqtt_reconnect_attempt_time_ = micros();
  Serial.print(F("MQTT connect end at "));
  Serial.print(last_mqtt_reconnect_attempt_time_);
  if (mqtt_client_.connected()) {
    Serial.println(F(" [successful]"));
    return true;
  } else {
    Serial.println(F(" [failed]"));
    timer_task_.cancel();
    return false;
  }
}

void NetworkClient::loop()
{
  if (Serial.available()) {
    // there is data on serial port, read command from there
    char c = char(Serial.read());
    if (c == 10 || c == 13) {
      // process command in form <topic> <value>
      if (serial_data_size_) {
        serial_data_[serial_data_size_] = 0;
        auto delim = strchr(serial_data_, ' ');
        if (!delim) {
          Serial.println(F("Invalid command, expected \"<topic> <value>\""));
        } else {
          *delim++ = 0;
          while (*delim == ' ' || *delim == '\t')
            ++delim;
          MessageHandler::debugPushMessage(
                serial_data_,
                reinterpret_cast<uint8_t*>(delim),
                unsigned(serial_data_size_ - (delim - serial_data_)));
        }
        serial_data_size_ = 0;
      }
    } else if (serial_data_size_ < SERIAL_BUFFER_SIZE - 1) {
      serial_data_[serial_data_size_++] = c;
    }
  }

  Ethernet.maintain();
  auto current_time = micros();
  if (lan_ok_) {
    if (Ethernet.localIP()[0] == 0) {
      Serial.println(F("LAN disconnected, attempting to connect"));
      lan_ok_ = false;
      timer_task_.cancel();
      initEthernet(Serial); // nothing more to do now
      last_lan_reconnect_attempt_time_ = current_time;
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
      if (current_time - last_lan_reconnect_attempt_time_ >= LAN_CHECK_INTERVAL) {
        // try reconnecting
        initEthernet(Serial);
        last_lan_reconnect_attempt_time_ = current_time;
      }
      return;
    }
  }

  ntp_.loop();

  if (mqtt_ok_) {
    if (!mqtt_client_.connected()) {
      Serial.println(F("MQTT disconnected, attempting to connect"));
      timer_task_.cancel();
      mqtt_ok_ = mqttConnect();
      if (!mqtt_ok_)
        return; // couldn't connect now, cannot continue
    }
    // have MQTT receive messages
  } else {
    // no MQTT previously, check if now connected
    if (current_time - last_mqtt_reconnect_attempt_time_ >= MQTT_RECONNECT_INTERVAL) {
      // new reconnect attempt
      mqtt_ok_ = mqttConnect();
      if (!mqtt_ok_)
        return;
    } else {
      return; // not connected
    }
  }

  // Make sure we are subscribed, if after connect we didn't succeed
  if (!subscribed_command_)
    subscribed_command_ = mqtt_client_.subscribe(MQTTTopic::Command.load());
  if (!subscribed_debug_)
    subscribed_debug_ = mqtt_client_.subscribe(MQTTTopic::CommandDebug.load());

  // now MQTT messages can be received
  mqtt_client_.loop();
  // ...and send back anything still in queue
  PublishTask::loop();
}

void NetworkClient::run()
{
  // once connected or after timeout, publish an announcement
  if (KWLConfig::HeartbeatTimestamp && ntp_.hasTime()) {
    auto time = ntp_.currentTimeHMS(config_.getTimezoneMin() * 60, config_.getDST());
    publish_task_.publish([time](){
      char buffer[9];
      time.writeHMS(buffer);
      buffer[8] = 0;
      return MessageHandler::publish(MQTTTopic::Heartbeat, buffer, true);
    });
  } else {
    publish_task_.publish(MQTTTopic::Heartbeat, F("online"), true);
  }
}
