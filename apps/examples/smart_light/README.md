# Smart Lighting Control with ARTIK 055S

[![License](https://img.shields.io/github/license/SamsungARTIK/TizenRT.svg)](LICENSE)

This application is to use ARTIK 055S to implement smart dimmable light.  The smart light application can be used with Samsung ARTIK mobile app to turned on, turn off and/or adjusting brightness of the light.  A physical on device button is also provided for a more traditional way of controlling light.  By using Samsun ARTIK mobile app, users can easily onboard the smart light onto ARTIK Cloud.  Over the air firmware update (OTA) capability is included in the smart light application as well. 

## Contents

> * [Prepare Hardware](#prepare-hardware)
> * [Build Application](#build-application)
> * [Program Binary](#program-binary)
> * [Run Smart Light application](#run-smart-light-application)
> * [Generate OTA Image](#generate-ota-image)

## Prepare Hardware

Connect LED to ARTIK 055S like shown below:
![Smart Light Circuit](../../../external/docs/media/SmartLight_Circuit.png)


## Build Application
```bash
$ ./tools/configure.sh artik055s/typical
$ make
```

## Program Binary
```bash
$ make download ALL
```

## Run Smart Light application
```bash
TASH>> smartlight
```

## Generate OTA Image
```bash
$ make image ota
```
Note: The resulting "ota.bin" can be found under **/build/configs/$(BOARD_NAME)/bin**


Enjoy!

