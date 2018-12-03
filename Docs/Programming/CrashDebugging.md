# Debugging Crashes


## DeadlockWatchdog

The project uses DeadlockWatchdog library to detect deadlocks in the code.
These typically happen when stack overflows or dynamic allocation is used
and the heap overflows (e.g., due to fragmentation). But, there may be other
reasons, e.g., unresponsive code reading a sensor or serial communication
port (e.g., W5100 Ethernet driver, if no Ethernet shield present).

DeadlockWatchdog resets the microcontroller if it was not reset in time.
Currently, we use maximum value (8s), since there are tasks, which do take
1-2 seconds. When the timeout hits, the instruction pointer is determined
at which the watchdog interrupt was generated. This is typically in an
endless loop or similar.


## Getting Crash Information

Crash information of recent 4 crashes (as configured by KWLConfig::MaxCrashReportCount)
is stored in EEPROM in KWLPersistentConfiguration and can be queried from
there. Each crash report contains instruction pointer, stack pointer, real
time (if available) and millis() timer at the time of the crash.

Following MQTT topics can be used to evaluate crash information and to provoke
crashes and restarts:

Topic                                | Value | Description
------------------------------------ | ----- | --------------------
`d15/debugset/kwl/crash/getvalues`   | (any) | Request to send crash reports.
`d15/debugset/kwl/crash/resetvalues` | (any) | Request to clear all crash reports.
`d15/debugset/kwl/crash/provoke_IKNOWWHATIMDOING` | `YES` | Provoke a deadlock and crash for testing purposes.
`d15/set/kwl/restart`                | `YES` | Restart the controller immediately.

Following MQTT Topics are used to communicate crash information:

Topic                         | Value          | Description
----------------------------- | -------------- | --------------------
`d15/state/kwl/statusbits`    | 0x########     | Bit 0x00040000 indicates crash presence.
`d15/debugstate/kwl/crash/##` | (crash report) | Describes crash report at slot ##.

Each crash report is formatted as follows: `ip XXXXXX sp XXX ntp ######### ms #########`.
Field IP contains instruction pointer in hexadecimal (byte address, not word
address), field SP contains stack pointer, field NTP contains seconds since
epoch of the crash (provided NTP is connected) and field MS contains value
of millis() at the time of crash.

Additionally, status bits of the system indicate crash report presence.


## Finding Code Location of the Crash

Now, the most interesting thing: how to find the code location.

First, you need to find your generated .elf file. To do that, you can turn on
verbose output during compilation in Arduino IDE preferences. When building, you
will see a line like this:

`"/Users/someone/Library/Arduino15/packages/arduino/tools/avr-gcc/4.9.2-atmel3.5.4-arduino2/bin/avr-objcopy" -O ihex -R .eeprom  "/var/folders/22/62g6566d6nbgvjvx_pp6499h0000gn/T/arduino_build_965557/KWLctl.ino.elf" "/var/folders/22/62g6566d6nbgvjvx_pp6499h0000gn/T/arduino_build_965557/KWLctl.ino.hex"`

The interesting thing is `/var/folders/22/62g6566d6nbgvjvx_pp6499h0000gn/T/arduino_build_965557/KWLctl.ino.elf`,
which is the generated ELF file containing the executable.

Now, you can disassemble the executable using avr-objdump like this:

`/Applications/Arduino.app/Contents/Java/hardware/tools/avr/bin/avr-objdump -d -S <path>/KWLctl.ino.elf >dump.txt`

This will produce `dump.txt`, where you can simply search for the instruction pointer
which caused the crash. Surrounding source code will give you the information about
where the crash happened.
