/*
* CANBadger Ethernet Message
* Copyright (c) 2016 Henrik Ferdinand Noelscher
* Copyright (c) 2021 Noelscher Consulting GmbH
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
 * Application Layer format that provides a packet structure for
 * sending and receiving data or commands to and from the CANBadger
 */

#ifndef ETHERNET_MESSAGE_HPP_
#define ETHERNET_MESSAGE_HPP_

#include "mbed.h"
#include "string.h"

enum MessageType {
	ACK,
	NACK, // nacks can provide a reason / error message in data field
	DATA, // for raw frame data, can contain multiple frames in data field
	ACTION,
	CONNECT,
	DEBUG_MSG
};


enum ActionType {
	NO_TYPE,
	SETTINGS, // update settings request
	EEPROM_WRITE,
	LOG_RAW_CAN_TRAFFIC,
	ENABLE_TESTMODE,
	STOP_CURRENT_ACTION,
	RESET,
	START_UDS,
	START_TP,
	UDS,
	TP,
	HIJACK,
	MITM,
	UPDATE_SD,
	DOWNLOAD_FILE,
	DELETE_FILE,
	RECEIVE_RULES,
	ADD_RULE,
	ENABLE_MITM_MODE,
	START_REPLAY,
	RELAY,
	LED
};

enum TestType {
	LOG_RAW_CAN_TEST = 1,
	DISABLE_TESTMODE,
	STOP_TEST,
	RESET_DEVICE
};

/*
 * EthernetMessages look like this on the wire:
 * MesgType (1 byte) | ActionType (1 byte) | dataLength (4 byte) | ... data | 0x00 (optional)
 * the types require casting because you can't define the type of an enum
 *  - to circumvent this, ethernetMessage class has two separate fields for them and unserialize will populate them
 *  NOTE: dataLength is transmitted little-endian
 */
struct EthernetMessageHeader {
	uint8_t type;
	uint8_t actionType;
	uint32_t dataLength;
};

class EthernetMessage {
public:
	MessageType type;
	ActionType actionType;
	char *data; // string of arbitrary length
	uint32_t dataLength; // size of paylod. important when sending
	bool heapUse = false; //

	// will parse ethernetMessage to wire format
	static char* serialize(EthernetMessage* msg, char* buf = 0);
	// will parse an ethernet message from a datagram / binary data
	static EthernetMessage* unserialize(char* data);

	~EthernetMessage();
};

/*
 * Following structs are payloads for EthernetMessages when receiving or responding to commands over Ethernet
 */

typedef struct {
	uint8_t		interface;				// the can interface to be used
    uint32_t   	localID;                // local identifier
    uint32_t   	remoteID;          	   	// remote identifier
    CANFormat   format;             	// 0 - STANDARD, 1 - EXTENDED IDENTIFIER, 2 - ANY
    bool 		enablePadding;				// use padding?
    uint8_t 	paddingByte;			// byte used for padding
    uint8_t		addressType;			// 0 - STANDART, 1 - EXTENDED
    uint8_t		targetDiagSession;		// target session level
} UDSSessionArgument;

typedef struct {
	uint16_t	SID;					// SID of response
	bool		success;				// successful response or error
	uint32_t	dataLength;				// size of following payload data
} UDSResponse;

typedef struct {
	uint16_t	SID;
	uint32_t	dataLength;
} UDSRequest;

typedef struct {
	uint32_t	localID;
	uint32_t	remoteID;
	uint16_t	securityAccessLevel;
	uint16_t	diagSessionLevel;
} SecHijackRequest;

typedef struct {
	bool		success;				// was the hijack successful
	uint16_t	sessionLevel;			// level of hijacked session
} SecHijackResponse;

#endif /* ETHERNET_MESSAGE_HPP_ */
