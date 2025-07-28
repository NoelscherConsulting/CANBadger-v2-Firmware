# CANBadger v2 Firmware
The CANBadger is a vehicle security analysis platform.
In this repository, you can find its firmware. You can find the hardware [here](https://github.com/NoelscherConsulting/CANBadger-v2-Hardware).

![](https://raw.githubusercontent.com/wiki/NoelscherConsulting/CANBadger-v2-Firmware/img/cb_pic1.jpg)

## Current Features
Here's a non-exhaustive list of what you can do with the CANBadger.
Your mileage may vary, as we can not test all features with all kinds of setups. 
**We take no responsibility for the things you do with this project**.
Your feedback is always welcome.

* Two modes of operation: Standalone Mode (runs without computer) and Ethernet Mode. To learn more about the modes, check out [this page](https://github.com/NoelscherConsulting/CANBadger-v2-Firmware/wiki/Modes).
* Two CAN Interfaces, supporting logging, bridging and configurable MITM
* Diagnostic Scanning: Use the CANBadger to **scan for UDS or TP2.0 session**
* **[Security Hijack](https://github.com/NoelscherConsulting/CANBadger-v2-Firmware/wiki/Security-Hijack)**: Take over sessions initiated by third party testers using the CANBadger. Then, do whatever you like.
* K-Line support: Use the CANBadger to interface with ECUs over K-Line (and, with some modifications, LIN).
* K-Line KKL support: Use a broad range of third party tools to perform vehicle diagnostics over K-Line, using the CANBadger KKL feature.
* Network support: Connect multiple CANBadgers over **ethernet**, control them using a computer using the [CANBadger Server](https://github.com/NoelscherConsulting/CANBadger-v2-Server)
* SocketCan: Either use the USB port to turn the CANBadger into a SocketCan interface or use the *Ethernet to SocketCan* bridge to do it over a network.

## I have a CANBadger v2 and would like to flash new firmware:
Please check out the Instructions in the [CANBadger's Wiki](https://github.com/NoelscherConsulting/CANBadger-v2-Firmware/wiki)

The [Wiki](https://github.com/NoelscherConsulting/CANBadger-v2-Firmware/wiki) contains some guides on how to set up your CANBadger, as well as some of the functions it supports.

If you're looking for the CANBadger Server, it's located [here](https://github.com/NoelscherConsulting/CANBadger-v2-Server).


Currently, the CANBadger Automotive Security Platform is maintained by [Noelscher Consulting GmbH](https://noelscher.com).
