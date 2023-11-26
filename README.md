# bigxionflasher-esp32

This project creates the possibility for users of a BionX systems to read and change certain parameters of their system.

This project is not related to the original BigXionFlasher project, but is a separate project focusing on porting it
to the ESP32 platform.

Components List:
* ESP32
* CAN Bus tranceiver. SN65HVD230 works.


Electrical connections:

ESP32 | SN65HVD230
--- | ---
3.3V | 3.3V
GND | GND
GPIO5 | CTX
GPIO4 | CRX

SN65HVD230 | Cable
--- | ---
CANH | CAN High
CANL | CAN Low
GND | CAN GND

See [this document](https://www.bigxionflasher.org/download/Kabelbelegung.pdf) for the cable pinout.


In order to use the project you can connect to the ESP32 using your phone using the `Serial Bluetooth Terminal`(or similar) app
and follow the instructions in the terminal.

