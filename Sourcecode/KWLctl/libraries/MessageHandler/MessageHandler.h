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
 * @brief Handler for incoming MQTT messages and asynchronous publishing task.
 *
 * This library requires StringView library and typically will be used with
 * PubSubClient MQTT library. Optionally, FlashStringLiteral library can be
 * used to define message topics as constexpr expressions in Flash memory.
 */
#pragma once

#include <StringView.h>
#include <avr/pgmspace.h>

/*
 * NOTE: Messages are normally never published synchronously to save RAM. However,
 * you can define this macro before including the header to force trying to send
 * the message synchronously before falling back to asynchronous send in
 * PublishTask::loop().
 */
//#define MESSAGE_HANDLER_SYNC_PUBLISH

/// In-place new operator.
inline void* operator new(size_t, void* ptr) { return ptr; }

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
   * previous publish() is aborted and replaced with the new one. E.g., if
   * a new value is to be transmitted, it overwrites the old value, which
   * was not yet transmitted.
   *
   * @param message_writer message writer calling MessageHandler::publish()
   *    to actually publish a message. Returns bool indicating if the send
   *    was complete.
   */
  template<typename Func>
  void publish(Func&& message_writer) {
    static_assert(sizeof(Func) < sizeof(closure_space_), "Too big writer closure, reduce");
  #ifdef MESSAGE_HANDLER_SYNC_PUBLISH
    if (message_writer())
      return;   // published immediately synchronously
  #endif
    // now move into closure
    new(closure_space_) Func(static_cast<Func&&>(message_writer));
    auto tmp = [](void* closure) -> bool {
      return (*reinterpret_cast<Func*>(closure))();
    };
    invoker_ = tmp;
    s_has_tasks_ = true;
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

  /// Cancel pending send.
  void cancel() noexcept { invoker_ = nullptr; }

  /// Check if any tasks are pending.
  static bool hasTasks() noexcept { return s_has_tasks_; }

  /*!
   * @brief Continue sending on all tasks with unsent data in loop().
   *
   * The return value allows for integration with task schedulers supporting
   * deep sleep. If no task is pending, then the scheduler can enter deep
   * sleep and stop calling loop(). The scheduler can also query presence
   * of tasks using hasTasks() method.
   *
   * @return @c true, if next loop() call is necessary, @c false otherwise.
   */
  static bool loop();

private:
  char closure_space_[12];            ///< Space for the closure of the writer.
  bool (*invoker_)(void*) = nullptr;  ///< Invoker of the writer, if active.
  PublishTask* next_;                 ///< Next registered publish task.

  static bool s_has_tasks_;           ///< Flag indicating if tasks are pending.
  static PublishTask* s_first_task_;  ///< First registered task.
};

/*!
 * @brief Handler for incoming MQTT messages and publising outgoing messages.
 *
 * Creating an instance automatically registers it with the handler, so
 * call to mqttMessageReceived() will call the handler. First handler
 * which processes the message wins.
 *
 * Derive from this class in your component to handle incoming messages.
 *
 * To send outgoing messages reliably, use PublishTask::publish(), which in turn
 * calls MessageHandler::publish() as often as needed to ensure the message is
 * indeed sent ultimately.
 */
class MessageHandler
{
public:
  /*!
   * @brief Signature of a publishing method.
   *
   * The signature is intentionally made compatible with PubSubClient, so it can be
   * easily integrated with PubSubClient.
   *
   * @param instance instance pointer as specified in begin().
   * @param topic,payload,retained parameters to publish() method.
   * @return @c true, if the message was sent, @c false, if not.
   */
  using publish_callback = bool (*)(void* instance, const char* topic, const char* payload, bool retained);

  MessageHandler(const MessageHandler&) = delete;
  MessageHandler& operator=(const MessageHandler&) = delete;

  /// Create message handler with the given name.
  explicit MessageHandler(const __FlashStringHelper* name);

  virtual ~MessageHandler();

  /*!
   * @brief Start sending and receiving messages.
   *
   * @param cb callback for sending messages.
   * @param cb_arg callback argument (instance of PubSubClient).
   * @param debug if set, print debugging messages to Serial output.
   */
  static void begin(publish_callback cb, void *cb_arg, bool debug = false);

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
  template<typename PayloadType, typename... Args>
  inline static bool publish(const __FlashStringHelper* topic, PayloadType payload, Args... args) {
    auto ctopic = reinterpret_cast<const char*>(topic);
    auto topic_len = strlen_P(ctopic) + 1;
    char buffer[topic_len];
    memcpy_P(buffer, ctopic, topic_len);
    return publish(buffer, payload, args...);
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

  /*!
   * @brief Handle a new message. Calls all registered handlers.
   *
   * The method signature is chosen intentionally so that it can be used with
   * PubSubClient directly as PubSubClient's callback.
   *
   * @param topic MQTT topic.
   * @param payload payload of the MQTT message (NUL-terminated after length).
   * @param length length of the payload.
   */
  static void mqttMessageReceived(char* topic, uint8_t* payload, unsigned int length);

private:
  /*!
   * @brief Try to handle received message.
   *
   * @param topic MQTT topic.
   * @param s payload of the MQTT message (NUL-terminated string view).
   * @return @c true, if the message was handled, @c false otherwise (e.g., for other component).
   */
  virtual bool mqttReceiveMsg(const StringView& topic, const StringView& s) = 0;

  MessageHandler* next_;
  const __FlashStringHelper* name_;
  static MessageHandler* s_first_handler;
  static publish_callback s_cb_;
  static void *s_cb_arg_;
  static bool s_debug_;
};

template<typename TopicType, typename PayloadType, typename... Args>
void PublishTask::publish(const TopicType& topic, PayloadType payload, Args... args) {
  return publish([&topic, payload, args...]() -> bool {
    return MessageHandler::publish(topic, payload, args...);
  });
}
