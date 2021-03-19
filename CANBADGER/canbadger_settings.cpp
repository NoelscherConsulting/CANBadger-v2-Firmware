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

#include "canbadger_settings.hpp"

/***Conversions***/
Conversions converter;

//  constructor setting some defaults
//  as this is not used directly but rather in restore()
//  we overwrite these defaults if settings are found on the SD
CanbadgerSettings::CanbadgerSettings(CANbadger *cb) {
	this->isConnected = false;
	this->connectedTo = 0;
	this->cb = cb;
	this->canbadgerStatus = 0;
	char *id = new char[19];
	// use eep id for unique default id
	cb->readEEPROMUID((uint8_t*) id);
	sprintf(id, "%X%X%X%X", id[0], id[2], id[4], id[5]);
	this->id = id;


	//setting defaults if no settings were found on SD
	this->CAN1Speed = 500000;
	this->CAN2Speed = 500000;
	this->SPISpeed = 20000000;
	this->setStatus(CAN1_STANDARD, 1);
	this->setStatus(CAN2_STANDARD, 1);
	if(this->cb->isSDInserted) {
		this->setStatus(SD_ENABLED, 1);
	}
}

// save the current settings on the SD
void CanbadgerSettings::persist() {
	// if SD_ENABLED is not set, stop
	if(!(this->getStatus(0))) {
		return;
	}

	// check for settings filename in EEPROM
	uint8_t name[50];
	uint8_t len = checkEEPROMForFilename(cb, name);

	// check for existing file
	size_t fsize;
	if(len > 0) {
		fsize = cb->getFileSize((const char*) name);
		if(fsize != UINT32_MAX) {
			cb->removeFileNoninteractive((const char*) name);
		}
	} else {
		fsize = cb->getFileSize("/canbadger_settings.txt");
		if(fsize != UINT32_MAX) {
			cb->removeFileNoninteractive("/canbadger_settings.txt");
		}
	}

	uint32_t offset = 0;

	const char *statusSettings[30] = { "SD_ENABLED", "USB_SERIAL_ENABLED", "ETHERNET_ENABLED", "OLED_ENABLED", "KEYBOARD_ENABLED",
					"LEDS_ENABLED", "KLINE1_INT_ENABLED",  "KLINE2_INT_ENABLED",  "CAN1_INT_ENABLED",  "CAN2_INT_ENABLED",
					"KLINE_BRIDE_ENABLED", "CAN_BRIDGE_ENABLED", "CAN1_LOGGING", "CAN2_LOGGING",  "KLINE1_LOGGING",
					"KLINE2_LOGGING", "CAN1_STANDARD", "CAN1_EXTENDED", "CAN2_STANDARD", "CAN2_EXTENDED",
					"CAN1_TO_CAN2_BRIDGE", "CAN2_TO_CAN1_BRIDGE", "KLINE1_TO_KLINE2_BRIDGE", "KLINE2_TO_KLINE1_BRIDGE", "UDS_CAN1_ENABLED",
					"UDS_CAN2_ENABLED", "CAN1_USE_FULLFRAME", "CAN2_USE_FULLFRAME", "CAN1_MONITOR", "CAN2_MONITOR"
			};

	// write settings in key: value form into the canbadgers tmpBuffer

	// add the canbadgers id
	cpyToOutBuf("id: ", &offset);
	cpyToOutBuf(this->id, &offset);
	cpyToOutBuf("\n", &offset);

	// add the boolean parameters
	for(int i = 0; i<30; i++) {
		cpyToOutBuf(statusSettings[i], &offset);
		cpyToOutBuf(": ", &offset);
		this->getStatus(i) ? cpyToOutBuf("1", &offset) : cpyToOutBuf("0", &offset);
		cpyToOutBuf("\n", &offset);
	}

	// add the speed parameters
	char valueBuffer[20];
	cpyToOutBuf("SPISpeed: ", &offset);
	sprintf(valueBuffer, "%lu", this->SPISpeed);
	cpyToOutBuf(valueBuffer, &offset);
	cpyToOutBuf("\n", &offset);
	cpyToOutBuf("CAN1Speed: ", &offset);
	sprintf(valueBuffer, "%lu", this->CAN1Speed);
	cpyToOutBuf(valueBuffer, &offset);
	cpyToOutBuf("\n", &offset);
	cpyToOutBuf("CAN2Speed: ", &offset);
	sprintf(valueBuffer, "%lu", this->CAN2Speed);
	cpyToOutBuf(valueBuffer, &offset);
	cpyToOutBuf("\n", &offset);
	cpyToOutBuf("KLINE1Speed: ", &offset);
	sprintf(valueBuffer, "%lu", this->KLINE1Speed);
	cpyToOutBuf(valueBuffer, &offset);
	cpyToOutBuf("\n", &offset);
	cpyToOutBuf("KLINE2Speed: ", &offset);
	sprintf(valueBuffer, "%lu", this->KLINE2Speed);
	cpyToOutBuf(valueBuffer, &offset);

	this->cb->writeTmpBuffer(offset--, 1, (uint8_t*) "\0");
	if(len > 0) {
		cb->writeFile((const char*) name, this->cb->tmpBuffer, 0, offset);
	} else {
		cb->writeFile("/canbadger_settings.txt", this->cb->tmpBuffer, 0, offset);
	}

}


