12.01.2019 Version HW-0.24, nur P300_HiVolt, in die Platinenerstellung gegeben

Änderungen von HW-0.23 zu HW-0.24 - P300_HiVolt
 - Anschluss zu Relais per Schraubklemmen, anstatt Molex Stecker, wegen der schwierigen Beschaffung und Crimpen der Molex Verbindung
 - Alternative für 5V Spannung Meanwell IRM-05-5 mit 5W(1A) Leistung hinzugefügt, empfohlen bei Verwendung Display
 - Platine 3mm verlängert
 - Bohr in Mitte/unten hinzugefügt
 - Leiterbahnen zu den Lüftern auf 2mm verbreitert

09.01.2019 Version HW-0.23 in die Platinenerstellung gegeben

Änderungen von HW-0.22 zu HW-0.23 - P300_Arduino_IF
 - Linke Seite der Platine vergrößert auf ArduinoMega-Größe
 - Stecker für Displayanschluss ergänzt
 - Fünf weitere Bohrungen für Befestigung ergänzt
 - Leiterbahnen im größeren Abstand zu Durchbohrungen und Bauteilen verlegt
 - Connector für J12 I2C nach links verschoben, damit ausreichend Platz für JTS Stecker
 - Auf dem Stecker J4 sind die beiden Lüfteranschlüsse Lüfter 1 und Lüfter 2 getauscht. Damit ist der Stecker kompatibel zu Pluggit. Der Pluggit-Lüfter-Steuerstecker kann direkt aufgesteckt werden, wenn Molex Stiftleiste - KK - 1x8-polig - Stecker, Molex-Best-Nr. 22272081 verwendet wird.
 - Auf dem Stecker J4 (Lüfteranschluss) die vergessenen Masseverbindungen Pin 4 und Pin 8 ergänzt.

Änderungen von HW-0.22 zu HW-0.23 - P300_HiVolt
 - 230V Lüfteranschluss kompatibel zu Pluggit, Molex Stiftleiste - Mini-Fit Jr - 2x4-polig - Stecker, Molex-Best-Nr. 39281083
 - Stecker J3, J4 für Relais auf Molex Stiftleiste - Mini-Fit Jr - 2x3-polig - Stecker, Molex-Best-Nr. 39288060 geändert.
 - Änderung der Belegung der Stecker J3, J4 für die Verwendung es Relaisboards mit den Ralaisanschlüssen: Mitte = Stromzuführung, links = Nicht geschaltet, rechts geschaltet
 - Räumliche Trennung von 230V und Niedervoltspannungen auf der Platine
 - Bohrungen Pins Varistor auf 1mm vergrößert
 
Neu HW-0.23 - P300_Display_IF
 - Platine zum Aufstecken des Displays für die räumlich getrennte Montage von Display zu P300_Arduino_IF
 - Bohrlöcher für Befestigung hinzugefügt

Änderungen von HW-0.20 zu HW-0.21 - P300_Arduino_IF
 - Pins für Relais auf die ursprünglichen Pins zurückgelegt:
   D40 = BP_DIR, D41 = BP_POW, D42 = L2, D43 = L1
 - Pins für PWM-Signal Lüfter auf ursprüngliche Belegung zurückgelegt
   D44 = PWM_L1, D46 = PWM_L2