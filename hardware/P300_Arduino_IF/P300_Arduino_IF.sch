EESchema Schematic File Version 4
LIBS:P300_Arduino_IF-cache
EELAYER 26 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title "Ersatzsteuerung für Pluggit P300/P450 Lüftungsanlage"
Date "2019-01-30"
Rev "V0.26-dev"
Comp "sven@familie-just.de"
Comment1 "https://github.com/svenjust/room-ventilation-system"
Comment2 "basiert auf: https://github.com/herrfrei/pluggit-ctrl"
Comment3 "optional: Horter DAC 0-10V, 2 x DHT22, 4fach Relais-Shield"
Comment4 "Arduino Mega, LAN-Shield, 4 x OneWire DS18B20, 2 x Optokoppler"
$EndDescr
$Comp
L P300_Arduino_IF-rescue:PC817-Isolator U1
U 1 1 5B783719
P 8100 5000
F 0 "U1" H 8100 5325 50  0000 C CNN
F 1 "PC817" H 8100 5234 50  0000 C CNN
F 2 "Package_DIP:DIP-4_W7.62mm" H 7900 4800 50  0001 L CIN
F 3 "http://www.soselectronic.cz/a_info/resource/d/pc817.pdf" H 8100 5000 50  0001 L CNN
	1    8100 5000
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:PC817-Isolator U2
U 1 1 5B783787
P 8100 5550
F 0 "U2" H 8100 5875 50  0000 C CNN
F 1 "PC817" H 8100 5784 50  0000 C CNN
F 2 "Package_DIP:DIP-4_W7.62mm" H 7900 5350 50  0001 L CIN
F 3 "http://www.soselectronic.cz/a_info/resource/d/pc817.pdf" H 8100 5550 50  0001 L CNN
	1    8100 5550
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:Conn_01x03-Connector_Generic J5
U 1 1 5B7839E7
P 8750 1550
F 0 "J5" H 8830 1592 50  0000 L CNN
F 1 "TEMP1" H 8830 1501 50  0000 L CNN
F 2 "Connector_Molex:Molex_KK-254_AE-6410-03A_1x03_P2.54mm_Vertical" H 8750 1550 50  0001 C CNN
F 3 "~" H 8750 1550 50  0001 C CNN
	1    8750 1550
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:Conn_01x03-Connector_Generic J6
U 1 1 5B783D81
P 8750 1950
F 0 "J6" H 8830 1992 50  0000 L CNN
F 1 "TEMP2" H 8830 1901 50  0000 L CNN
F 2 "Connector_Molex:Molex_KK-254_AE-6410-03A_1x03_P2.54mm_Vertical" H 8750 1950 50  0001 C CNN
F 3 "~" H 8750 1950 50  0001 C CNN
	1    8750 1950
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:Conn_01x03-Connector_Generic J7
U 1 1 5B783D9F
P 8750 2300
F 0 "J7" H 8830 2342 50  0000 L CNN
F 1 "TEMP3" H 8830 2251 50  0000 L CNN
F 2 "Connector_Molex:Molex_KK-254_AE-6410-03A_1x03_P2.54mm_Vertical" H 8750 2300 50  0001 C CNN
F 3 "~" H 8750 2300 50  0001 C CNN
	1    8750 2300
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:Conn_01x03-Connector_Generic J8
U 1 1 5B783DCB
P 8750 2650
F 0 "J8" H 8830 2692 50  0000 L CNN
F 1 "TEMP4" H 8830 2601 50  0000 L CNN
F 2 "Connector_Molex:Molex_KK-254_AE-6410-03A_1x03_P2.54mm_Vertical" H 8750 2650 50  0001 C CNN
F 3 "~" H 8750 2650 50  0001 C CNN
	1    8750 2650
	1    0    0    -1  
$EndComp
Wire Wire Line
	8550 1850 8200 1850
Wire Wire Line
	8200 1850 8200 1450
Wire Wire Line
	8550 2200 8200 2200
Wire Wire Line
	8200 2200 8200 1850
Connection ~ 8200 1850
Wire Wire Line
	8550 2550 8200 2550
Wire Wire Line
	8200 2550 8200 2200
Connection ~ 8200 2200
$Comp
L P300_Arduino_IF-rescue:GND-power #PWR0101
U 1 1 5B7848DF
P 3100 5200
F 0 "#PWR0101" H 3100 4950 50  0001 C CNN
F 1 "GND" H 3105 5027 50  0000 C CNN
F 2 "" H 3100 5200 50  0001 C CNN
F 3 "" H 3100 5200 50  0001 C CNN
	1    3100 5200
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:GND-power #PWR0102
U 1 1 5B78490F
P 8200 2850
F 0 "#PWR0102" H 8200 2600 50  0001 C CNN
F 1 "GND" H 8205 2677 50  0000 C CNN
F 2 "" H 8200 2850 50  0001 C CNN
F 3 "" H 8200 2850 50  0001 C CNN
	1    8200 2850
	1    0    0    -1  
$EndComp
Wire Wire Line
	3100 5200 3650 5200
$Comp
L P300_Arduino_IF-rescue:+5V-power #PWR0103
U 1 1 5B785E95
P 3150 5800
F 0 "#PWR0103" H 3150 5650 50  0001 C CNN
F 1 "+5V" H 3165 5973 50  0000 C CNN
F 2 "" H 3150 5800 50  0001 C CNN
F 3 "" H 3150 5800 50  0001 C CNN
	1    3150 5800
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:+5V-power #PWR0104
U 1 1 5B78648F
P 8350 1350
F 0 "#PWR0104" H 8350 1200 50  0001 C CNN
F 1 "+5V" H 8365 1523 50  0000 C CNN
F 2 "" H 8350 1350 50  0001 C CNN
F 3 "" H 8350 1350 50  0001 C CNN
	1    8350 1350
	1    0    0    -1  