// try to read the settings from the SD (or EEPROM), if thats not possible create new settings object
CanbadgerSettings* CanbadgerSettings::restore(CANbadger *cb)  {

	// check for settings filename in EEPROM
	uint8_t name[50];
	uint8_t len = checkEEPROMForFilename(cb, name);

	char* settings;
	bool success = false;
	uint8_t parse_fails = 0;

	while(true) {

		// check for filename from EEPROM on SD
		size_t fsize = cb->getFileSize((const char*)name);
		if(parse_fails == 0 && fsize > 0 && fsize < UINT32_MAX) {
			settings = new char[fsize];
			size_t read = cb->readFile((const char*) name, (uint8_t*)settings, 0, fsize);
			if(read > 0) {
				success = true;
			}
		}

		// check default settings name
		if(!success && parse_fails < 2){
			fsize = cb->getFileSize("/canbadger_settings.txt");
			if(fsize > 0 && fsize < UINT32_MAX) {
				settings = new char[fsize];
				size_t read = cb->readFile("/canbadger_settings.txt", (uint8_t*)settings, 0, fsize);
				if(read > 0) {
					success = true;
				}
			}
		}

		// check for settings on the EEPROM or use defaults
		if(!success) {
			// no settings have been saved yet! get new settings with defaults
			CanbadgerSettings *default_settings = new CanbadgerSettings(cb);
			// check if settings are stored in EEPROM
			default_settings->loadFromEEPROM();

			return default_settings;
		}

		CanbadgerSettings *cbSettings = new CanbadgerSettings(cb);

		// return settings on successfull parse
		if(cbSettings->parse(settings)) {
			return cbSettings;
		}

		// settings were not successfully parsed
		// try other options (default filename or EEPROM
		parse_fails++;
	}
}


// parse content from file or EEPROM to set the attributes of this settings object
bool CanbadgerSettings::parse(char *compact_settings) {
	const char *statusSettings[30] = { "SD_ENABLED", "USB_SERIAL_ENABLED", "ETHERNET_ENABLED", "OLED_ENABLED", "KEYBOARD_ENABLED",
						"LEDS_ENABLED", "KLINE1_INT_ENABLED",  "KLINE2_INT_ENABLED",  "CAN1_INT_ENABLED",  "CAN2_INT_ENABLED",
						"KLINE_BRIDE_ENABLED", "CAN_BRIDGE_ENABLED", "CAN1_LOGGING", "CAN2_LOGGING",  "KLINE1_LOGGING",
						"KLINE2_LOGGING", "CAN1_STANDARD", "CAN1_EXTENDED", "CAN2_STANDARD", "CAN2_EXTENDED",
						"CAN1_TO_CAN2_BRIDGE", "CAN2_TO_CAN1_BRIDGE", "KLINE1_TO_KLINE2_BRIDGE", "KLINE2_TO_KLINE1_BRIDGE", "UDS_CAN1_ENABLED",
						"UDS_CAN2_ENABLED", "CAN1_USE_FULLFRAME", "CAN2_USE_FULLFRAME", "CAN1_MONITOR", "CAN2_MONITOR"
				};

	//splitting input into lines
	char* lineend;
	char* param;
	bool endParse = false;

	while(!endParse) {
		// find the end of the line
		lineend = strchr(compact_settings, '\n');
		if(lineend == NULL) {
			// this is the last line to parse
			endParse=true;
		} else {
			// make line into a single string
			*lineend++ = '\0';
		}

		// parameter at the beginning of line, rest of the settings starts at next line
		param = compact_settings;
		compact_settings = lineend;

		//get parameter and value from line as single strings
		char *value = strchr(param, ':');
		if(value !=NULL) {
			*value++ = '\0';
			trim(value);
		} else {
			// incorrect parameter syntax in this line
			// this invalidates the loaded settings
			// fall back to other options
			return false;
		}

		//parse parameters and values

		//extract the id or interface speeds
		if(strcmp(param, "id") == 0) {
			strcpy(this->id, value);
		} else if(strcmp(param, "SPISpeed") == 0) {
			this->SPISpeed = (uint32_t)strtoumax(value, NULL, 10);
		} else if(strcmp(param, "CAN1Speed") == 0) {
			this->CAN1Speed = (uint32_t)strtoumax(value, NULL, 10);
		} else if(strcmp(param, "CAN2Speed") == 0) {
			this->CAN2Speed = (uint32_t)strtoumax(value, NULL, 10);
		} else if(strcmp(param, "KLINE1Speed") == 0) {
			this->KLINE1Speed = (uint32_t)strtoumax(value, NULL, 10);
		} else if(strcmp(param, "KLINE2Speed") == 0) {
			this->KLINE2Speed = (uint32_t)strtoumax(value, NULL, 10);
		} else {
			// parse all the status bits
			for(int i = 0; i<30; i++) {
				if(strcmp(param, statusSettings[i]) == 0) {
					this->setStatus((uint8_t)i, (uint8_t)strtoumax(value, NULL, 10));
					break;
				}
			}
		}

	}

	return true;
}

