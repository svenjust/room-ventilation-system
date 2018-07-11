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

/*!
 * @file
 * @brief Handler for incoming MQTT messages.
 */
#pragma once

#include "FlashStringLiteral.h"

class StringView;
class __FlashStringHelper;

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

  virtual ~MessageHandler();

  /*!
   * @brief Publish a message.
   *
   * @param topic message topic.
   * @param payload message payload (string).
   * @param retained if set, retain the message on the server for quick read upon client connect.
   * @return @c true, if published successfully, @c false otherwise.
   */
  static bool publish(const char* topic, const char* payload, bool retained = false);

  /*!
   * @brief Publish a message.
   *
   * @param topic message topic.
   * @param payload message payload (string).
   * @param retained if set, retain the message on the server for quick read upon client connect.
   * @return @c true, if published successfully, @c false otherwise.
   */
  static bool publish(const char* topic, const __FlashStringHelper* payload, bool retained = false);

  /*!
   * @brief Publish a message.
   *
   * @param topic message topic.
   * @param payload message payload (integer).
   * @param retained if set, retain the message on the server for quick read upon client connect.
   * @return @c true, if published successfully, @c false otherwise.
   */
  static bool publish(const char* topic, long payload, bool retained = false);

  /*!
   * @brief Publish a message.
   *
   * @param topic message topic.
   * @param payload message payload (integer).
   * @param retained if set, retain the message on the server for quick read upon client connect.
   * @return @c true, if published successfully, @c false otherwise.
   */
  static bool publish(const char* topic, int payload, bool retained = false) {
    return publish(topic, long(payload), retained);
  }

  /*!
   * @brief Publish a message.
   *
   * @param topic message topic.
   * @param payload message payload (unsigned integer).
   * @param retained if set, retain the message on the server for quick read upon client connect.
   * @return @c true, if published successfully, @c false otherwise.
   */
  static bool publish(const char* topic, unsigned long payload, bool retained = false);

  /*!
   * @brief Publish a message.
   *
   * @param topic message topic.
   * @param payload message payload (unsigned integer).
   * @param retained if set, retain the message on the server for quick read upon client connect.
   * @return @c true, if published successfully, @c false otherwise.
   */
  static bool publish(const char* topic, unsigned int payload, bool retained = false) {
    return publish(topic, static_cast<unsigned long>(payload), retained);
  }

  /*!
   * @brief Publish a message.
   *
   * @param topic message topic.
   * @param payload message payload (floating point).
   * @param precision number of decimal places to display.
   * @param retained if set, retain the message on the server for quick read upon client connect.
   * @return @c true, if published successfully, @c false otherwise.
   */
  static bool publish(const char* topic, double payload, unsigned char precision = 2, bool retained = false);

  /*!
   * @brief Publish a message to a topic string stored in Flash memory.
   *
   * @param topic message topic stored in Flash memory.
   * @param payload message payload (any type supported by other publish() methods).
   * @param args additional arguments to type-specific publish() method.
   * @return @c true, if published successfully, @c false otherwise.
   */
  template<unsigned Len, typename PayloadType, typename... Args>
  inline static bool publish(const FlashStringLiteral<Len>& topic, PayloadType payload, Args... args) {
    return publish(topic.load(), payload, args...);
  }

private:
  friend class NetworkClient;

  /*!
   * @brief Try to handle received message.
   *
   * @param topic MQTT topic.
   * @param payload payload of the MQTT message (NUL-terminated after length).
   * @param length length of the payload.
   * @return @c true, if the message was handled, @c false otherwise (e.g., for other component).
   */
  virtual bool mqttReceiveMsg(const StringView& topic, const char* payload, unsigned int length) = 0;

  /*!
   * @brief Handle a new message. Calls all registered handlers.
   *
   * @param topic MQTT topic.
   * @param payload payload of the MQTT message (NUL-terminated after length).
   * @param length length of the payload.
   */
  static void mqttMessageReceived(char* topic, uint8_t* payload, unsigned int length);

  MessageHandler* next_;
  static MessageHandler* s_first_handler;
};
