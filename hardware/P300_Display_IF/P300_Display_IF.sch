EESchema Schematic File Version 4
LIBS:P300_Display_IF-cache
EELAYER 26 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title "Interface Platine Display -> Arduino"
Date "2019-01-24"
Rev "V0.26-dev"
Comp "sven@familie-just.de"
Comment1 "https://github.com/svenjust/room-ventilation-system"
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L P300_Display_IF-rescue:GND-power #PWR0101
U 1 1 5B7848DF
P 3800 5000
F 0 "#PWR0101" H 3800 4750 50  0001 C CNN
F 1 "GND" H 3805 4827 50  0000 C CNN
F 2 "" H 3800 5000 50  0001 C CNN
F 3 "" H 3800 5000 50  0001 C CNN
	1    3800 5000
	1    0    0    -1  
$EndComp
Wire Wire Line
	3800 5000 4350 5000
$Comp
L P300_Display_IF-rescue:+5V-power #PWR0103
U 1 1 5B785E95
P 3550 5550
F 0 "#PWR0103" H 3550 5400 50  0001 C CNN
F 1 "+5V" H 3565 5723 50  0000 C CNN
F 2 "" H 3550 5550 50  0001 C CNN
F 3 "" H 3550 5550 50  0001 C CNN
	1    3550 5550
	1    0    0    -1  
$EndComp
Text GLabel 4100 1900 0    50   Input ~ 0
Tacho_L1
Text GLabel 4100 1800 0    50   Input ~ 0
Tacho_L2
Text GLabel 7050 5200 2    50   Input ~ 0
PWM_L2
Text GLabel 7050 5000 2    50   Input ~ 0
PWM_L1
Wire Wire Line
	4100 1800 4350 1800
Wire Wire Line
	4100 1900 4350 1900
Text GLabel 4200 3900 0    50   Input ~ 0
DRUCK2
Text GLabel 4200 3800 0    50   Input ~ 0
DRUCK1
Wire Wire Line
	4200 3800 4350 3800
Wire Wire Line
	4200 3900 4350 3900
Text GLabel 7050 4700 2    50   Input ~ 0
BP_DIR
Text GLabel 7050 3900 2    50   Input ~ 0
TEMP4
Text GLabel 7050 3800 2    50   Input ~ 0
TEMP3
Text GLabel 7050 3700 2    50   Input ~ 0
TEMP2
Text GLabel 7050 3600 2    50   Input ~ 0
TEMP1
Text GLabel 7100 3400 2    50   Input ~ 0
DHT1
Text GLabel 7100 3500 2    50   Input ~ 0
DHT2
Wire Wire Line
	6950 3700 7050 3700
Wire Wire Line
	7050 3600 6950 3600
Wire Wire Line
	6950 3800 7050 3800
Text GLabel 7050 4600 2    50   Input ~ 0
BP_POW
Text GLabel 4200 4400 0    50   Input ~ 0
TGS2600
Wire Wire Line
	4200 4400 4350 4400
Text GLabel 7050 4800 2    50   Input ~ 0
L1
Text GLabel 7050 4900 2    50   Input ~ 0
L2
Text Notes 5350 1100 0    50   ~ 0
Used by LAN-IF
Text GLabel 4200 2400 0    50   Input ~ 0
SDA
Text GLabel 4200 2500 0    50   Input ~ 0
SCL
Wire Wire Line
	4200 2400 4350 2400
Wire Wire Line
	4200 2500 4350 2500
Text GLabel 4150 2000 0    50   Input ~ 0
CO2_TX
Text GLabel 4150 2100 0    50   Input ~ 0
CO2_RX
Wire Wire Line
	4150 2000 4350 2000
Wire Wire Line
	4150 2100 4350 2100
Text Notes 7100 5900 1    50   ~ 0
Used by \nLAN-IF
Wire Wire Line
	7050 4600 6950 4600
Wire Wire Line
	6950 5000 7050 5000
Wire Wire Line
	6950 5200 7050 5200
