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
#include "canbadger_settings_constants.h"

/***Conversions***/
Conversions converter;

//  constructor setting some defaults
//  as this is not used directly but rather in restore()
//  we overwrite these defaults if settings are found on the SD
CanbadgerSettings::CanbadgerSettings(CANbadger *cb) {
	this->isConnected = false;
	this->cb = cb;
	this->canbadgerStatus = 0;
	this->useDHCP = true;
	// use eep id for unique default id
	cb->readEEPROMUID((uint8_t*) id);
	sprintf(id, "%X%X%X%X", id[0], id[2], id[4], id[5]);
	this->ip[0] = 'D';
	this->ip[1] = 'H';
	this->ip[2] = 'C';
	this->ip[3] = 'P';
	this->ip[4] = '\0';


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
		if(fsize != UINT32_MAX and fsize != 0) {
			cb->removeFileNoninteractive((const char*) name);
		} else {
			len = 0;
		}
	}
	if (len == 0) {
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
	cpyToOutBuf("\n", &offset);

	// add the IP or "DHCP"
	cpyToOutBuf("IP: ", &offset);
	if (this->useDHCP) {
		cpyToOutBuf("DHCP", &offset);
	} else {
		cpyToOutBuf(this->ip, &offset);
	}

	this->cb->writeTmpBuffer(offset, 1, (uint8_t*) "\0");
	if(len > 0) {
		cb->writeFile((const char*) name, this->cb->tmpBuffer, 0, offset);
	} else {
		cb->writeFile("/canbadger_settings.txt", this->cb->tmpBuffer, 0, offset);
	}

}

// try to read the settings from the SD (or EEPROM), if thats not possible create new settings object
CanbadgerSettings* CanbadgerSettings::restore(CANbadger *cb)  {
	// check if we need to reset everything, first
	// for this, we look for a file named 'RESET_SETTINGS' on the SD card
	// if we find it, we reset everything (by clearing the whole eep) and delete the file
	if(cb->doesFileExist("RESET_SETTINGS.txt")) {
		cb->clearEEPROM();
		cb->removeFileNoninteractive("RESET_SETTINGS.txt");
		// blink red to confirm
		cb->blinkLED(3, LED_RED);
	}

	// check for settings filename in EEPROM
	uint8_t name[50];
	uint8_t len = checkEEPROMForFilename(cb, name);

	char settings[READ_BUFF_SIZE];

	CanbadgerSettings *cbSettings = new CanbadgerSettings(cb);

	// check for filename from EEPROM on SD
	size_t fsize = cb->getFileSize((const char*)name);
	if(fsize > 0 && fsize < UINT32_MAX) {
		size_t read = cb->readFile((const char*) name, (uint8_t*)settings, 0, fsize);
		if(read > 0 && read < READ_BUFF_SIZE) {
			settings[read] = 0;
			// return settings on successful parse
			if(cbSettings->parse((char*)settings, fsize)) {
				return cbSettings;
			}
		}
	}

	// check default settings name, if no filename was provided in EEPROM
	fsize = cb->getFileSize("/canbadger_settings.txt");
	if(fsize > 0 && fsize < UINT32_MAX) {
		size_t read = cb->readFile("/canbadger_settings.txt", (uint8_t*)settings, 0, fsize);
		if(read > 0 && read < READ_BUFF_SIZE) {
			settings[read] = 0;
			// return settings on successful parse
			if(cbSettings->parse((char*)settings, fsize)) {
				return cbSettings;
			}
		}
	}
	// delete settings if previous parses failed to restore default values
	delete cbSettings;


	// check for settings on the EEPROM or use defaults, if no settings were retrieved from the SD
	CanbadgerSettings *default_settings = new CanbadgerSettings(cb);
	// check if settings are stored in EEPROM
	default_settings->loadFromEEPROM();

	// try to persist the new default settings on the SD
	default_settings->persist();

	return default_settings;

}

