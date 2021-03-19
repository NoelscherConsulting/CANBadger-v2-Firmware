/*
* CANBadger Settings
* Copyright (c) 2020 Noelscher Consulting GmbH
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

/*
 * Class representing multiple settings or internal states of the CanBadger
 * like the Badgers ID, interface speeds or bridging
 */

#ifndef CANBADGER_SETTINGS_HPP
#define CANBADGER_SETTINGS_HPP

#include <map>
#include <string>
#include <sstream>
#include <cinttypes>
#include <stdint.h>
#include "canbadger.h"
#include "ethernet_message.hpp"
#include "conversions.h"


using namespace std;
class CANbadger;



class CanbadgerSettings {
public:
	CanbadgerSettings(CANbadger *cb);

	// try to restore a new settings object from info on the SD,
	// will return defaults if no/invalid data is found
	static CanbadgerSettings* restore(CANbadger *cb);

	// used to write settings to the sd
	// to persist settings across reboots
	// settings will be saved in /canbadger_settings.txt
	void persist();

	// parse file or EEPROM content to set attribute parameters
	bool parse(char *compact_settings);

	// set or get the value of a certain bit in the canbadger status bytes
	void setStatus(uint8_t statusType, uint8_t value);
	bool getStatus(uint8_t statusType);

	char* getID();

	// set or get the interface speed for CAN, KLINE or SPI
	// if_ident is 0 for SPI, 1 or 2 for CAN1/CAN2, 3 or 4 for KLINE1/KLINE2
	bool setSpeed(uint8_t if_ident, uint32_t value);
	uint32_t getSpeed(uint8_t if_ident);

	// store or load the settings values in/from EEPROM
	bool storeInEEPROM();
	bool loadFromEEPROM();

	// puts the current settings into the given buffer in a format that can be send to the canbadger server
	// returns bytesize of payload
	size_t getSettingsPayload(uint8_t *payload);
	// sets the settings values from the given buffer, will call persist if save is true
	void receiveSettingsPayload(uint8_t *payload, bool save);

	// save the needed information when a connection to the server is made
	void handleConnect(EthernetMessage *msg, char *remoteAddress);

	bool isConnected;
	char *connectedTo; // receiving server ip
	bool currentActionIsRunning = false;
	CANbadger *cb;

private:
	/** Bit encoded. Determines the status (active functions) of the CANBadger

							-Bit 0:  SD card enabled and detected
							-Bit 1:  USB Serial enabled
							-Bit 2:  Ethernet enabled
							-Bit 3:  OLED enabled
							-Bit 4:  Keyboard enabled
							-Bit 5:  LEDS enabled
							-Bit 6:  KLINE1 interrupt enabled
							-Bit 7:  KLINE2 interrupt enabled
							-Bit 8:  CAN1 interrupt enabled
							-Bit 9:  CAN2 interrupt enabled
							-Bit 10: KLINE bridge mode enabled
							-Bit 11: CAN bridge mode enabled
							-Bit 12: CAN1 Logging enabled
							-Bit 13: CAN2 Logging enabled
							-Bit 14: KLINE1 Logging enabled
							-Bit 15: KLINE2 Logging enabled
							-Bit 16: CAN1 Standard Format
							-Bit 17: CAN1 Extended Format
							-Bit 18: CAN2 Standard Format
							-Bit 19: CAN2 Extended Format
							-Bit 20: CAN1 to CAN2 Bridge
							-Bit 21: CAN2 to CAN1 Bridge
							-Bit 22: KLINE1 to KLINE2 Bridge
							-Bit 23: KLINE2 to KLINE1 Bridge
							-Bit 24: UDS CAN1
							-Bit 25: UDS CAN2
							-Bit 26: CAN1 Fullframe
							-Bit 27: CAN2 Fullframe
							-Bit 28: CAN1 Monitor
							-Bit 29: CAN2 Monitor

					 */
	uint32_t canbadgerStatus;

	uint32_t CAN1Speed;//these contain the current frequency for the interfaces
	uint32_t CAN2Speed;
	uint32_t KLINE1Speed;
	uint32_t KLINE2Speed;
	uint32_t SPISpeed;

	char* id;

	uint32_t cpyToOutBuf(const char *data, uint32_t *offset);

};

void trim(char *s);
uint8_t checkEEPROMForFilename(CANbadger *cb, uint8_t *name);

#endif
