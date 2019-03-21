# Teleporter-HTTP-ESP32

https://github.com/idzuna/wifiteleporter

Transfer GPIO status between ESP-WROOM-32 boards using HTTP

## Pin assign

| No | Name      | Function |                                           |
-----|-----------|----------|--------------------------------------------
|  2 | 3V3       | -        |                                           |
|  3 | EN        | -        |                                           |
|  4 | SENSOR_VP | ADC0     |                                           |
|  5 | SENSOR_VN | ADC3     |                                           |
|  6 | IO34      | ADC6     |                                           |
|  7 | IO35      | ADC7     |                                           |
|  8 | IO32      | INPUT0   |                                           |
|  9 | IO33      | INPUT1   |                                           |
| 10 | IO25      | OUTPUT0  |                                           |
| 11 | IO26      | OUTPUT1  |                                           |
| 12 | IO27      | OUTINV01 |                                           |
| 13 | IO14      | ROLESEL0 |                                           |
| 14 | IO12      | WIFIMODE | Strapping pin. Do not pull-up externally. |
| -  | GND       | -        |                                           |
| 16 | IO13      | LED0     |                                           |
| 17 | SD2       | -        | Internal flash interface. Do not connect. |
| 18 | SD3       | -        | Internal flash interface. Do not connect. |
| 19 | CMD       | -        | Internal flash interface. Do not connect. |

| No | Name      | Function |                                             |
-----|-----------|----------|----------------------------------------------
| 37 | IO23      | INPUT2   |                                             |
| 36 | IO22      | INPUT3   |                                             |
| 35 | TXD0      | -        |                                             |
| 34 | RXD0      | -        |                                             |
| 33 | IO21      | OUTPUT2  |                                             |
|    | GND       | -        |                                             |
| 31 | IO19      | OUTPUT3  |                                             |
| 30 | IO18      | LED1     |                                             |
| 29 | IO5       | -        | Strapping pin. Do not pull-down externally. |
| 28 | IO17      | ROLESEL1 |                                             |
| 27 | IO16      | OUTINV23 |                                             |
| 26 | IO4       | SSIDSEL  |                                             |
| 25 | IO0       | -        | Strapping pin. Do not pull-down externally. |
| 24 | IO2       | GROUPSEL |                                             |
| 23 | IO15      | -        | Strapping pin. Do not pull-down externally. |
| 22 | SD1       | -        | Internal flash interface. Do not connect.   |
| 21 | SD0       | -        | Internal flash interface. Do not connect.   |
| 20 | CLK       | -        | Internal flash interface. Do not connect.   |


## Signal descriptions:

### OUTPUTn

Digital output ports.
Each port has 20mA drive strength and reflects a INPUTn port of an other device.
Before the first communication, OUTPUTn stays high if the signal inversion is disabled and low if it is enabled.

### INPUTn

Digital input ports.
Each port is pulled-up by 45kƒ¶ internal resistor.
In order not to miss input trigger between polling interval, falling edge interrput is implemented.

                                   * cause falling edge interrupt
    INPUT0  level: ^^^^^^^^^^^^^^^^|____|^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    
    Polling and  :    |          |          |          |          |
    communication     V          V          V          V          V
    
    OUTPUT0 level: ^^^^^^^^^^^^^^^^^^^^^^^^^|__________|^^^^^^^^^^^^^^^^^^

### LEDn

Status LED output ports.
Each port has 20mA drive strength and stays high for 100 ms after a communication to the other device.
If ROLESEL0 = H and ROLESEL1 = L, LEDn represents communications to clientn.
Otherwise, LED0 represents communications to client0 or server and LED1 port is disabled (high impedance).

### ADCn:

12bit analog input ports.
ADC values (0 - 4095) can be gotten from HTTP.

### OUTINV01 / OUTINV23

Output inversion selection port.
OUTINV01 controls whether to invert OUTPUT0-1 and OUTINV02 controls OUTPUT2-3.
The port configuration is loaded only once at startup and modifications after startup have no effects.

| OUTINV01 | Description                |
-----------|-----------------------------
|     z    | Disable output inversion   |
|     L    | Invert OUTPUT0 and OUTPUT1 |
|     H    | Invert OUTPUT0             |

| OUTINV23 | Description                |
-----------|-----------------------------
|     z    | Disable output inversion   |
|     L    | Invert OUTPUT2 and OUTPUT3 |
|     H    | Invert OUTPUT2             |

### ROLESELn

Device role selection ports.
Each port is pulled-up by 45kƒ¶ internal resistor and controls IP address and device role.
The port configuration is loaded only once at startup and modifications after startup have no effects.

| ROLESEL0 | ROLESEL1 |  Role   |         OUTPUT0-1       |           OUTPUT2-3     |  LED0   |  LED1   |
-----------|----------|---------|---------------------------------------------------|---------|----------
|    H     |    H     | server  | INPUT0-1 of the client0 | INPUT2-3 of the client0 | client0 |    z    |
|    L     |    H     | client0 | INPUT0-1 of the server  | INPUT2-3 of the server  | server  |    z    |
|    H     |    L     | server  | INPUT0-1 of the client0 | INPUT0-1 of the client1 | client0 | client1 |
|    L     |    L     | client1 | INPUT0-1 of the server  | INPUT2-3 of the server  | server  |    z    |

### GROUPSEL

Device group selection port.
It controls IP address and only the same group devices can communicate each other.
The port configuration is loaded only once at startup and modifications after startup have no effects.

| GROUPSEL | Group |
-----------|--------
|     z    |   0   |
|     L    |   1   |
|     H    |   2   |

### WIFIMODE

Wi-Fi mode selection port.
The port configuration is loaded only once at startup and modifications after startup have no effects.

| WIFIMODE | Description       |
-----------|--------------------
|     z    | Station mode      |
|     L    | Access point mode |
|     H    | Prohibited        |

### SSIDSEL

SSID selection port.
The port configuration is loaded only once at startup and modifications after startup have no effects.

| SSIDSEL | SSID           |
----------|-----------------
|    z    | BASESSID       |
|    L    | BASESSID + "0" |
|    H    | BASESSID + "1" |

BASESSID is defined in this source code.

## IP address:

The device IP address is determined by GROUPSEL and ROLESELn ports.

| Group |  Role   | IP address         |
--------|---------|---------------------
|   0   | server  | BASEIPADDRESS +  0 |
|   0   | client0 | BASEIPADDRESS +  1 |
|   0   | client1 | BASEIPADDRESS +  2 |
|   1   | server  | BASEIPADDRESS +  4 |
|   1   | client0 | BASEIPADDRESS +  5 |
|   1   | client1 | BASEIPADDRESS +  6 |
|   2   | server  | BASEIPADDRESS +  8 |
|   2   | client0 | BASEIPADDRESS +  9 |
|   2   | client1 | BASEIPADDRESS + 10 |