$EndComp
Wire Wire Line
	8350 1350 8350 1650
Wire Wire Line
	8350 1650 8550 1650
Wire Wire Line
	8550 2050 8350 2050
Wire Wire Line
	8350 2050 8350 1650
Connection ~ 8350 1650
Wire Wire Line
	8550 2400 8350 2400
Wire Wire Line
	8350 2400 8350 2050
Connection ~ 8350 2050
Wire Wire Line
	8550 2750 8350 2750
Wire Wire Line
	8350 2750 8350 2400
Connection ~ 8350 2400
Wire Wire Line
	8200 2850 8200 2550
Connection ~ 8200 2550
Wire Wire Line
	8550 1450 8200 1450
Wire Wire Line
	8100 1650 8350 1650
Wire Wire Line
	8100 2050 8350 2050
Wire Wire Line
	8100 2400 8350 2400
Wire Wire Line
	8100 2750 8350 2750
Connection ~ 8350 2750
$Comp
L P300_Arduino_IF-rescue:GND-power #PWR0105
U 1 1 5B791D0B
P 7700 5750
F 0 "#PWR0105" H 7700 5500 50  0001 C CNN
F 1 "GND" H 7705 5577 50  0000 C CNN
F 2 "" H 7700 5750 50  0001 C CNN
F 3 "" H 7700 5750 50  0001 C CNN
	1    7700 5750
	1    0    0    -1  
$EndComp
Wire Wire Line
	7700 5750 7700 5650
Wire Wire Line
	7700 5100 7800 5100
Wire Wire Line
	7800 5650 7700 5650
Connection ~ 7700 5650
$Comp
L P300_Arduino_IF-rescue:Conn_01x08-Connector_Generic J4
U 1 1 5B7977EE
P 9050 5200
F 0 "J4" H 9100 5500 50  0000 L CNN
F 1 "Fan control" V 9150 4950 50  0000 L CNN
F 2 "Connector_Molex:Molex_KK-254_AE-6410-08A_1x08_P2.54mm_Vertical" H 9050 5200 50  0001 C CNN
F 3 "~" H 9050 5200 50  0001 C CNN
	1    9050 5200
	1    0    0    -1  
$EndComp
Wire Wire Line
	8850 5000 8600 5000
Wire Wire Line
	8600 5000 8600 5100
Wire Wire Line
	8600 5100 8400 5100
Wire Wire Line
	8850 5100 8700 5100
Wire Wire Line
	8700 5100 8700 4900
Wire Wire Line
	8700 4900 8400 4900
Wire Wire Line
	8850 5400 8600 5400
Wire Wire Line
	8600 5400 8600 5650
Wire Wire Line
	8600 5650 8400 5650
Wire Wire Line
	8850 5500 8500 5500
Wire Wire Line
	8500 5500 8500 5450
Wire Wire Line
	8500 5450 8400 5450
Text GLabel 7500 4650 0    50   Input ~ 0
Tacho_L2
Text GLabel 7450 5500 0    50   Input ~ 0
Tacho_L1
Text GLabel 3400 2100 0    50   Input ~ 0
Tacho_L1
Text GLabel 3400 2000 0    50   Input ~ 0
Tacho_L2
$Comp
L P300_Arduino_IF-rescue:Conn_01x06-Connector_Generic J1
U 1 1 5B7A1E69
P 10600 5550
F 0 "J1" H 10679 5542 50  0000 L CNN
F 1 "Relay brd" H 10679 5451 50  0000 L CNN
F 2 "Connector_Molex:Molex_KK-254_AE-6410-06A_1x06_P2.54mm_Vertical" H 10600 5550 50  0001 C CNN
F 3 "~" H 10600 5550 50  0001 C CNN
	1    10600 5550
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:+5V-power #PWR0106
U 1 1 5B7BA26C
P 10300 5150
F 0 "#PWR0106" H 10300 5000 50  0001 C CNN
F 1 "+5V" H 10315 5323 50  0000 C CNN
F 2 "" H 10300 5150 50  0001 C CNN
F 3 "" H 10300 5150 50  0001 C CNN
	1    10300 5150
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:GND-power #PWR0107
U 1 1 5B7BA2CA
P 10150 6000
F 0 "#PWR0107" H 10150 5750 50  0001 C CNN
F 1 "GND" H 10155 5827 50  0000 C CNN
F 2 "" H 10150 6000 50  0001 C CNN
F 3 "" H 10150 6000 50  0001 C CNN
	1    10150 6000
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:R-Device R2
U 1 1 5B7BEF96
P 7950 2050
F 0 "R2" V 7743 2050 50  0000 C CNN
F 1 "4.7k" V 7834 2050 50  0000 C CNN
F 2 "Resistors_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical" V 7880 2050 50  0001 C CNN
F 3 "~" H 7950 2050 50  0001 C CNN
	1    7950 2050
	0    1    1    0   
$EndComp
$Comp
L P300_Arduino_IF-rescue:R-Device R3
U 1 1 5B7BEFD4
P 7950 2400
F 0 "R3" V 7743 2400 50  0000 C CNN
F 1 "4.7k" V 7834 2400 50  0000 C CNN
F 2 "Resistors_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical" V 7880 2400 50  0001 C CNN
F 3 "~" H 7950 2400 50  0001 C CNN
	1    7950 2400
	0    1    1    0   
$EndComp
$Comp
L P300_Arduino_IF-rescue:R-Device R4
U 1 1 5B7BF00C
P 7950 2750
F 0 "R4" V 7743 2750 50  0000 C CNN
F 1 "4.7k" V 7834 2750 50  0000 C CNN
F 2 "Resistors_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical" V 7880 2750 50  0001 C CNN
F 3 "~" H 7950 2750 50  0001 C CNN
	1    7950 2750
	0    1    1    0   
