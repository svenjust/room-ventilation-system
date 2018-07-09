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


// siehe http://fluuux.de/2013/04/daten-im-eeprom-speichern-auslesen-und-variablen-zuweisen/

/************************************************************
  Funktionen zum Lesen und Schreiben des EEPROMS            *
  **********************************************************/

// Speicherbereich löschen
void eeprom_erase_all(byte b = 0xFF) {
  int i;
  for (i = EEPROM_MIN_ADDR; i <= EEPROM_MAX_ADDR; i++) {
    EEPROM.write(i, b);
  }
}

// Speicherbereich auslesen
// gibt einen Dump des Speicherinhalts in tabellarischer Form über den seriellen Port aus.
void eeprom_serial_dump_table(int bytesPerRow = 16) {
  int i;
  int j;
  byte b;
  char buf[10];
  j = 0;

  for (i = EEPROM_MIN_ADDR; i <= EEPROM_MAX_ADDR; i++) {
    if (j == 0) {
      sprintf(buf, "%03X: ", i);
      Serial.print(buf);
    }
    b = EEPROM.read(i);
    sprintf(buf, "%02X ", b);
    j++;
    if (j == bytesPerRow) {
      j = 0;
      Serial.println(buf);
    } else Serial.print(buf);
  }
}

//Schreibt eine Folge von Bytes, beginnend an der angegebenen Adresse.
//Gibt True zurück wenn das gesamte Array geschrieben wurde,
//Gibt False zurück wenn Start- oder Endadresse nicht zwischen dem minimal und maximal zulässigen Bereich liegt.
//Wenn False zurückgegeben wurde, wurde nichts geschrieben
boolean eeprom_write_bytes(int startAddr, const byte* array, int numBytes) {
  int i;

  if (!eeprom_is_addr_ok(startAddr) || !eeprom_is_addr_ok(startAddr + numBytes)) return false;

  for (i = 0; i < numBytes; i++) {
    EEPROM.write(startAddr + i, array[i]);
  }  return true;
}

//Schreibt einen int-Wert an der angegebenen Adresse.
boolean eeprom_write_int(int addr, int value) {
  byte *ptr;

  ptr = (byte*)&value;
  return eeprom_write_bytes(addr, ptr, sizeof(value));
}

//Liest einen Integer Wert an der angegebenen Adresse
boolean eeprom_read_int(int addr, int* value) {
  return eeprom_read_bytes(addr, (byte*)value, sizeof(int));
}

//Liest die angegebene Anzahl von Bytes an der angegebenen Adresse
boolean eeprom_read_bytes(int startAddr, byte array[], int numBytes) {
  int i;

  if (!eeprom_is_addr_ok(startAddr) || !eeprom_is_addr_ok(startAddr + numBytes)) return false;

  for (i = 0; i < numBytes; i++) {
    array[i] = EEPROM.read(startAddr + i);
  } return true;
}

//Liefert True wenn die angegebene Adresse zwischen dem minimal
//und dem maximal zulässigen Bereich liegt.
//Wird durch andere übergeordnete Funktionen aufgerufen um Fehler
//zu vermeiden.
boolean eeprom_is_addr_ok(int addr) {
  return ((addr >= EEPROM_MIN_ADDR) && (addr <= EEPROM_MAX_ADDR));
}

//Schreibt einen String, beginnend bei der angegebenen Adresse
boolean eeprom_write_string(int addr, const char* string) {
  int numBytes;
  numBytes = strlen(string) + 1;

  return eeprom_write_bytes(addr, (const byte*)string, numBytes);
}

//Liest einen String ab der angegebenen Adresse
boolean eeprom_read_string(int addr, char* eeprombuffer, int bufSize) {
  byte ch;
  int bytesRead;

  if (!eeprom_is_addr_ok(addr)) return false;
  if (bufSize == 0) return false;

  if (bufSize == 1) {
    eeprombuffer[0] = 0;
    return true;
  }

  bytesRead = 0;
  ch = EEPROM.read(addr + bytesRead);
  eeprombuffer[bytesRead] = ch;
  bytesRead++;

  while ((ch != 0x00) && (bytesRead < bufSize) && ((addr + bytesRead) <= EEPROM_MAX_ADDR)) {
    ch = EEPROM.read(addr + bytesRead);
    eeprombuffer[bytesRead] = ch;
    bytesRead++;
  }

  if ((ch != 0x00) && (bytesRead >= 1)) eeprombuffer[bytesRead - 1] = 0;

  return true;
}

