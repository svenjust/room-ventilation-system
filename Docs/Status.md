# System Status Communication

The controller communicates system status via MQTT messages to a MQTT broker.
For this to work, at least the IP address and MQTT broker address have to be
configured in UserConfig.hpp (see KWLConfig::NetworkIPAddress and
KWLConfig::NetworkMQTTBroker). Network gateway address as well as DNS and NTP
server are derived from IP address by replacing the last octet with 1.


## Status MQTT Topics

Following status topics are defined:

Topic                                          | Value (Unit)      | Description
---------------------------------------------- | ----------------- | -------------------------------------------
`d15/state/kwl/heartbeat`                      | `online` / `offline` / HH:MM:SS | Online status of the system (see below).
`d15/state/kwl/statusbits`                     | `0xEEEEIIVV`      | Status bits indicating overall system state (see below).
`d15/state/kwl/fan1/speed`                     | #### (rpm)        | Speed of FAN1 (intake).
`d15/state/kwl/fan2/speed`                     | #### (rpm)        | Speed of FAN2 (exhaust).
`d15/state/kwl/aussenluft/temperatur`          | ###.## (ºC)       | Temperature of outside air.
`d15/state/kwl/zuluft/temperatur`              | ###.## (ºC)       | Temperature of inlet air.
`d15/state/kwl/abluft/temperatur`              | ###.## (ºC)       | Temperature of outlet air.
`d15/state/kwl/fortluft/temperatur`            | ###.## (ºC)       | Temperature of exhaust air.
`d15/state/kwl/effiencyKwl`                    | ## (%)            | Current efficiency.
`d15/state/kwl/lueftungsstufe`                 | typically 0-3     | Current ventilation mode.
`d15/state/kwl/dht1/temperatur`                | ###.## (ºC)       | Temperature reported by additional DHT1 sensor (if any).
`d15/state/kwl/dht2/temperatur`                | ### (%)           | Humidity reported by additional DHT1 sensor (if any).
`d15/state/kwl/dht1/humidity`                  | ###.## (ºC)       | Temperature reported by additional DHT2 sensor (if any).
`d15/state/kwl/dht2/humidity`                  | ### (%)           | Humidity reported by additional DHT2 sensor (if any).
`d15/state/kwl/abluft/co2`                     | #### (ppm)        | CO2 concentration measured by additional CO2 sensor (if any).
`d15/state/kwl/abluft/voc`                     | #### (ppm)        | Air pollutants concentration measured by additional VOC sensor (if any).
`d15/state/kwl/antifreeze`                     | `on` / `off`      | State of antifreeze protection.
`d15/state/kwl/summerbypass/flap`              | `open` / `closed` / `unknown` | State of summer bypass flap.
`d15/state/kwl/summerbypass/mode`              | `auto` / `manual` | Summer bypass mode.
`d15/state/kwl/summerbypass/TempAbluftMin`     | ### (ºC)          | Minimum outlet air temperature to consider for opening bypass flap.
`d15/state/kwl/summerbypass/TempAussenluftMin` | ### (ºC)          | Minimum outside air temperature to consider for opening bypass flap.
`d15/state/kwl/summerbypass/HystereseMinutes`  | ### (min)         | Hysteresis for bypass flap change in automatic mode.
`d15/state/kwl/summerbypass/HysteresisTemp`    | ## (ºC)           | Hysteresis temperature to open/close bypass flap.
`d15/state/kwl/heatingapp/combinedUse`         | `YES` / `NO`      | Indicates whether the ventilation system is used in conjuction with a fireplace.
`d15/state/kwl/program/index`                  | ##                | Currently running program index or -1 if none (see ProgramManager.md).
`d15/state/kwl/program/set`                    | #                 | Current program set (0-7, see ProgramManager.md).
`d15/state/kwl/program/`                       | (program string)  | Returned in response to program query (see ProgramManager.md).

NOTE: MQTT topics will be changed in the future to harmonize the language used
(with legacy topic compatibility).


## Heartbeat

The controller sends a heartbeat message once every 30s (by default, it can be configured
or turned off using KWLConfig::HeartbeatPeriod). This message either contains the word
`online` or time in format HH:MM:SS (if NTP is available and KWLConfig::HeartbeatTimestamp
is set to true).

If the connection to the broker breaks, a will message with value `offline` will
be left at the broker, so attached clients can react to the event.


## Status Bits

Status bits in form of a hexadecimal number `0xEEEEIIVV` indicate the overall status
of the system. Top 4 digits indicate error conditions, mid 2 digits indicate additional
information and lower 2 digits contain additional information value.

Error condition flags are OR-ed together to form an error status (`EEEE`):
    * 0001 - FAN1 (intake) not working
    * 0002 - FAN2 (exhaust) not working
    * 0004 - crash presence bit (see Programming/CrashDebugging.md)
    * 0008 - NTP time not yet synchronized or NTP server not answering
    * 0010 - T1 sensor (outside air) not working
    * 0020 - T2 sensor (inlet air) not working
    * 0040 - T3 sensor (outlet air) not working
    * 0080 - T4 sensor (exhaust air) not working

Info messages contain info type (`II`) and info value (`VV`). Following info types
are currently defined:
    * 00 - no additional information
    * 01 - calibration in progress, value indicates which mode is being calibrated
    * 02 - antifreeze is trying to raise temperature using preheater, value indicates
           preheater strength in %
    * 03 - antifreeze turned off one or both fans, value indicates 0 only intake,
           1 also exhaust, for fireplace mode
    * 04 - summer bypass is opening (value 1) or closing (value 0)


## Sensor Values

Sensor values for FAN1, FAN2 and temperature sensors are self-explanatory.

Sensor values for DHT1, DHT2, CO2 and VOC sensors will be only communicated, if
respective sensors are actually installed.


## Ventilation Mode

Current ventilation mode will be communicated upon change and periodically.


## Summer Bypass

Summer bypass state values are communicated when the state of the bypass changes and
periodically.

Summer bypass configuration is only communicated on-demand.