// parse content from file or EEPROM to set the attributes of this settings object
bool CanbadgerSettings::parse(char *unparsed_settings, size_t len) {
	const char *statusSettings[30] = { "SD_ENABLED", "USB_SERIAL_ENABLED", "ETHERNET_ENABLED", "OLED_ENABLED", "KEYBOARD_ENABLED",
						"LEDS_ENABLED", "KLINE1_INT_ENABLED",  "KLINE2_INT_ENABLED",  "CAN1_INT_ENABLED",  "CAN2_INT_ENABLED",
						"KLINE_BRIDE_ENABLED", "CAN_BRIDGE_ENABLED", "CAN1_LOGGING", "CAN2_LOGGING",  "KLINE1_LOGGING",
						"KLINE2_LOGGING", "CAN1_STANDARD", "CAN1_EXTENDED", "CAN2_STANDARD", "CAN2_EXTENDED",
						"CAN1_TO_CAN2_BRIDGE", "CAN2_TO_CAN1_BRIDGE", "KLINE1_TO_KLINE2_BRIDGE", "KLINE2_TO_KLINE1_BRIDGE", "UDS_CAN1_ENABLED",
						"UDS_CAN2_ENABLED", "CAN1_USE_FULLFRAME", "CAN2_USE_FULLFRAME", "CAN1_MONITOR", "CAN2_MONITOR"
				};

	char *settings_str = unparsed_settings;

	char* lineend;
	char* key;
	bool endParse = false;

	// parse line by line, for each line extract key and value (separated by :)
	while(!endParse && unparsed_settings < (settings_str + len)) {
		// find the end of the line
		lineend = strchr(unparsed_settings, '\n');
		if(lineend == NULL) {
			// this is the last line to parse
			endParse=true;
		} else {
			// make line into a single string
			*lineend++ = '\0';
		}

		// key at the beginning of line, rest of the settings starts at next line
		key = unparsed_settings;
		unparsed_settings = lineend;

		//get key and value from line as single strings
		char *value = strchr(key, ':');
		if(value !=NULL) {
			*value++ = '\0';
			trim(value);
		} else {
			// incorrect syntax in this line
			// try to parse the following lines
			continue;
		}

		// clean up key
		trim(key);
		if(strncmp(key, "\r", 1) == 0) {
			key++;
		}

		//parse found key-value-pair

		//extract the id, ip or interface speeds

		if(strcmp(key, "id") == 0) {
			strcpy(this->id, value);
		} else if (strncmp(key, "IP", 2) == 0) {
            size_t ip_len = strlen(value);
			if(endParse) {
			    // if end of file is reached, we need to calculate the IP string length this way:
                ip_len = (settings_str + len) - value - 1;
			}
            strncpy(this->ip, value, ip_len);
			this->ip[ip_len] = 0x00;
			// if the IP string is set to DHCP instantly set it to use DHCP
			if (strncmp(this->ip, "DHCP", 4) == 0) {
				this->useDHCP = true;
			} else {
				this->useDHCP = false;
			}
		} else if(strcmp(key, "SPISpeed") == 0) {
			this->SPISpeed = (uint32_t)strtoumax(value, NULL, 10);
		} else if(strcmp(key, "CAN1Speed") == 0) {
			this->CAN1Speed = (uint32_t)strtoumax(value, NULL, 10);
		} else if(strcmp(key, "CAN2Speed") == 0) {
			this->CAN2Speed = (uint32_t)strtoumax(value, NULL, 10);
		} else if(strcmp(key, "KLINE1Speed") == 0) {
			this->KLINE1Speed = (uint32_t)strtoumax(value, NULL, 10);
		} else if(strcmp(key, "KLINE2Speed") == 0) {
			this->KLINE2Speed = (uint32_t)strtoumax(value, NULL, 10);
		} else {
			// parse all the status bits
			for(int i = 0; i<30; i++) {
				if(strcmp(key, statusSettings[i]) == 0) {
					this->setStatus((uint8_t)i, (uint8_t)strtoumax(value, NULL, 10));
					break;
				}
			}
		}

	}

	return true;
}

// TODO
// write the compact settings into EEPROM
bool  CanbadgerSettings::storeInEEPROM() {
	uint8_t compact_settings[CBS_COMP_SETT_BUFF_SIZE] = {0};
	size_t data_len = this->getSettingsPayload(compact_settings);

	return this->cb->writeEEPROM(0, CBS_COMP_SETT_BUFF_SIZE, compact_settings);
}

