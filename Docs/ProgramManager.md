# Program Manager

The ventilation system supports programming ventilation mode based on time of day
and weekday. A network connection, MQTT broker and a NTP server is a prerequisite,
so you need to configure at least NetworkIPAddress, NetworkMQTTBroker and
NetworkNTPServer in UserConfig.h.


## Programs and Program Sets

The program manager manages programs, which contain start and end time, ventilation
mode to use for this time and two sets of flags - weekdays and program set flags.
Further, the program manager manages up to 8 program sets, which can be selected
by writing a single value (e.g., a set for regular use and another set for vacation
period).

Weekdays flags indicate, whether a program is considered for the given weekday or
not.

Program set flags indicate whether a program is considered for the given program
set or not.

Program data and current program set index are stored in EEPROM, so they survive
a power outage.


## Program Selection

The program selection runs periodically and simply checks each program in the list.
If a program matches (i.e., the program is enabled and the current time is in the
time range of the program, including weekday check), then this program is considered.
The controller then picks matching program with the **highest index**.

With this, you can store more generic programs at lower indices and fine-tune with
more precise programs at higher indices.


## Program String

A program is defined by a string in the form:

        HH:MM HH:MM F wwwwwww pppppppp

where:
* HH:MM HH:MM are start and stop times,
* F is ventilation mode to set (typically 0-3),
* wwwwwww are flags for weekdays indicating whether to run the program on
  a given weekday (1) or not (0),
* pppppppp are flags for program sets in which this program is enabled (1)
  for this set or not (0).


## MQTT Communication

To communicate with the program manager, use the following MQTT messages:

Message                         | Value            | Description
------------------------------- | ---------------- | ------------------------
`d15/set/kwl/program/##/data`   | program string   | Set program data.
`d15/set/kwl/program/##/enable` | program set mask | Enable or disable a program for program sets.
`d15/set/kwl/program/##/get`    | (ignored)        | Send program info back for one program or all, if ## is `all`.
`d15/set/kwl/program/set`       | program set idx  | Activate given program set (0-7).

When setting a program, weekday flags and enabled flag are optional. If weekdays
are not set, the program will be considered every day. If enabled flag is not
set, the program will be by default enabled.

The controller sends back the following status information:

Message                           | Value            | Description
--------------------------------- | ---------------- | ------------------------
`d15/state/kwl/program/##/data`   | program string   | Program data.
`d15/state/kwl/program/##/enable` | program set mask | Program enable mask for program sets.
`d15/state/kwl/program/index`     | ##               | Currently running program index or -1 if none running.
`d15/state/kwl/program/set`       | program set idx  | Currently used program set (0-7).


