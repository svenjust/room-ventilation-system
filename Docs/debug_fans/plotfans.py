#!/usr/bin/python
# -*- coding: latin-1 -*-

################################################################
#
#   Copyright notice
#
#   Control software for a Room Ventilation System
#   https://github.com/svenjust/room-ventilation-system
#
#   Copyright (C) 2019  Sven Just (sven@familie-just.de)
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
import matplotlib
import csv
import argparse
import os
####################################################################
# WAS MACHT DIESES SCRIPT?
# Dieses Script gehört zum Projekt Room Ventilation System,
# https://github.com/svenjust/room-ventilation-system
####################################################################
# Dieses Python Script liest eine Logdatei mit den Drehzahlen der 
# Lüfter ein und stellt die Werte graphisch dar.
# Die Werte werden per mqtt vom Controller an einen mqtt Broker 
# übertragen.
#
# Um die Logdatei zu erzeugen, muss die Software für den Controller 
# mit dem "#DEBUG" kompiliert werden.
#
# Die Übertragung kann für jeden Lüfter einzeln aktiviert werden.
#   mosquitto_pub -t d15/debugset/kwl/fan1/getvalues -m on
#   mosquitto_pub -t d15/debugset/kwl/fan2/getvalues -m on
#
# Die übertragenen Werte können mit der folgenden Zeile in einer 
# Datei "/tmp/debug.log" protokolliert werden:
#   mosquitto_sub -v -h localhost -t "d15/debugstate/#" > /tmp/debug.log
#
# Die Werte aus der Datei "/tmp/debug.log" können durch dieses 
# Script dargestellt werden.
#
# AUFRUF: python <Pfad zu Script>\plotfans.py --infile /tmp/debug.log --out /tmp
#
######################### matplotlib INSTALLIEREN ##################
# pip install matplotlib
#   oder
# apt-get install python-matplotlib
####################################################################

def PlotFan(Fannumber, DoAction):
	
	x1 = []
	y0 = []
	y1 = []
	y2 = []
	y3 = []
		
	with open(inFanLogfile,'r') as csvfile:
		plots = csv.reader(csvfile, delimiter=' ')
		for row in plots:
			if row[1] == 'Fan' + str(Fannumber):
				x1.append(int(row[4].replace(',','')))
				y0.append(int(row[6].replace(',','')))
				y1.append(int(row[8].replace(',','')))
				y2.append(int(row[10].replace(',','')))
				y3.append(int(row[12].replace(',','')))


	fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(20.0, 10.0))   # figsize in inches

	# make a little extra space between the subplots
	fig.subplots_adjust(hspace=0.5)

	fig.suptitle('REPORT FAN ' + str(Fannumber))

	ax1.plot(x1, y1, label='tfs: PWM-Signal to fan', color='steelblue')
	ax1.plot(x1, y2, label='ssf: Setpoint speed fan', color='darkseagreen')
	ax1.plot(x1, y3, label='rpm: Revolutions per minute', color='darkorange')
	ax1.set_xlabel('Timestamp')
	ax1.set_ylabel('Value')
	ax1.grid(True)
	ax1.legend(loc = 'upper right')

	ax2.plot(x1, y0, label='GAP = rpm - ssf', color='indigo')
	ax2.set_ylabel('Gap between rpm and ssf')
	ax2.set_xlabel('Timestamp')
	ax2.grid(True)
	ax2.legend(loc = 'upper right')

	
	print ("Write plot file to: " + os.path.join(outdir,"Plot_Fan" + str(Fannumber) + '.png'))
	
	plt.savefig(os.path.join(outdir,"Plot_Fan" + str(Fannumber) + '.png'),dpi=150)
	
	if DoAction=='show':
		plt.show()

################################################## MAIN ##################################################
		
inFanLogfile='debug.log'
outdir='.'
fan=''
	
# Define and parse command line arguments
parser = argparse.ArgumentParser(description="plotfans.py plots the values from the fans. ")
parser.add_argument("--infile", help="File to read the fan values (default: '" + inFanLogfile + "')")
parser.add_argument("--out", help="Directory to write the plot file(s) (default: './')")
parser.add_argument("--fan",  type=int, help="1 or 2, Number of fan to plot or nothing to plot both fans (default: 'nothing')")
parser.add_argument("--show",  dest='DoAction', action='store_const',  const='show', default='print', help='Show the plot on screen (default: plot to file)')

# If the infansfile is specified on the command line then override the default
args = parser.parse_args()
if args.out:
	outdir = args.out

if args.infile:
	inFanLogfile = args.infile

if args.DoAction == "print":
	matplotlib.use('Agg')
import matplotlib.pyplot as plt

if (args.fan==1):
	PlotFan(1,args.DoAction)
elif (args.fan==2):
	PlotFan(2,args.DoAction)
else:
	PlotFan(1,args.DoAction)
	PlotFan(2,args.DoAction)
	