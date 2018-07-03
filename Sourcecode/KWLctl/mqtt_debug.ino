/*
################################################################
#
#   Copyright notice
#
#   Control software for a Room Ventilation System
#   https://github.com/svenjust/room-ventilation-system
#    
#   Copyright (C) 2018  Sven Just (sven@familie-just.de)
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
#   This copyright notice MUST APPEAR in all copies of the script!
#
################################################################
*/

void mqtt_debug_Preheater() {
  String out = "";
  out =  "Preheater - M: ";
  out += millis();
  out += ", Gap: ";
  out += abs(antifreezeTempUpperLimit - tempSensors.get_t4_exhaust());
  out += ", techSetpointPreheater: ";
  out += techSetpointPreheater;
  char _buffer[out.length()];
  out.toCharArray(_buffer,out.length()); 
  mqttClient.publish(MQTTTopic::KwlDebugstatePreheater, _buffer);
}
