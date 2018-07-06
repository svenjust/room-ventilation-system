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
 * @brief Client to communicate with MQTT protocol.
 */
// TODO move ethernet init and client handling here
class MQTTClient
{
public:
  /// Construct MQTT client.
  explicit MQTTClient(PubSubClient& client);

  // TODO remove after MQTT client cleaned up.
  static void mqttReceiveMsg(char* topic, uint8_t* payload, unsigned int length);

  /// Get global instance of the MQTT client.
  static PubSubClient& getClient() { return *client_; }

private:
  static PubSubClient* client_;
};