/**************************************************
  Wenn WRITE_EEPROM = true oder statusFlag != 49  *
  dann EEPROM löschen und Standardwerte schreiben *
  ************************************************/
void initializeEEPROM() {
  initializeEEPROM(false);
}

void initializeEEPROM(boolean EraseMemory)
{
  int i;
  int statusFlag = EEPROM.read(0);

  Serial.print("StatusFlag ist: ");
  Serial.println(statusFlag);

  Serial.println("Lese EEPROM...");
  eeprom_serial_dump_table();

  if (FACTORY_RESET_EEPROM || statusFlag != 49 || EraseMemory)
  {
    Serial.println("Loesche Speicherbereich...");
    eeprom_erase_all();
    delay(1000);

    Serial.println("Lese EEPROM...");
    eeprom_serial_dump_table();

    delay(1000);

    Serial.println("Schreibe Speicherbereich...");

    strcpy(eeprombuffer, "1"); //Status-FLAG (0-1) 1 Zeichen
    eeprom_write_string(0, eeprombuffer);

    // Normdrehzahl Lüfter 1
    eeprom_write_int(2, defStandardSpeedSetpointFan1);
    
    // Normdrehzahl Lüfter 2
    eeprom_write_int(4, defStandardSpeedSetpointFan2);

    // bypassTempAbluftMin
    eeprom_write_int(6, defStandardBypassTempAbluftMin);

    // bypassTempAussenluftMin
    eeprom_write_int(8, defStandardBypassTempAussenluftMin);

    // bypassHystereseMinutes
    eeprom_write_int(10, defStandardBypassHystereseMinutes);

    // antifreezeHyst 3
    eeprom_write_int(12, defStandardBypassHystereseTemp);

    // bypassManualSetpoint
    eeprom_write_int(14, defStandardBypassManualSetpoint);

    // bypassMode
    eeprom_write_int(16, defStandardBypassMode);

    // PWM für max 10 Lüftungsstufen und zwei Lüfter und einem Integer
    // max 10 Werte * 2 Lüfter * 2 Byte
    // 20 bis 60
    
    if (defStandardModeCnt > 10) {
      Serial.println("ERROR: ModeCnt zu groß");
    }
    for (int i = 0; ((i < defStandardModeCnt) && (i < 10)); i++) {
     /* Serial.print (F("Berechnung fuer Stufe: "));
      Serial.println (i);
      Serial.print("Wert Fan 1: ");
      Serial.println((int)(defStandardSpeedSetpointFan1 * defStandardKwlModeFactor[i] * 1000 / defStandardNenndrehzahlFan));
      Serial.print("Wert Fan 2: ");
      Serial.println((int)(defStandardSpeedSetpointFan2 * defStandardKwlModeFactor[i] * 1000 / defStandardNenndrehzahlFan));
      */
      eeprom_write_int(20 + (i * 4), (int)(defStandardSpeedSetpointFan1 * defStandardKwlModeFactor[i] * 1000 / defStandardNenndrehzahlFan));
      eeprom_write_int(22 + (i * 4), (int)(defStandardSpeedSetpointFan2 * defStandardKwlModeFactor[i] * 1000 / defStandardNenndrehzahlFan));
    }
    // ENDE PWM für max 10 Lüftungsstufen

    // heatingAppCombUse
    eeprom_write_int(60, defStandardHeatingAppCombUse);
    
    // Weiter geht es ab Speicherplatz 62dez ff
    
    delay(1000);

    Serial.println("Lese Speicherbereich...");
    eeprom_serial_dump_table();

    // erase eeprombuffer
    for (i = 0; i < BUFSIZE; i++)
    {
      eeprombuffer[i] = 0;
    }
  }
}
