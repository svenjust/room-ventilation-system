void mqtt_debug_fan1() {
  String out = "";
  out =  "Fan1 - M: ";
  out += millis();
  out += ", Gap1: ";
  out += (speedTachoFan1 - speedSetpointFan1);
  out += ", tsf1: ";
  out += techSetpointFan1;
  out += ", ssf1: ";
  out += speedSetpointFan1;
  char _buffer[out.length()];
  out.toCharArray(_buffer,out.length()); 
  mqttClient.publish(TOPICKwlDebugstateFan1, _buffer);
}

void mqtt_debug_fan2() {
  String out = "";
  out =  "Fan2 - M: ";
  out += millis();
  out += ", Gap2: ";
  out += (speedTachoFan2 - speedSetpointFan2);
  out += ", tsf2: ";
  out += techSetpointFan2;
  out += ", ssf2: ";
  out += speedSetpointFan2;
  char _buffer[out.length()];
  out.toCharArray(_buffer,out.length()); 
  mqttClient.publish(TOPICKwlDebugstateFan2, _buffer);
}

void mqtt_debug_Preheater() {
  String out = "";
  out =  "Preheater - M: ";
  out += millis();
  out += ", Gap: ";
  out += abs(antifreezeTempUpperLimit - TEMP4_Fortluft);
  out += ", techSetpointPreheater: ";
  out += techSetpointPreheater;
  char _buffer[out.length()];
  out.toCharArray(_buffer,out.length()); 
  mqttClient.publish(TOPICKwlDebugstatePreheater, _buffer);
}
