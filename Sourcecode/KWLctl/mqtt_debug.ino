void mqtt_debug_fan1() {
  String out = "";
  out =  "Timestamp: " + millis();
  out += ", tachoFan1TimeSum: ";
  out += tachoFan1TimeSum;
  out += ", Umdrehungen pro Minute: ";
  out += speedTachoFan1;
  out += ", number of Pulse:  ";
  out += cycleFan1Counter;
  out += ", Intervall (mS): ";
  out += (currentMillis - previousMillisFan);      
  char _buffer[out.length()];
  out.toCharArray(_buffer,out.length()); 
  mqttClient.publish(TOPICKwlDebugstateFan1, _buffer);
}

void mqtt_debug_fan2() {
  String out = "";
  out =  "Timestamp: " + millis();
  out += ", tachoFan2TimeSum: ";
  out += tachoFan2TimeSum;
  out += ", Umdrehungen pro Minute: ";
  out += speedTachoFan2;
  out += ", number of Pulse:  ";
  out += cycleFan2Counter;
  out += ", Intervall (mS): ";
  out += (currentMillis - previousMillisFan);   
  char _buffer[out.length()];
  out.toCharArray(_buffer,out.length()); 
  mqttClient.publish(TOPICKwlDebugstateFan2, _buffer);
}