$EndComp
$Comp
L P300_Arduino_IF-rescue:R-Device R5
U 1 1 5B7BF166
P 7600 5050
F 0 "R5" V 7393 5050 50  0000 C CNN
F 1 "220" V 7484 5050 50  0000 C CNN
F 2 "Resistors_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical" V 7530 5050 50  0001 C CNN
F 3 "~" H 7600 5050 50  0001 C CNN
	1    7600 5050
	-1   0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:R-Device R6
U 1 1 5B7BF281
P 7600 5600
F 0 "R6" V 7393 5600 50  0000 C CNN
F 1 "220" V 7484 5600 50  0000 C CNN
F 2 "Resistors_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical" V 7530 5600 50  0001 C CNN
F 3 "~" H 7600 5600 50  0001 C CNN
	1    7600 5600
	-1   0    0    -1  
$EndComp
Wire Wire Line
	7800 2750 7700 2750
Wire Wire Line
	7700 2750 7700 2650
Wire Wire Line
	7700 2650 8550 2650
Wire Wire Line
	7800 2400 7700 2400
Wire Wire Line
	7700 2400 7700 2300
Wire Wire Line
	7700 2300 8550 2300
Wire Wire Line
	7700 1650 7700 1550
Wire Wire Line
	7700 1550 8550 1550
Wire Wire Line
	7800 2050 7700 2050
Wire Wire Line
	7700 2050 7700 1950
$Comp
L P300_Arduino_IF-rescue:Conn_01x03-Connector_Generic J2
U 1 1 5B7E1BF6
P 10550 1450
F 0 "J2" H 10629 1492 50  0000 L CNN
F 1 "DHT1" H 10629 1401 50  0000 L CNN
F 2 "Connector_Molex:Molex_KK-254_AE-6410-03A_1x03_P2.54mm_Vertical" H 10550 1450 50  0001 C CNN
F 3 "~" H 10550 1450 50  0001 C CNN
	1    10550 1450
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:GND-power #PWR0108
U 1 1 5B7E1C65
P 10150 3650
F 0 "#PWR0108" H 10150 3400 50  0001 C CNN
F 1 "GND" H 10155 3477 50  0000 C CNN
F 2 "" H 10150 3650 50  0001 C CNN
F 3 "" H 10150 3650 50  0001 C CNN
	1    10150 3650
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:+5V-power #PWR0109
U 1 1 5B7E1D55
P 10250 1300
F 0 "#PWR0109" H 10250 1150 50  0001 C CNN
F 1 "+5V" H 10265 1473 50  0000 C CNN
F 2 "" H 10250 1300 50  0001 C CNN
F 3 "" H 10250 1300 50  0001 C CNN
	1    10250 1300
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:Conn_01x03-Connector_Generic J3
U 1 1 5B7E81A3
P 10550 1800
F 0 "J3" H 10629 1842 50  0000 L CNN
F 1 "DHT2" H 10629 1751 50  0000 L CNN
F 2 "Connector_Molex:Molex_KK-254_AE-6410-03A_1x03_P2.54mm_Vertical" H 10550 1800 50  0001 C CNN
F 3 "~" H 10550 1800 50  0001 C CNN
	1    10550 1800
	1    0    0    -1  
$EndComp
Wire Wire Line
	7700 5100 7700 5650
Wire Wire Line
	8850 4650 8850 4900
Text GLabel 6350 5400 2    50   Input ~ 0
PWM_L2
Text GLabel 6350 5200 2    50   Input ~ 0
PWM_L1
Text GLabel 7450 5200 0    50   Input ~ 0
PWM_L2
Text GLabel 7450 5750 0    50   Input ~ 0
PWM_L1
Wire Wire Line
	7600 5450 7800 5450
Wire Wire Line
	7450 5750 7600 5750
Wire Wire Line
	10350 1700 10150 1700
Wire Wire Line
	10350 1350 10150 1350
Wire Wire Line
	10150 1350 10150 1700
Connection ~ 10150 1700
Wire Wire Line
	10350 1900 10250 1900
Wire Wire Line
	10250 1900 10250 1550
Wire Wire Line
	10350 1550 10250 1550
Connection ~ 10250 1550
Wire Wire Line
	10250 1550 10250 1300
$Comp
L P300_Arduino_IF-rescue:+12V-power #PWR0110
U 1 1 5B89F9EE
P 3250 6500
F 0 "#PWR0110" H 3250 6350 50  0001 C CNN
F 1 "+12V" H 3265 6673 50  0000 C CNN
F 2 "" H 3250 6500 50  0001 C CNN
F 3 "" H 3250 6500 50  0001 C CNN
	1    3250 6500
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:Conn_01x02-Connector_Generic J9
U 1 1 5B8A319A
P 3050 6900
F 0 "J9" H 2970 6575 50  0000 C CNN
F 1 "12V" H 2970 6666 50  0000 C CNN
F 2 "Connector_Molex:Molex_KK-254_AE-6410-02A_1x02_P2.54mm_Vertical" H 3050 6900 50  0001 C CNN
F 3 "~" H 3050 6900 50  0001 C CNN
	1    3050 6900
	-1   0    0    1   
$EndComp
$Comp
L P300_Arduino_IF-rescue:GND-power #PWR0111
U 1 1 5B8A6A5D
P 3150 6050
F 0 "#PWR0111" H 3150 5800 50  0001 C CNN
F 1 "GND" H 3155 5877 50  0000 C CNN
F 2 "" H 3150 6050 50  0001 C CNN
F 3 "" H 3150 6050 50  0001 C CNN
	1    3150 6050
	1    0    0    -1  
