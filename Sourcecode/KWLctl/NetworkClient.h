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

/*!
 * @file
 * @brief Client to communicate with MQTT protocol.
 */
#pragma once

#include "StringView.h"
#include "TimeScheduler.h"
#include "MessageHandler.h"

#include <Ethernet.h>
#include <PubSubClient.h>

class MicroNTP;
class KWLPersistentConfig;

/*!
 * @brief Client to communicate with MQTT protocol over Ethernet.
 */
class NetworkClient : private MessageHandler
{
public:
  NetworkClient(const NetworkClient&) = delete;
  NetworkClient& operator=(const NetworkClient&) = delete;

  /// Construct network client.
  NetworkClient(KWLPersistentConfig& config, MicroNTP& ntp);

  /// Start network client.
  void begin(Print& initTracer);

  /// Check if LAN is OK.
  bool isLANOk() const { return lan_ok_; }

  /// Check if MQTT is OK.
  bool isMQTTOk() const { return mqtt_ok_; }

private:
  /// Initialize Ethernet connection.
  void initEthernet(Print& initTracer);

  /// Initialize MQTT connection.
  bool mqttConnect();

  /// Check network.
  void run();

  /// Loop method to be called regularly to maintain the connection.
  void loop();

  /// (Re-)subscribe to topics, if not subscribed yet.
  void resubscribe();

  /// Loop task to send MQTT messages.
  static void sendMQTT();

  virtual bool mqttReceiveMsg(const StringView& topic, const StringView& s) override;

  /// Maximum size of serial buffer for sending messages over serial port.
  static constexpr uint8_t SERIAL_BUFFER_SIZE = 128;

  /// Ethernet client for MQTT client.
  EthernetClient eth_client_;
  /// MQTT client.
  PubSubClient mqtt_client_;
  /// Persistent configuration.
  KWLPersistentConfig& config_;
  /// NTP client to report online as timestamp.
  MicroNTP& ntp_;
  /// Last time when LAN started a reconnect attempt.
  unsigned long last_mqtt_reconnect_attempt_time_ = 0;
  /// Last time when MQTT started a reconnect attempt.
  unsigned long last_lan_reconnect_attempt_time_ = 0;
  /// Flag set when LAN is present.
  bool lan_ok_ = false;
  /// Flag set when MQTT is present.
  bool mqtt_ok_ = false;
  /// Subscribe flag for commands.
  bool subscribed_command_ = false;
  /// Subscribe flag for debug commands.
  bool subscribed_debug_ = false;
  /// Task to publish MQTT heartbeat message.
  PublishTask publish_task_;
  /// Data received over serial port.
  char serial_data_[SERIAL_BUFFER_SIZE];
  /// Size of data received so far.
  uint8_t serial_data_size_ = 0;
  /// Task timing statistics.
  Scheduler::TaskTimingStats stats_;
  /// Timer tasks handling heartbeat.
  Scheduler::TimedTask<NetworkClient> timer_task_;
  /// Task polling statistics.
  Scheduler::TaskPollingStats poll_stats_;
  /// Poll tasks for maintaining network connection.
  Scheduler::PollTask<NetworkClient> poll_task_;
  /// Poll tasks for sending MQTT messages.
  Scheduler::PollTask<> mqtt_send_poll_task_;
};
