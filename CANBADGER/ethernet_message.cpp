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

#include "ethernet_message.hpp"

EthernetMessage::~EthernetMessage() {
	if(this->dataLength > 0 && this->heapUse) { delete[] this->data; }
}

char* EthernetMessage::serialize(EthernetMessage* msg, char *buf) {
	if(msg->type != ACK && msg->type != NACK) {
		if(msg->dataLength == 0)
			return 0;
	}
	char *ethMsg;
	if(buf == NULL)
		ethMsg = new char[6+msg->dataLength];
	else
		ethMsg = buf;

	ethMsg[0] = (uint8_t) msg->type;
	ethMsg[1] = (uint8_t) msg->actionType;
	ethMsg[5] = (msg->dataLength >> 24) & 0xFF;
	ethMsg[4] = (msg->dataLength >> 16) & 0xFF;
	ethMsg[3] = (msg->dataLength >> 8) & 0xFF;
	ethMsg[2] = msg->dataLength & 0xFF;
	memcpy(ethMsg+6, msg->data, msg->dataLength);

	return ethMsg;
}

EthernetMessage* EthernetMessage::unserialize(char *data) {
	EthernetMessage *msg = new EthernetMessage;
	EthernetMessageHeader header;
	header.type = (uint8_t) data[0];
	header.actionType = (uint8_t) data[1];
	header.dataLength = (data[5] << 24) | (data[4] << 16) | (data[3] << 8) | (data[2]);

	// enforce data length boundaries
	if(header.dataLength < 0 || header.dataLength > 2048)
		header.dataLength = 0;

	// set types
	msg->type = (MessageType) header.type;
	msg->actionType = (ActionType) header.actionType;
	msg->dataLength = header.dataLength;

	// copy data
	if(header.dataLength > 0) {
		msg->data = new char[header.dataLength+1];
		memcpy(msg->data, data+6, header.dataLength);
		msg->data[header.dataLength] = 0;
	} else {
		msg->data = NULL;
	}

	// set heap use to true, the data array needs to be cleaned up on destruction
	msg->heapUse = true;

	return msg;
}


