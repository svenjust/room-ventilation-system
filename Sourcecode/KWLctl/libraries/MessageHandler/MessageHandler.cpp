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

#include "MessageHandler.h"

#include <PubSubClient.h>
#include <stdlib.h>

PublishTask* PublishTask::s_first_task_ = nullptr;
bool PublishTask::s_has_tasks_ = false;

MessageHandler* MessageHandler::s_first_handler = nullptr;
PubSubClient* MessageHandler::s_client_ = nullptr;
bool MessageHandler::s_debug_ = false;

PublishTask::PublishTask() :
  next_(s_first_task_)
{
  s_first_task_ = this;
}

bool PublishTask::loop()
{
  bool retval = false;
  auto cur = s_first_task_;
  while (cur) {
    if (cur->invoker_) {
      retval = true;
      auto res = cur->invoker_(cur->closure_space_);
      if (res)
        cur->invoker_ = nullptr;  // sent successfully
    }
    cur = cur->next_;
  }
  s_has_tasks_ = retval;
  return retval;
}

MessageHandler::MessageHandler(const __FlashStringHelper* name) :
  next_(s_first_handler),
  name_(name)
{
  s_first_handler = this;
}

MessageHandler::~MessageHandler() {}

void MessageHandler::begin(PubSubClient& client, bool debug)
{
  if (s_client_)
    s_client_->setCallback(nullptr);
  s_client_ = &client;
  s_debug_ = debug;
  client.setCallback(&MessageHandler::mqttMessageReceived);
}

bool MessageHandler::publish(const char* topic, const char* payload, bool retained)
{
  if (s_debug_) {
    Serial.print(F("MQTT send "));
    Serial.print(topic);
    Serial.print(':');
    Serial.print(' ');
    Serial.print(payload);
    if (retained)
      Serial.print(F(" [retained]"));
    Serial.println();
  }
  if (s_client_)
    return s_client_->publish(topic, payload, retained);
  else
    return false;
}

bool MessageHandler::publish(const char* topic, const __FlashStringHelper* payload, bool retained)
{
  auto len = strlen_P(reinterpret_cast<const char*>(payload));
  char buffer[len + 1];
  memcpy_P(buffer, reinterpret_cast<const char*>(payload), len + 1);
  return publish(topic, buffer, retained);
}

bool MessageHandler::publish(const char* topic, long payload, bool retained)
{
  char buffer[16];
  ltoa(payload, buffer, 10);
  return publish(topic, buffer, retained);
}

bool MessageHandler::publish(const char* topic, unsigned long payload, bool retained)
{
  char buffer[16];
  ultoa(payload, buffer, 10);
  return publish(topic, buffer, retained);
}

bool MessageHandler::publish(const char* topic, double payload, unsigned char precision, bool retained)
{
  char buffer[32];
  dtostrf(payload, 1, precision, buffer);
  return publish(topic, buffer, retained);
}

void MessageHandler::mqttMessageReceived(char* topic, uint8_t* payload, unsigned int length)
{
  payload[length] = 0;  // ensure NUL termination
  const StringView topicStr(topic);
  const StringView s(reinterpret_cast<const char*>(payload), length);
  if (s_debug_) {
    Serial.print(F("MQTT receive ["));
    Serial.write(topicStr.c_str(), topicStr.length());
    Serial.print(F("]: ["));
    Serial.write(payload, length);
    Serial.println(']');
  }

  auto handler = s_first_handler;
  while (handler) {
    if (s_debug_) {
      Serial.print(F("- trying MQTT handler: "));
      Serial.println(handler->name_);
    }
    if (handler->mqttReceiveMsg(topicStr, s)) {
      if (s_debug_) {
        Serial.print(F("MQTT message handled by: "));
        Serial.println(handler->name_);
      }
      return;
    }
    handler = handler->next_;
  }

  if (s_debug_) {
    Serial.println(F("Unexpected MQTT message received, no handler found"));
  }
}