$EndComp
Wire Wire Line
	3400 2000 3650 2000
Wire Wire Line
	3400 2100 3650 2100
$Comp
L P300_Arduino_IF-rescue:Conn_01x04-Connector_Generic J10
U 1 1 5B8B295B
P 2150 3500
F 0 "J10" H 2050 3750 50  0000 L CNN
F 1 "Pressure Analog" V 2250 3200 50  0000 L CNN
F 2 "Connector_Molex:Molex_KK-254_AE-6410-04A_1x04_P2.54mm_Vertical" H 2150 3500 50  0001 C CNN
F 3 "~" H 2150 3500 50  0001 C CNN
	1    2150 3500
	1    0    0    -1  
$EndComp
Text GLabel 1000 3500 0    50   Input ~ 0
DRUCK1
Text GLabel 1000 3600 0    50   Input ~ 0
DRUCK2
$Comp
L P300_Arduino_IF-rescue:R-Device R9
U 1 1 5B8D8614
P 1500 3500
F 0 "R9" V 1293 3500 50  0000 C CNN
F 1 "4.7k" V 1384 3500 50  0000 C CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical" V 1430 3500 50  0001 C CNN
F 3 "~" H 1500 3500 50  0001 C CNN
	1    1500 3500
	0    1    1    0   
$EndComp
Wire Wire Line
	1650 3500 1950 3500
$Comp
L P300_Arduino_IF-rescue:R-Device R10
U 1 1 5B8DC959
P 1700 3600
F 0 "R10" V 1493 3600 50  0000 C CNN
F 1 "4.7k" V 1584 3600 50  0000 C CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical" V 1630 3600 50  0001 C CNN
F 3 "~" H 1700 3600 50  0001 C CNN
	1    1700 3600
	0    1    1    0   
$EndComp
Wire Wire Line
	1850 3600 1950 3600
$Comp
L P300_Arduino_IF-rescue:GND-power #PWR0112
U 1 1 5B8E93AB
P 1800 4000
F 0 "#PWR0112" H 1800 3750 50  0001 C CNN
F 1 "GND" H 1805 3827 50  0000 C CNN
F 2 "" H 1800 4000 50  0001 C CNN
F 3 "" H 1800 4000 50  0001 C CNN
	1    1800 4000
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:R-Device R8
U 1 1 5B8E940A
P 1450 3800
F 0 "R8" V 1243 3800 50  0000 C CNN
F 1 "4.7k" V 1334 3800 50  0000 C CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical" V 1380 3800 50  0001 C CNN
F 3 "~" H 1450 3800 50  0001 C CNN
	1    1450 3800
	-1   0    0    1   
$EndComp
$Comp
L P300_Arduino_IF-rescue:R-Device R7
U 1 1 5B8E9522
P 1150 3800
F 0 "R7" V 943 3800 50  0000 C CNN
F 1 "4.7k" V 1034 3800 50  0000 C CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical" V 1080 3800 50  0001 C CNN
F 3 "~" H 1150 3800 50  0001 C CNN
	1    1150 3800
	-1   0    0    1   
$EndComp
Wire Wire Line
	1000 3500 1150 3500
Wire Wire Line
	1000 3600 1450 3600
Wire Wire Line
	1150 3650 1150 3500
Connection ~ 1150 3500
Wire Wire Line
	1150 3500 1350 3500
Wire Wire Line
	1450 3650 1450 3600
Connection ~ 1450 3600
Wire Wire Line
	1450 3600 1550 3600
Wire Wire Line
	1800 4000 1800 3950
Wire Wire Line
	1800 3700 1950 3700
Wire Wire Line
	1450 3950 1800 3950
Connection ~ 1800 3950
Wire Wire Line
	1800 3950 1800 3700
Wire Wire Line
	1150 3950 1450 3950
Connection ~ 1450 3950
$Comp
L P300_Arduino_IF-rescue:+12V-power #PWR0113
U 1 1 5B90806F
P 1800 3300
F 0 "#PWR0113" H 1800 3150 50  0001 C CNN
F 1 "+12V" H 1815 3473 50  0000 C CNN
F 2 "" H 1800 3300 50  0001 C CNN
F 3 "" H 1800 3300 50  0001 C CNN
	1    1800 3300
	1    0    0    -1  
$EndComp
Text GLabel 3500 4100 0    50   Input ~ 0
DRUCK2
Text GLabel 3500 4000 0    50   Input ~ 0
DRUCK1
Wire Wire Line
	3500 4000 3650 4000
Wire Wire Line
	3500 4100 3650 4100
Text GLabel 6350 4900 2    50   Input ~ 0
BP_DIR
Text GLabel 10050 5550 0    50   Input ~ 0
BP_DIR
Text GLabel 7550 2650 0    50   Input ~ 0
TEMP4
Text GLabel 6350 4100 2    50   Input ~ 0
TEMP4
Text GLabel 6350 4000 2    50   Input ~ 0
TEMP3
Text GLabel 6350 3900 2    50   Input ~ 0
TEMP2
Text GLabel 6350 3800 2    50   Input ~ 0
TEMP1
Text GLabel 7550 1550 0    50   Input ~ 0
TEMP1
Text GLabel 7550 1950 0    50   Input ~ 0
TEMP2
Text GLabel 7550 2300 0    50   Input ~ 0
TEMP3
$Comp
L P300_Arduino_IF-rescue:R-Device R1
U 1 1 5B78726A
P 7950 1650
F 0 "R1" V 7743 1650 50  0000 C CNN
F 1 "4.7k" V 7834 1650 50  0000 C CNN
F 2 "Resistors_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical" V 7880 1650 50  0001 C CNN
F 3 "~" H 7950 1650 50  0001 C CNN
	1    7950 1650
	0    1    1    0   
