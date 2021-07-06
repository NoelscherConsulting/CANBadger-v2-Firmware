/*
* CANBadger Ethernet Manager
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

#ifndef ETHERNET_MANAGER_HPP_
#define ETHERNET_MANAGER_HPP_

//#define DEBUG
#define START_THREAD 1
#define CB_VERSION 2
#define ETHERNET_START_SIG 1


#include "mbed.h"
#include "EthernetInterface.h"
#include "rtos.h"
#include "ethernet_message.hpp"
#include "canbadger_settings.hpp"
#include "stdlib.h"

class CanbadgerSettings;

class EthernetManager {
public:
	/* initializes ethernet manager
	 * receives uid of device, which will be used for discovery
	 * will setup the ethernet interface and request an ip using dhcp
	 */
	EthernetManager(int deviceUid, CanbadgerSettings *canbadgerSettings,
			Mail<EthernetMessage, 16> *commandQueue);
	// destructor
	~EthernetManager();

	/* used for putting messages into outQueue and sending them from the application
	 * returns an error code if anything went wrong, otherwise ??
	 */
	int sendMessage(MessageType type, char *data, uint32_t dataLength);

	/*
	 * used for easily sending a string message, as in device.printf
	 *
	 */
	int sendFormattedDataMessage(char *msg, ...);
	int sendFormattedDebugMessage(char *msg, ...);

	// send an ACK to currently connected host
	void sendACK();
	void sendNACK();
	/*
	 * for when you really have to decide at which point a packet is sent
	 * contrary to sendMessage, memory management is at the hand of the caller
	 */
	int sendMessageBlocking(MessageType type, ActionType atype, char *data, uint32_t dataLength);
	int sendMessagesBlocking(EthernetMessage **messages, size_t numMessages);
	// store frame in xram and send it - used for canlogger
	int sendRamFrame(MessageType type, char *data, uint32_t dataLength);

	void run();

	// this will block - you have been warned
	int debugLog(char *debugMsg);

	// will clear the ramStart and ramEnd counters
	void resetRam();

	// called if we received an id change
	// will reacquire the deviceIdentifier from the settings
	void reacquireIdentifier();

	// setup sockets and endpoints
	void setup();

	// send the broadcast including identifier and ip
	void broadcast();

	// check connectionSocket for incoming CONNECT command
	void checkForConnectionRequest();

	// check for incoming messages and handle them
	void handleInbox();

	// check for outgoing messages and send them
	void handleOutqueue();

	// close TCP connection (and create new socket for next connection)
	void closeConnection();


private:
	int deviceUid;
	char *deviceIdentifier;
	CanbadgerSettings *canbadgerSettings;
	EthernetInterface eth;
	Ticker broadcastTicker;
	size_t idLength;

	UDPSocket broadcastSocket;
	UDPSocket connectionSocket;
	TCPSocketConnection actionSocket;

	Endpoint broadcastEndpoint;
	Endpoint server;

	Mail<EthernetMessage, 16> outQueue;
	Mail<EthernetMessage, 16> *commandQueue;

	volatile uint32_t ramStart;
	volatile uint32_t ramEnd;
	uint32_t ram_write(uint32_t addr, char *buf, uint32_t len);
	uint32_t ram_read(uint32_t addr, char *buf, uint32_t len);
	char *xramSerializationBuffer;

	char *recvBuffer;
	char *xramSendBuffer;

};



#endif /* ETHERNET_MANAGER_HPP_ */
