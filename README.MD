# Steuerung einer Lüftungsanlage für Wohnhäuser

Diese Steuerung wurde als Ersatz der Originalsteuerung entwickelt.

Das Original war Pluggit P300
  
Es werden:
1. zwei Lüfter angesteuert und deren Tachosignale ausgelesen
2. vier Temperatursensoren abgefragt
3. Steuerung Abluftventilator zum Frostschutz
4. Optional: Vorheizregister als Frostschutz 
5. Optional: Sommer-Bypass geschaltet
6. Optional: Vorheizregister als Frostschutz

Die Drehzahlregelung der Lüftermotoren erfolgt durch PID-Regler. Dies sorgt dafür, das die geforderten Drehzahlen der Lüfter sicher  erreicht werden. Die Leistungssteuerung des Vorheizregisters erfolgt durch einen PID-Regler.

Differenzdrucksensoren werden bisher nicht berücksichtigt.

Die Steuerung ist per LAN (W5100) erreichbar. Als Protokoll wird mqtt verwendet. Über mqtt liefert die Steuerung Temperaturen,    Drehzahlen der Lüfter, Stellung der Bypassklappe und den Status der AntiFreeze Funktion zurück.

Codeteile stammen von:
- Tachosignal auslesen wie
  https://forum.arduino.cc/index.php?topic=145226.msg1102102#msg1102102
 
 - ntpClient, nonblocking, auch im Fehlerfall
   Der Standard Arduino ntpClient blockiert im Fehlerfall für mindestens eine Sekunde
   https://github.com/blackketter/NTPClient

- Tachosignal auslesen wie
https://forum.arduino.cc/index.php?topic=145226.msg1102102#msg1102102