$EndComp
Wire Wire Line
	7800 1650 7700 1650
Text GLabel 6400 3600 2    50   Input ~ 0
DHT1
Text GLabel 6400 3700 2    50   Input ~ 0
DHT2
Text GLabel 9950 1450 0    50   Input ~ 0
DHT1
Text GLabel 9950 1800 0    50   Input ~ 0
DHT2
Wire Wire Line
	9950 1450 10350 1450
Wire Wire Line
	9950 1800 10350 1800
Wire Wire Line
	6250 3900 6350 3900
Wire Wire Line
	6350 3800 6250 3800
Wire Wire Line
	6250 4000 6350 4000
Text GLabel 6350 4800 2    50   Input ~ 0
BP_POW
Text GLabel 10050 5450 0    50   Input ~ 0
BP_POW
$Comp
L P300_Arduino_IF-rescue:Conn_01x03-Connector_Generic J11
U 1 1 5B7AD27C
P 10550 2250
F 0 "J11" H 10629 2292 50  0000 L CNN
F 1 "TGS2600" H 10629 2201 50  0000 L CNN
F 2 "Connector_Molex:Molex_KK-254_AE-6410-03A_1x03_P2.54mm_Vertical" H 10550 2250 50  0001 C CNN
F 3 "~" H 10550 2250 50  0001 C CNN
	1    10550 2250
	1    0    0    -1  
$EndComp
Wire Wire Line
	10150 1700 10150 2150
Wire Wire Line
	10350 2150 10150 2150
Wire Wire Line
	10350 2350 10250 2350
Wire Wire Line
	10250 2350 10250 1900
Connection ~ 10250 1900
Text GLabel 9950 2250 0    50   Input ~ 0
TGS2600
Wire Wire Line
	9950 2250 10350 2250
Text GLabel 3500 4600 0    50   Input ~ 0
TGS2600
Wire Wire Line
	3500 4600 3650 4600
$Comp
L P300_Arduino_IF-rescue:Conn_01x04-Connector_Generic J12
U 1 1 5B7C5A57
P 10550 3350
F 0 "J12" H 10629 3342 50  0000 L CNN
F 1 "I2C" H 10629 3251 50  0000 L CNN
F 2 "Connector_Molex:Molex_KK-254_AE-6410-04A_1x04_P2.54mm_Vertical" H 10550 3350 50  0001 C CNN
F 3 "~" H 10550 3350 50  0001 C CNN
	1    10550 3350
	1    0    0    -1  
$EndComp
Wire Wire Line
	10350 3550 10250 3550
Text GLabel 10050 5750 0    50   Input ~ 0
L2
Text GLabel 10050 5650 0    50   Input ~ 0
L1
Text GLabel 6350 5000 2    50   Input ~ 0
L1
Text GLabel 6350 5100 2    50   Input ~ 0
L2
Text GLabel 3500 2600 0    50   Input ~ 0
SDA
Text GLabel 3500 2700 0    50   Input ~ 0
SCL
Wire Wire Line
	3500 2600 3650 2600
Wire Wire Line
	3500 2700 3650 2700
Text GLabel 9950 3350 0    50   Input ~ 0
SDA
Text GLabel 9950 3450 0    50   Input ~ 0
SCL
Text GLabel 3450 2200 0    50   Input ~ 0
CO2_TX
Text GLabel 3450 2300 0    50   Input ~ 0
CO2_RX
Wire Wire Line
	3450 2200 3650 2200
Wire Wire Line
	3450 2300 3650 2300
$Comp
L P300_Arduino_IF-rescue:Conn_01x04-Connector_Generic J13
U 1 1 5B7CD69A
P 10550 2750
F 0 "J13" H 10629 2742 50  0000 L CNN
F 1 "CO2" H 10629 2651 50  0000 L CNN
F 2 "Connector_Molex:Molex_KK-254_AE-6410-04A_1x04_P2.54mm_Vertical" H 10550 2750 50  0001 C CNN
F 3 "~" H 10550 2750 50  0001 C CNN
	1    10550 2750
	1    0    0    -1  
$EndComp
Text GLabel 9950 2850 0    50   Input ~ 0
CO2_TX
Text GLabel 9950 2750 0    50   Input ~ 0
CO2_RX
Wire Wire Line
	10400 5350 10150 5350
Wire Wire Line
	10150 5350 10150 6000
Wire Wire Line
	10050 5550 10400 5550
Wire Wire Line
	10300 5850 10300 5150
Wire Wire Line
	10300 5850 10400 5850
Wire Wire Line
	6350 4800 6250 4800
Wire Wire Line
	6250 5200 6350 5200
Wire Wire Line
	6250 5400 6350 5400
Wire Wire Line
	6250 4900 6350 4900
Wire Wire Line
	10050 5450 10400 5450
Wire Wire Line
	10050 5650 10400 5650
Wire Wire Line
	10050 5750 10400 5750
$Comp
L P300_Arduino_IF-rescue:Conn_01x02-Connector_Generic J14
U 1 1 5B813B8D
P 2950 5950
F 0 "J14" H 2870 5625 50  0000 C CNN
F 1 "5V" H 2870 5716 50  0000 C CNN
F 2 "Connector_Molex:Molex_KK-254_AE-6410-02A_1x02_P2.54mm_Vertical" H 2950 5950 50  0001 C CNN
F 3 "~" H 2950 5950 50  0001 C CNN
	1    2950 5950
	-1   0    0    1   
$EndComp
Wire Wire Line
	3150 5800 3150 5850