Wire Wire Line
	6950 4700 7050 4700
Wire Wire Line
	3550 5550 3550 5600
Wire Wire Line
	3550 5600 4350 5600
Wire Wire Line
	6950 4900 7050 4900
Wire Wire Line
	6950 4800 7050 4800
Wire Wire Line
	6950 3900 7050 3900
Wire Wire Line
	6950 3400 7100 3400
Wire Wire Line
	6950 3500 7100 3500
$Comp
L P300_Display_IF-rescue:Conn_01x08-Connector_Generic J15
U 1 1 5C116096
P 1800 2350
F 0 "J15" H 1750 1850 50  0000 L CNN
F 1 "TFT Display 1" V 1900 2050 50  0000 L CNN
F 2 "Connector_Molex:Molex_KK-6410-08_08x2.54mm_Straight" H 1800 2350 50  0001 C CNN
F 3 "~" H 1800 2350 50  0001 C CNN
	1    1800 2350
	1    0    0    -1  
$EndComp
$Comp
L P300_Display_IF-rescue:Conn_01x08-Connector_Generic J16
U 1 1 5C13E74F
P 2300 2350
F 0 "J16" H 2250 1850 50  0000 L CNN
F 1 "TFT Display 2" V 2400 2050 50  0000 L CNN
F 2 "Connector_Molex:Molex_KK-254_AE-6410-08A_1x08_P2.54mm_Vertical" H 2300 2350 50  0001 C CNN
F 3 "~" H 2300 2350 50  0001 C CNN
	1    2300 2350
	-1   0    0    -1  
$EndComp
Text GLabel 1600 2050 0    50   Input ~ 0
LCD_RST
Text GLabel 1600 2150 0    50   Input ~ 0
LCD_CS
Text GLabel 1600 2250 0    50   Input ~ 0
LCD_RS
Text GLabel 1600 2350 0    50   Input ~ 0
LCD_WR
Text GLabel 1600 2450 0    50   Input ~ 0
LCD_RD
$Comp
L P300_Display_IF-rescue:GND-power #PWR0119
U 1 1 5C1452A8
P 1250 3050
F 0 "#PWR0119" H 1250 2800 50  0001 C CNN
F 1 "GND" H 1255 2877 50  0000 C CNN
F 2 "" H 1250 3050 50  0001 C CNN
F 3 "" H 1250 3050 50  0001 C CNN
	1    1250 3050
	1    0    0    -1  
$EndComp
$Comp
L P300_Display_IF-rescue:+5V-power #PWR0120
U 1 1 5C158BBD
P 1150 2000
F 0 "#PWR0120" H 1150 1850 50  0001 C CNN
F 1 "+5V" H 1165 2173 50  0000 C CNN
F 2 "" H 1150 2000 50  0001 C CNN
F 3 "" H 1150 2000 50  0001 C CNN
	1    1150 2000
	1    0    0    -1  
$EndComp
Wire Wire Line
	1150 2650 1150 2000
$Comp
L P300_Display_IF-rescue:+3.3V-power #PWR0121
U 1 1 5C16CD35
P 950 2000
F 0 "#PWR0121" H 950 1850 50  0001 C CNN
F 1 "+3.3V" H 965 2173 50  0000 C CNN
F 2 "" H 950 2000 50  0001 C CNN
F 3 "" H 950 2000 50  0001 C CNN
	1    950  2000
	1    0    0    -1  
$EndComp
Wire Wire Line
	1350 2750 1600 2750
Wire Wire Line
	950  2900 1350 2900
Wire Wire Line
	1350 2900 1350 2750
$Comp
L P300_Display_IF-rescue:+3.3V-power #PWR0122
U 1 1 5C1C40C4
P 4050 5500
F 0 "#PWR0122" H 4050 5350 50  0001 C CNN
F 1 "+3.3V" H 4065 5673 50  0000 C CNN
F 2 "" H 4050 5500 50  0001 C CNN
F 3 "" H 4050 5500 50  0001 C CNN
	1    4050 5500
	1    0    0    -1  
