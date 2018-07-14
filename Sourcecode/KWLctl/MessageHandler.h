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

/*!
 * @brief Task used to publish MQTT messages asynchronously.
 *
 * When a publish task is activated, it will try to send the MQTT message
 * right away. If not successful, it will retry next time to ultimately
 * send the message.
 *
 * The message is sent in a callback, so it's possible to send also many
 * messages from the callback by holding the state somewhere. By default,
 * publish task will hold a small closure, so it's possible to pass some
 * arguments in the closure. Normally, however, one would read the current
 * value from the class.
 *
 * @note Each task consumes 16B of memory.
 */
class PublishTask
{
public:
  PublishTask(const PublishTask&) = delete;
  PublishTask& operator=(const PublishTask&) = delete;

  PublishTask();

  /*!
   * @brief Publish using a function.
   *
   * If the task is still in use and another publish() is called, then the
   * previous publish() is aborted.
   *
   * @param message_writer message writer calling MessageHandler::publish()
   *    to actually publish a message. Returns bool indicating if the send
   *    was complete.
   */
  template<typename Func>
  void publish(Func&& message_writer) {
    static_assert(sizeof(Func) < sizeof(closure_space_), "Too big writer closure, reduce");
  #if 0 // do not publish synchronosuly to save RAM
    if (message_writer())
      return;   // published immediately synchronously
  #endif
    // now move into closure
    new(closure_space_) Func(static_cast<Func&&>(message_writer));
    auto tmp = [](void* closure) -> bool {
      return (*reinterpret_cast<Func*>(closure))();
    };
    invoker_ = tmp;
  }

  /*!
   * @brief Publish a message asynchronously.
   *
   * @param topic message topic.
   * @param payload message payload (any type supported by other publish() methods).
   * @param args additional arguments to type-specific publish() method.
   * @return @c true, if published successfully, @c false otherwise.
   */
  template<typename TopicType, typename PayloadType, typename... Args>
  void publish(const TopicType& topic, PayloadType payload, Args... args);

  /// Continue sending on all tasks with unsent data in loop().
  static void loop();

private:
  char closure_space_[12];            ///< Space for the closure of the writer.
  bool (*invoker_)(void*) = nullptr;  ///< Invoker of the writer, if active.
  PublishTask* next_;                 ///< Next registered publish task.
  static PublishTask* s_first_task_;  ///< First registered task.
};

/*!
 * @brief Handler for incoming MQTT messages.
 *
 * Creating an instance automatically registers it with the handler.
 * Derive from this class in your component to handle incoming and outgoing messages.
 */
class MessageHandler
{
public:
  MessageHandler(const MessageHandler&) = delete;
  MessageHandler& operator=(const MessageHandler&) = delete;

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

  /*!
   * @brief Publish a message conditionally.
   *
   * If a flag is set for this message type, it will be published, otherwise
   * not. If publishing succeeds, the flag will be removed. This eases writing
   * publish tasks - simply keep a bitmask of messages to send, send them
   * conditionally and return true if all bits were cleared.
   *
   * @param flags flag bitmask with bits set to indicate messages to send.
   * @param check_flag which flag bit to check.
   * @param topic message topic.
   * @param payload message payload (any type supported by other publish() methods).
   * @param args additional arguments to type-specific publish() method.
   * @return @c true, if published successfully, @c false otherwise.
   */
  template<typename FlagType, typename TopicType, typename PayloadType, typename... Args>
  static bool publish_if(FlagType& flags, FlagType check_flag, const TopicType& topic, PayloadType payload, Args... args) {
    if ((flags & check_flag) == 0)
      return true; // already sent
    if (!publish(topic, payload, args...))
      return false; // didn't succeed, will retry
    flags &= ~check_flag; // successful, remove flag
    return true;
  }

private:
  friend class NetworkClient;

  /*!
   * @brief Try to handle received message.
   *
   * @param topic MQTT topic.
   * @param s payload of the MQTT message (NUL-terminated string view).
   * @return @c true, if the message was handled, @c false otherwise (e.g., for other component).
   */
  virtual bool mqttReceiveMsg(const StringView& topic, const StringView& s) = 0;

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

template<typename TopicType, typename PayloadType, typename... Args>
void PublishTask::publish(const TopicType& topic, PayloadType payload, Args... args) {
  return publish([&topic, payload, args...]() -> bool {
    return MessageHandler::publish(topic, payload, args...);
  });
}