$Comp
L P300_Arduino_IF-rescue:Jumper-Device JP1
U 1 1 5B82731D
P 3500 6400
F 0 "JP1" V 3454 6526 50  0000 L CNN
F 1 "12V_en" V 3545 6526 50  0000 L CNN
F 2 "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical" H 3500 6400 50  0001 C CNN
F 3 "~" H 3500 6400 50  0001 C CNN
	1    3500 6400
	0    1    1    0   
$EndComp
$Comp
L P300_Arduino_IF-rescue:GND-power #PWR0118
U 1 1 5B8273AD
P 3250 7000
F 0 "#PWR0118" H 3250 6750 50  0001 C CNN
F 1 "GND" H 3255 6827 50  0000 C CNN
F 2 "" H 3250 7000 50  0001 C CNN
F 3 "" H 3250 7000 50  0001 C CNN
	1    3250 7000
	1    0    0    -1  
$EndComp
Wire Wire Line
	3150 5950 3150 6050
Wire Wire Line
	3250 6900 3250 7000
Wire Wire Line
	3250 6800 3500 6800
Wire Wire Line
	3500 6800 3500 6700
Connection ~ 3250 6800
Wire Wire Line
	3250 6500 3250 6800
Wire Wire Line
	6250 5100 6350 5100
Wire Wire Line
	6250 5000 6350 5000
Wire Wire Line
	6250 4100 6350 4100
Wire Wire Line
	6250 3600 6400 3600
Wire Wire Line
	6250 3700 6400 3700
Wire Wire Line
	1800 3300 1800 3400
Wire Wire Line
	1800 3400 1950 3400
$Comp
L P300_Arduino_IF-rescue:Conn_01x08-Connector_Generic J15
U 1 1 5C116096
P 1600 1700
F 0 "J15" H 1550 1200 50  0000 L CNN
F 1 "TFT Display 1" V 1700 1400 50  0000 L CNN
F 2 "Connectors_Molex:Molex_KK-6410-08_08x2.54mm_Straight" H 1600 1700 50  0001 C CNN
F 3 "~" H 1600 1700 50  0001 C CNN
	1    1600 1700
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:Conn_01x08-Connector_Generic J16
U 1 1 5C13E74F
P 1950 1700
F 0 "J16" H 1900 1200 50  0000 L CNN
F 1 "TFT Display 2" V 2050 1400 50  0000 L CNN
F 2 "Connectors_Molex:Molex_KK-6410-08_08x2.54mm_Straight" H 1950 1700 50  0001 C CNN
F 3 "~" H 1950 1700 50  0001 C CNN
	1    1950 1700
	-1   0    0    -1  
$EndComp
Text GLabel 1400 1400 0    50   Input ~ 0
LCD_RST
Text GLabel 1400 1500 0    50   Input ~ 0
LCD_CS
Text GLabel 1400 1600 0    50   Input ~ 0
LCD_RS
Text GLabel 1400 1700 0    50   Input ~ 0
LCD_WR
Text GLabel 1400 1800 0    50   Input ~ 0
LCD_RD
$Comp
L P300_Arduino_IF-rescue:GND-power #PWR0119
U 1 1 5C1452A8
P 1050 2250
F 0 "#PWR0119" H 1050 2000 50  0001 C CNN
F 1 "GND" H 1055 2077 50  0000 C CNN
F 2 "" H 1050 2250 50  0001 C CNN
F 3 "" H 1050 2250 50  0001 C CNN
	1    1050 2250
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:+5V-power #PWR0120
U 1 1 5C158BBD
P 950 1350
F 0 "#PWR0120" H 950 1200 50  0001 C CNN
F 1 "+5V" H 965 1523 50  0000 C CNN
F 2 "" H 950 1350 50  0001 C CNN
F 3 "" H 950 1350 50  0001 C CNN
	1    950  1350
	1    0    0    -1  
$EndComp
Wire Wire Line
	950  2000 950  1350
$Comp
L P300_Arduino_IF-rescue:+3.3V-power #PWR0121
U 1 1 5C16CD35
P 750 1350
F 0 "#PWR0121" H 750 1200 50  0001 C CNN
F 1 "+3.3V" H 765 1523 50  0000 C CNN
F 2 "" H 750 1350 50  0001 C CNN
F 3 "" H 750 1350 50  0001 C CNN
	1    750  1350
	1    0    0    -1  
$EndComp
$Comp
L P300_Arduino_IF-rescue:+3.3V-power #PWR0122
U 1 1 5C1C40C4
P 3350 5700
F 0 "#PWR0122" H 3350 5550 50  0001 C CNN
F 1 "+3.3V" H 3365 5873 50  0000 C CNN
F 2 "" H 3350 5700 50  0001 C CNN
F 3 "" H 3350 5700 50  0001 C CNN
	1    3350 5700
	1    0    0    -1  
$EndComp
Wire Wire Line
	3350 5700 3650 5700
Wire Wire Line
	950  2000 1400 2000
Wire Wire Line
	1400 1900 1050 1900
Wire Wire Line
	750  1300 750  1350
Connection ~ 750  1350
Wire Wire Line
	750  1350 750  2100
Text GLabel 3450 3100 0    50   Input ~ 0
LCD_RD
Text GLabel 3450 3200 0    50   Input ~ 0
LCD_WR
Text GLabel 3450 3300 0    50   Input ~ 0
LCD_RS
Text GLabel 3450 3400 0    50   Input ~ 0
LCD_CS
Text GLabel 3450 3500 0    50   Input ~ 0
LCD_RST
Wire Wire Line
	3450 3100 3650 3100
Wire Wire Line
	3450 3200 3650 3200
Wire Wire Line
	3450 3300 3650 3300
Wire Wire Line
	3450 3400 3650 3400
Wire Wire Line
	3450 3500 3650 3500