$EndComp
Wire Wire Line
	4050 5500 4350 5500
Wire Wire Line
	1150 2650 1600 2650
Wire Wire Line
	1600 2550 1250 2550
Wire Wire Line
	1250 2550 1250 3050
Wire Wire Line
	950  1950 950  2000
Connection ~ 950  2000
Wire Wire Line
	950  2000 950  2900
Text GLabel 4150 2900 0    50   Input ~ 0
LCD_RD
Text GLabel 4150 3000 0    50   Input ~ 0
LCD_WR
Text GLabel 4150 3100 0    50   Input ~ 0
LCD_RS
Text GLabel 4150 3200 0    50   Input ~ 0
LCD_CS
Text GLabel 4150 3300 0    50   Input ~ 0
LCD_RST
Wire Wire Line
	4150 2900 4350 2900
Wire Wire Line
	4150 3000 4350 3000
Wire Wire Line
	4150 3100 4350 3100
Wire Wire Line
	4150 3200 4350 3200
Wire Wire Line
	4150 3300 4350 3300
Text GLabel 7100 2100 2    50   Input ~ 0
LCD_D7
Text GLabel 7100 2000 2    50   Input ~ 0
LCD_D6
Text GLabel 7100 1900 2    50   Input ~ 0
LCD_D5
Text GLabel 7100 1800 2    50   Input ~ 0
LCD_D4
Text GLabel 7100 1700 2    50   Input ~ 0
LCD_D3
Text GLabel 7100 1600 2    50   Input ~ 0
LCD_D2
Text GLabel 7100 2200 2    50   Input ~ 0
LCD_D0
Text GLabel 7100 2300 2    50   Input ~ 0
LCD_D1
Text GLabel 2650 2150 2    50   Input ~ 0
LCD_D0
Text GLabel 2650 2050 2    50   Input ~ 0
LCD_D1
Text GLabel 2650 2250 2    50   Input ~ 0
LCD_D7
Text GLabel 2650 2350 2    50   Input ~ 0
LCD_D6
Text GLabel 2650 2450 2    50   Input ~ 0
LCD_D5
Text GLabel 2650 2550 2    50   Input ~ 0
LCD_D4
Text GLabel 2650 2650 2    50   Input ~ 0
LCD_D3
Text GLabel 2650 2750 2    50   Input ~ 0
LCD_D2
Wire Wire Line
	2650 2050 2500 2050
Wire Wire Line
	2650 2150 2500 2150
Wire Wire Line
	2650 2250 2500 2250
Wire Wire Line
	2650 2350 2500 2350
Wire Wire Line
	2650 2450 2500 2450
Wire Wire Line
	2650 2550 2500 2550
Wire Wire Line
	2650 2650 2500 2650
Wire Wire Line
	2650 2750 2500 2750
$Comp
L arduino:Arduino_Mega2560_Shield XA1
U 1 1 5B783564
P 5650 3750
F 0 "XA1" H 5650 1370 60  0000 C CNN
F 1 "Arduino_Mega2560_Shield_Display_MCU_Friends" H 5650 1264 60  0000 C CNN
F 2 "Arduino_Mega2560_Shield_Display_MCU_Friends" H 6350 6500 60  0001 C CNN
F 3 "https://store.arduino.cc/arduino-mega-2560-rev3" H 6350 6500 60  0001 C CNN
	1    5650 3750
	1    0    0    -1  
$EndComp
Wire Wire Line
	6950 1600 7100 1600
Wire Wire Line
	6950 1700 7100 1700
Wire Wire Line
	6950 1800 7100 1800
Wire Wire Line
	6950 1900 7100 1900
Wire Wire Line
	6950 2000 7100 2000
Wire Wire Line
	6950 2100 7100 2100
Wire Wire Line
	6950 2200 7100 2200
Wire Wire Line
	6950 2300 7100 2300
$EndSCHEMATC
