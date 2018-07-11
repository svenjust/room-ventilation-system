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
#include "NetworkClient.h"
#include "KWLConfig.h"

#include <PubSubClient.h>
#include <stdlib.h>

MessageHandler* MessageHandler::s_first_handler = nullptr;

MessageHandler::MessageHandler() : next_(s_first_handler)
{
  s_first_handler = this;
}

MessageHandler::~MessageHandler() {}

bool MessageHandler::publish(const char* topic, const char* payload, bool retained)
{
  if (KWLConfig::serialDebug) {
    Serial.print(F("MQTT send "));
    Serial.print(topic);
    Serial.print(':');
    Serial.print(' ');
    Serial.print(payload);
    if (retained)
      Serial.print(F(" [retained]"));
    Serial.println();
  }
  if (NetworkClient::hasClient())
    return NetworkClient::getClient().publish(topic, payload, retained);
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
  char buffer[16];
  dtostrf(payload, sizeof(buffer) - 1, precision, buffer);
  return publish(topic, buffer, retained);
}

void MessageHandler::mqttMessageReceived(char* topic, uint8_t* payload, unsigned int length)
{
  payload[length] = 0;  // ensure NUL termination
  const StringView topicStr(topic);
  if (KWLConfig::serialDebug) {
    Serial.print(F("MQTT receive "));
    Serial.write(topicStr.c_str(), topicStr.length());
    Serial.print(':');
    Serial.print(' ');
    Serial.write(payload, length);
    Serial.println();
  }

  auto handler = s_first_handler;
  while (handler) {
    if (handler->mqttReceiveMsg(topicStr, reinterpret_cast<const char*>(payload), length))
      return;
    handler = handler->next_;
  }

  if (KWLConfig::serialDebug) {
    Serial.println(F("Unexpected MQTT message received"));
  }
}