Text GLabel 6400 2300 2    50   Input ~ 0
LCD_D7
Text GLabel 6400 2200 2    50   Input ~ 0
LCD_D6
Text GLabel 6400 2100 2    50   Input ~ 0
LCD_D5
Text GLabel 6400 2000 2    50   Input ~ 0
LCD_D4
Text GLabel 6400 1900 2    50   Input ~ 0
LCD_D3
Text GLabel 6400 1800 2    50   Input ~ 0
LCD_D2
Text GLabel 6400 2400 2    50   Input ~ 0
LCD_D0
Text GLabel 6400 2500 2    50   Input ~ 0
LCD_D1
Wire Wire Line
	6250 2200 6400 2200
Wire Wire Line
	6250 2300 6400 2300
Wire Wire Line
	6250 2400 6400 2400
Wire Wire Line
	6250 2500 6400 2500
Text GLabel 2300 1500 2    50   Input ~ 0
LCD_D0
Text GLabel 2300 1400 2    50   Input ~ 0
LCD_D1
Text GLabel 2300 1600 2    50   Input ~ 0
LCD_D7
Text GLabel 2300 1700 2    50   Input ~ 0
LCD_D6
Text GLabel 2300 1800 2    50   Input ~ 0
LCD_D5
Text GLabel 2300 1900 2    50   Input ~ 0
LCD_D4
Text GLabel 2300 2000 2    50   Input ~ 0
LCD_D3
Text GLabel 2300 2100 2    50   Input ~ 0
LCD_D2
Wire Wire Line
	2300 1400 2150 1400
Wire Wire Line
	2300 1500 2150 1500
Wire Wire Line
	2300 1600 2150 1600
Wire Wire Line
	2300 1700 2150 1700
Wire Wire Line
	2300 1800 2150 1800
Wire Wire Line
	2300 1900 2150 1900
Wire Wire Line
	2300 2000 2150 2000
Wire Wire Line
	2300 2100 2150 2100
Text Notes 8000 6200 0    39   ~ 0
Lüfter L1 ist Zuluft\nLüfter L2 ist Abluft\n(wegen Belegungskompatibilität \ndes Steckers mit Pluggit ist:\n L2 auf Stecker Pin1-4 und \n L1 auf Stecker Pin5-8)
$Comp
L power:GND #PWR02
U 1 1 5C33A4E0
P 8750 5700
F 0 "#PWR02" H 8750 5450 50  0001 C CNN
F 1 "GND" H 8755 5527 50  0000 C CNN
F 2 "" H 8750 5700 50  0001 C CNN
F 3 "" H 8750 5700 50  0001 C CNN
	1    8750 5700
	1    0    0    -1  
$EndComp
Wire Wire Line
	8850 5200 8750 5200
Wire Wire Line
	8750 5200 8750 5600
Wire Wire Line
	8850 5600 8750 5600
Connection ~ 8750 5600
Wire Wire Line
	8750 5600 8750 5700
$Comp
L Device:R R11
U 1 1 5C490061
P 8850 4200
F 0 "R11" H 8920 4246 50  0000 L CNN
F 1 "4,7k" H 8920 4155 50  0000 L CNN
F 2 "Resistors_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical" V 8780 4200 50  0001 C CNN
F 3 "~" H 8850 4200 50  0001 C CNN
	1    8850 4200
	1    0    0    -1  
$EndComp
$Comp
L Device:R R12
U 1 1 5C4900E8
P 8550 4200
F 0 "R12" H 8620 4246 50  0000 L CNN
F 1 "4,7k" H 8620 4155 50  0000 L CNN
F 2 "Resistors_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P2.54mm_Vertical" V 8480 4200 50  0001 C CNN
F 3 "~" H 8550 4200 50  0001 C CNN
	1    8550 4200
	1    0    0    -1  
$EndComp
$Comp
L power:+5V #PWR03
U 1 1 5C4903EC
P 8700 3950
F 0 "#PWR03" H 8700 3800 50  0001 C CNN
F 1 "+5V" H 8715 4123 50  0000 C CNN
F 2 "" H 8700 3950 50  0001 C CNN
F 3 "" H 8700 3950 50  0001 C CNN
	1    8700 3950
	1    0    0    -1  
$EndComp
Wire Wire Line
	8550 3950 8550 4050
Connection ~ 8700 3950
$Comp
L power:GND #PWR01
U 1 1 5C4B22F8
P 7950 3950
F 0 "#PWR01" H 7950 3700 50  0001 C CNN
F 1 "GND" H 7955 3777 50  0000 C CNN
F 2 "" H 7950 3950 50  0001 C CNN
F 3 "" H 7950 3950 50  0001 C CNN
	1    7950 3950
	-1   0    0    1   
$EndComp
$Comp
L Device:C_Small C1
U 1 1 5C4DFA42
P 7950 4100
F 0 "C1" H 8150 4000 50  0000 R CNN
F 1 "100nF" H 8200 4200 50  0000 R CNN
F 2 "Capacitors_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm" H 7950 4100 50  0001 C CNN
F 3 "~" H 7950 4100 50  0001 C CNN
	1    7950 4100
	-1   0    0    1   
$EndComp
$Comp
L Device:C_Small C2
U 1 1 5C50216D
P 8100 4100
F 0 "C2" H 8008 4054 50  0000 R CNN
F 1 "100nF" H 8008 4145 50  0000 R CNN
F 2 "Capacitors_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm" H 8100 4100 50  0001 C CNN
F 3 "~" H 8100 4100 50  0001 C CNN
	1    8100 4100
	-1   0    0    1   
$EndComp
Wire Wire Line
	8100 4000 7950 4000
Wire Wire Line
	7950 4000 7950 3950
Connection ~ 7950 4000
Wire Wire Line
	9950 2750 10350 2750