// write the compact settings into EEPROM
bool  CanbadgerSettings::storeInEEPROM() {
	uint8_t compact_settings[50] = {0};
	size_t data_len = this->getSettingsPayload(compact_settings);

	return this->cb->writeEEPROM(0, 50, compact_settings);
}

// try to restore the settings from EEPROM
bool CanbadgerSettings::loadFromEEPROM() {
	uint8_t compact_settings[50] = {0};

	bool available = this->cb->readEEPROM(0, 50, compact_settings);
	// if the EEPROM only returns 0xFF, power was off and no data is available
	if(compact_settings[0] == 0xFF) { available = false; }

	if(!available) { return false; }

	this->receiveSettingsPayload(compact_settings, false);
	return true;
}

char* CanbadgerSettings::getID() {
	return this->id;
}
void CanbadgerSettings::setStatus(uint8_t statusType, uint8_t value)
{
	converter.setBit(&canbadgerStatus,value,statusType);
}

bool CanbadgerSettings::getStatus(uint8_t statusType)
{
	return converter.getBit(canbadgerStatus,statusType);
}

// set one of the interface speed values (! this just applies to the settings value and not to the interface directly !)
bool CanbadgerSettings::setSpeed(uint8_t if_ident, uint32_t speed) {
	switch(if_ident) {
		case 0:
			SPISpeed = speed;
			return 1;
		case 1:
			CAN1Speed = speed;
			return 1;
		case 2:
			CAN2Speed = speed;
			return 1;
		case 3:
			KLINE1Speed = speed;
			return 1;
		case 4:
			KLINE2Speed = speed;
			return 1;
		default:
			return 0;
	}
}

// get the speed value for an interface (! the desired value represented in the settings, not the actual value !)
uint32_t CanbadgerSettings::getSpeed(uint8_t if_ident) {
	switch(if_ident) {
			case 0:
				return SPISpeed;
			case 1:
				return CAN1Speed;
			case 2:
				return CAN2Speed;
			case 3:
				return KLINE1Speed;
			case 4:
				return KLINE2Speed;
			default:
				return -1;
		}
}

// puts the settings in a compact format for transfer to canbadger server (or into EEPROM)
size_t CanbadgerSettings::getSettingsPayload(uint8_t *payload) {
	// get the size of id, create appropriate buffer and fill it with size + id
	size_t id_len = strlen(this->id);
	payload[0] = id_len;
	memcpy(&payload[1], this->id, id_len);

	// append status bytes and speeds
	memcpy(&payload[id_len + 1], &(this->canbadgerStatus), 4);
	memcpy(&payload[id_len + 5], &(this->SPISpeed), 4);
	memcpy(&payload[id_len + 9], &(this->CAN1Speed), 4);
	memcpy(&payload[id_len + 13], &(this->CAN2Speed), 4);
	memcpy(&payload[id_len + 17], &(this->KLINE1Speed), 4);
	memcpy(&payload[id_len + 21], &(this->KLINE2Speed), 4);

	return id_len + 25;
}

