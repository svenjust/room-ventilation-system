# System Installation

TODO document general installation


## Setting up MQTT Communication Prefix

In order to set up MQTT communication, you need to configure at least NetworkIPAddress
and NetworkMQTTBroker in UserConfig.h (see KWLConfig.h for details).

By default, MQTT topics use prefix `d15` to address the system. You can change this
in several ways:
* Initially by setting PrefixMQTT in UserConfig.h to your prefix.
* At runtime by sending a value to topic `d15/set/kwl/install/prefix`.

The setting is stored in EEPROM, so it will be available after a restart. Currently,
the prefix is limited to 7 characters.

The documentation always refers to prefix `d15`, you need to replace it with your
user-defined prefix.

Setting the prefix allows to run multiple ventilation systems in one network, but
more importantly, to run one real system and one for experimentation, but still
connected to the same network and same MQTT broker.

To differentiate clients, the client name is composed as `kwlClient:<prefix>`.
I.e., the default config would use `kwlClient:d15` as client name in MQTT broker.