Wire Wire Line
	9950 2850 10350 2850
Wire Wire Line
	10150 2650 10150 2150
Wire Wire Line
	10150 2650 10350 2650
Connection ~ 10150 2150
Wire Wire Line
	10250 2350 10250 2950
Wire Wire Line
	10250 2950 10350 2950
Connection ~ 10250 2350
Connection ~ 10250 2950
Wire Wire Line
	9950 3350 10350 3350
Wire Wire Line
	9950 3450 10350 3450
Wire Wire Line
	10250 2950 10250 3550
Wire Wire Line
	10150 2650 10150 3250
Wire Wire Line
	10150 3250 10350 3250
Connection ~ 10150 2650
Wire Wire Line
	10150 3650 10150 3250
Connection ~ 10150 3250
Wire Notes Line
	11050 1050 11050 4000
Wire Notes Line
	11050 4000 9400 4000
Wire Notes Line
	9400 4000 9400 1050
Wire Notes Line
	9400 1050 11050 1050
Text Notes 9600 1000 0    50   ~ 0
Sensoren und I2C\nFeuchte, VOC, CO2 und I2C
Wire Wire Line
	7550 1550 7700 1550
Connection ~ 7700 1550
Wire Wire Line
	7550 1950 7700 1950
Connection ~ 7700 1950
Wire Wire Line
	7700 1950 8550 1950
Wire Wire Line
	7550 2300 7700 2300
Connection ~ 7700 2300
Wire Wire Line
	7550 2650 7700 2650
Connection ~ 7700 2650
Wire Notes Line
	9250 1050 9250 3150
Text Notes 7550 1000 0    50   ~ 0
Temperatursensoren\nDS18B20
Wire Wire Line
	8850 4350 8850 4400
Connection ~ 8850 4650
Wire Wire Line
	8700 3950 8850 3950
Wire Wire Line
	8850 4050 8850 3950
Wire Wire Line
	7600 4900 7800 4900
Wire Wire Line
	7450 5200 7600 5200
Wire Wire Line
	7500 4650 8850 4650
Wire Wire Line
	8550 3950 8700 3950
Wire Wire Line
	7500 5300 8550 5300
Connection ~ 8550 5300
Wire Wire Line
	8550 5300 8850 5300
Wire Wire Line
	8550 4350 8550 4500
Wire Wire Line
	8100 4400 8850 4400
Wire Wire Line
	8100 4200 8100 4400
Connection ~ 8850 4400
Wire Wire Line
	8850 4400 8850 4650
Wire Wire Line
	7950 4500 8550 4500
Wire Wire Line
	7950 4200 7950 4500
Connection ~ 8550 4500
Wire Wire Line
	8550 4500 8550 5300
Wire Notes Line
	6950 3650 9300 3650
Wire Notes Line
	9300 3650 9300 6350
Wire Notes Line
	9300 6350 6950 6350
Wire Notes Line
	6950 6350 6950 3650
Wire Wire Line
	7450 5500 7500 5500
Wire Wire Line
	7500 5500 7500 5300
Text Notes 7600 3600 0    50   ~ 0
Lüfteranschluss\nTacho- und PWM Signal
Wire Notes Line
	9400 4900 11050 4900
Wire Notes Line
	11050 4900 11050 6350
Wire Notes Line
	11050 6350 9400 6350
Wire Notes Line
	9400 6350 9400 4900
Text Notes 9700 4850 0    50   ~ 0
Relaisanschluss\nLüfter und Bypass
Wire Notes Line
	6950 1050 6950 3150
Wire Notes Line
	6950 3150 9250 3150
Wire Notes Line
	6950 1050 9250 1050
Wire Wire Line
	750  2100 1400 2100
Wire Wire Line
	1050 1900 1050 2250
Wire Notes Line
	600  1050 2750 1050
Wire Notes Line
	2750 1050 2750 2550
Wire Notes Line
	2750 2550 600  2550
Wire Notes Line
	600  2550 600  1050
Text Notes 900  1000 0    50   ~ 0
Anschluss Display\n480x320\nmcufriend.com
Wire Notes Line
	600  2950 2400 2950
Wire Notes Line
	2400 2950 2400 4400
Wire Notes Line
	2400 4400 600  4400
Wire Notes Line
	600  4400 600  2950
Text Notes 700  2900 0    50   ~ 0
Analoge Drucksensoren
Wire Wire Line
	3500 6100 3650 6100
$Comp
L arduino:Arduino_Mega2560_Shield XA1
U 1 1 5B783564
P 4950 3950
F 0 "XA1" H 4950 1570 60  0000 C CNN
F 1 "Arduino_Mega2560_Shield" H 4950 1464 60  0000 C CNN
F 2 "Arduino_Mega2560_Shield" H 5650 6700 60  0001 C CNN
F 3 "https://store.arduino.cc/arduino-mega-2560-rev3" H 5650 6700 60  0001 C CNN
	1    4950 3950
	1    0    0    -1  
$EndComp
Wire Wire Line
	3150 5850 3650 5850
Wire Wire Line
	3650 5850 3650 5800
Connection ~ 3150 5850
Wire Notes Line
	6800 1050 6800 7350
Wire Notes Line
	6800 7350 2800 7350
Wire Notes Line
	2800 7350 2800 1050
Wire Notes Line
	2800 1050 6800 1050
Text Notes 3600 1000 0    50   ~ 0
Arduino Mega 2560 Rev3\nLAN Shield W5100
Wire Wire Line
	6250 1800 6400 1800
Wire Wire Line
	6250 1900 6400 1900
Wire Wire Line
	6250 2000 6400 2000
Wire Wire Line
	6250 2100 6400 2100
$EndSCHEMATC
