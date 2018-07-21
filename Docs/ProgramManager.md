Program Manager
===============

The ventilation system supports programming ventilation mode based on time of day
and weekday. A network connection, MQTT broker and a NTP server is a prerequisite,
so you need to configure at least NetworkIPAddress, NetworkMQTTBroker and
NetworkNTPServer in UserConfig.h.


# Program String

A program is defined by a string in the form:

        HH:MM HH:MM M xxxxxxx F

where:
* HH:MM HH:MM are start and stop times,
* M is ventilation mode to set (typically 0-3),
* xxxxxxx are flags for weekdays indicating whether to run the program on
  a given weekday (1) or not (0),
* F is flag 0 or 1 whether the program is disabled or enabled.


# MQTT Communication

To communicate with the program manager, use the following MQTT messages:

Message                         | Value          | Description
------------------------------- | -------------- | ------------------------
`d15/set/kwl/program/##/data`   | program string | Set program data.
`d15/set/kwl/program/##/enable` | `yes` / `no`   | Enable or disable a program.
`d15/set/kwl/program/##/get`    | (ignored)      | Send program info back for one program or all, if ## is `all`.

When setting a program, weekday flags and enabled flag are optional. If weekdays
are not set, the program will be considered every day. If enabled flag is not
set, the program will be by default enabled.

The controller sends back the following status information:

Message                           | Value          | Description
--------------------------------- | -------------- | ------------------------
`d15/state/kwl/program/##/data`   | program string | Program data.
`d15/state/kwl/program/##/enable` | `yes` / `no`   | Program enable flag.
`d15/state/kwl/program/index`     | ##             | Currently running program index or -1 if none running.


# Program Selection

The program selection runs every minute and simply checks each program in the list.
If a program matches (i.e., the current time is in the time range of the program
including weekday check and the program is enabled), then this program is considered.
The controller then picks matching program with the **highest index**.

With this, you can store more generic programs at lower indices and fine-tune with
more precise programs at higher indices.

Since the program manager also allows enabling/disabling programs, you may create
for instance vacation program(s) at highest indices and keep these disabled. When
you go for vacation, simply enable this program and when you return back, disable
it.