// try to restore the settings from EEPROM
bool CanbadgerSettings::loadFromEEPROM() {
	uint8_t compact_settings[CBS_COMP_SETT_BUFF_SIZE] = {0};

	bool available = this->cb->readEEPROM(0, CBS_COMP_SETT_BUFF_SIZE, compact_settings);
	// if the EEPROM only returns 0xFF, power was off and no data is available
	if(compact_settings[0] == 0xFF) { available = false; }

	if(!available) { return false; }

	this->receiveSettingsPayload(compact_settings, false);
	return true;
}

char* CanbadgerSettings::getID() {
	return this->id;
}

char* CanbadgerSettings::getIP() {
	return this->ip;
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

// TODO
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
// returns length of the resulting payload
size_t CanbadgerSettings::getSettingsPayload(uint8_t *payload) {
	// get the size of id, create appropriate buffer and fill it with size + id
	size_t id_len = strlen(this->id);
	payload[0] = id_len;
	memcpy(&payload[1], this->id, id_len);

	size_t ip_len = strlen(this->ip);
	payload[id_len + 1] = ip_len;
	memcpy(&payload[id_len + 2], this->ip, ip_len);

	// append status bytes and speeds
	size_t integers_start = id_len + ip_len + 2;
	memcpy(&payload[integers_start], &(this->canbadgerStatus), 4);
	memcpy(&payload[integers_start + 4], &(this->SPISpeed), 4);
	memcpy(&payload[integers_start + 8], &(this->CAN1Speed), 4);
	memcpy(&payload[integers_start + 12], &(this->CAN2Speed), 4);
	memcpy(&payload[integers_start + 16], &(this->KLINE1Speed), 4);
	memcpy(&payload[integers_start + 20], &(this->KLINE2Speed), 4);

	return id_len + ip_len + 26;
}


// on receiving new settings from the canbadger server, update the local settings and persist them if specified
void CanbadgerSettings::receiveSettingsPayload(uint8_t *payload, bool save) {
	// copy the values from the message payload into the settings object
	size_t id_len = payload[0];
	if(id_len > 0) {
		memcpy(this->id, &payload[1], id_len);
		this->id[id_len] = '\0';
	}
	size_t ip_len = payload[id_len + 1];
	if(ip_len > 0) {
		memcpy(this->ip, &payload[id_len + 2], ip_len);
		this->ip[ip_len] = '\0';
	}

	size_t integers_start = id_len + ip_len + 2;
	memcpy(&(this->canbadgerStatus), &payload[integers_start], 4);
	memcpy(&(this->SPISpeed), &payload[integers_start + 4], 4);
	memcpy(&(this->CAN1Speed), &payload[integers_start + 8], 4);
	memcpy(&(this->CAN2Speed), &payload[integers_start + 12], 4);
	memcpy(&(this->KLINE1Speed), &payload[integers_start + 16], 4);
	memcpy(&(this->KLINE2Speed), &payload[integers_start + 20], 4);

	// as we have received new settings, we persist them on the sd
	if(save) { this->persist(); }

	// update the ethernet_manager, so it broadcasts the new id
	if(this->cb->getEthernetManager()){
		this->cb->getEthernetManager()->reacquireIdentifier();
	}
}


// saves connection info on connection request from canbadger server
void CanbadgerSettings::handleConnect(char *remoteAddress) {
	// copy the string to a safe location
	int addr_len = strlen(remoteAddress);
	strncpy(connectedTo, remoteAddress, addr_len);
	connectedTo[addr_len] = 0x00;
	this->isConnected = true;
}

// set status to not connected
void CanbadgerSettings::setDisconnected() {
	this->isConnected = false;
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
	if(! cb->readEEPROM(CBS_COMP_SETT_BUFF_SIZE, 1, &length) ) {
		return 0;
	}
	if(length == 255) { return 0; }
	if(length > 49) { length = 49; }
	cb->readEEPROM(CBS_COMP_SETT_BUFF_SIZE + 1, length, name);
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
