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

#include <Arduino.h>

class PubSubClient;

/*!
 * @brief Handler for incoming MQTT messages.
 *
 * Creating an instance automatically registers it with the handler.
 * Derive from this class in your component to handle incoming and outgoing messages.
 */
class MessageHandler
{
public:
  MessageHandler();

  virtual ~MessageHandler() {}

  /*!
   * @brief Publish a message.
   *
   * @param topic message topic.
   * @param payload message payload (string).
   * @param retained if set, retain the message on the server for quick read upon client connect.
   * @return @c true, if published successfully, @c false otherwise.
   */
  bool publish(const char* topic, const char* payload, bool retained = false);

  /*!
   * @brief Publish a message.
   *
   * @param topic message topic.
   * @param payload message payload (integer).
   * @param retained if set, retain the message on the server for quick read upon client connect.
   * @return @c true, if published successfully, @c false otherwise.
   */
  bool publish(const char* topic, long payload, bool retained = false);

  /*!
   * @brief Publish a message.
   *
   * @param topic message topic.
   * @param payload message payload (floating point).
   * @param precision number of decimal places to display.
   * @param retained if set, retain the message on the server for quick read upon client connect.
   * @return @c true, if published successfully, @c false otherwise.
   */
  bool publish(const char* topic, double payload, unsigned char precision = 2, bool retained = false);

private:
  friend class MQTTClient;

  /*!
   * @brief Try to handle received message.
   *
   * @param topic MQTT topic.
   * @param payload payload of the MQTT message (NUL-terminated after length).
   * @param length length of the payload.
   * @return @c true, if the message was handled, @c false otherwise (e.g., for other component).
   */
  virtual bool mqttReceiveMsg(const StringView& topic, const char* payload, unsigned int length) = 0;

  MessageHandler* next_;
};

/*!
 * @brief Client to communicate with MQTT protocol.
 */
// TODO move ethernet init and client handling here
class MQTTClient
{
public:
  /// Construct MQTT client.
  explicit MQTTClient(PubSubClient& client);

  // TODO make private after MQTT client cleaned up.
  static void mqttReceiveMsg(char* topic, byte* payload, unsigned int length);

private:
  PubSubClient& client_;
};