// on receiving new settings from the canbadger server, update the local settings and persist them if specified
void CanbadgerSettings::receiveSettingsPayload(uint8_t *payload, bool save) {
	// copy the values from the message payload into the settings object
	size_t id_len = payload[0];
	memcpy(this->id, &payload[1], id_len);
	this->id[id_len] = '\0';
	memcpy(&(this->canbadgerStatus), &payload[id_len + 1], 4);
	memcpy(&(this->SPISpeed), &payload[id_len + 5], 4);
	memcpy(&(this->CAN1Speed), &payload[id_len + 9], 4);
	memcpy(&(this->CAN2Speed), &payload[id_len + 13], 4);
	memcpy(&(this->KLINE1Speed), &payload[id_len + 17], 4);
	memcpy(&(this->KLINE2Speed), &payload[id_len + 21], 4);

	// as we have received new settings, we persist them on the sd
	if(save) { this->persist(); }

	// update the ethernet_manager, so it broadcasts the new id
	if(this->cb->getEthernetManager()){
		this->cb->getEthernetManager()->reacquireIdentifier();
	}
}

// saves connection info on connection request from canbadger server
void CanbadgerSettings::handleConnect(EthernetMessage *msg, char *remoteAddress) {
	// copy the string to a safe location
	int addr_len = strlen(remoteAddress);
	char *connectedTo = new char[addr_len+1];
	strncpy(connectedTo, remoteAddress, addr_len);
	connectedTo[addr_len] = 0x00;
	this->connectedTo = connectedTo;
	this->isConnected = true;
}

// copies the string in data into the canbadgers tmpBuffer that we write to a file later, increases offset by string length
uint32_t CanbadgerSettings::cpyToOutBuf(const char *data, uint32_t *offset) {
	this->cb->writeTmpBuffer(*offset, strlen(data), (uint8_t*) data);
	*offset = *offset + strlen(data);

	return *offset;
}

// check if a filename was stored in EEPROM
// return the length
uint8_t checkEEPROMForFilename(CANbadger *cb, uint8_t *name) {
	// expect name to hold at least 50 chars
	uint8_t length;
	cb->readEEPROM(50, 1, &length);
	if(length == 255) { return 0; }
	if(length > 50) { length = 49; }

	cb->readEEPROM(51, length, name);
	name[length] = 0;
	return length;
}

// trims whitespaces in given c-string
void trim(char *s) {
	const char* d = s;
	do {
		while(isspace(*d)) {
			d++;
		}
	} while((*s++ = *d ++));
}

// helper converting number in a string into integer
uintmax_t strtoumax(const char *nptr, char **endptr, int base)
{
	const char *s;
	uintmax_t acc, cutoff;
	int c;
	int neg, any, cutlim;
	/*
	 * See strtoq for comments as to the logic used.
	 */
	s = nptr;
	do {
		c = (unsigned char) *s++;
	} while (isspace(c));
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else {
		neg = 0;
		if (c == '+')
			c = *s++;
	}
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X')) {
		c = s[1];
		s += 2;
		base = 16;
	}
	if (base == 0)
		base = c == '0' ? 8 : 10;
        /* BIONIC: avoid division and modulo for common cases */
#define  CASE_BASE(x)                            \
            case x: cutoff = UINTMAX_MAX / x;    \
	            cutlim = UINTMAX_MAX % x;    \
		    break
        switch (base) {
        CASE_BASE(8);
	CASE_BASE(10);
	CASE_BASE(16);
	default:
	    cutoff = UINTMAX_MAX / base;
	    cutlim = UINTMAX_MAX % base;
	}
	for (acc = 0, any = 0;; c = (unsigned char) *s++) {
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= base)
			break;
		if (any < 0)
			continue;
		if (acc > cutoff || (acc == cutoff && c > cutlim)) {
			any = -1;
			acc = UINTMAX_MAX;
			errno = ERANGE;
		} else {
			any = 1;
			acc *= (uintmax_t)base;
			acc += c;
		}
	}
	if (neg && any > 0)
		acc = -acc;
	if (endptr != 0)
		*endptr = (char *) (any ? s - 1 : nptr);
	return (acc);
}
