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

	#include "ethernet_manager.hpp"

	// global stuff
	bool sendBroadcast = false;

	void toggleBroadcast() {
		sendBroadcast = true;
	}

	EthernetManager::EthernetManager(int deviceUid, CanbadgerSettings *canbadgerSettings, Mail<EthernetMessage, 16> *commandQueue) {
		this->deviceUid = deviceUid;
		this->canbadgerSettings = canbadgerSettings;
		char *cbName = canbadgerSettings->getID();
		this->deviceIdentifier = new char[30];
		snprintf(this->deviceIdentifier, 7+strlen(cbName), "CB|%s|%d|",
				cbName, CB_VERSION);
		this->idLength = strlen(this->deviceIdentifier);
		this->dataOutDestPort = 0;
		sendBroadcast = false;
		this->broadcastTicker.attach(toggleBroadcast, 2.0);
		this->commandQueue = commandQueue;
		this->ramStart = 0;
		this->ramEnd = 0;
		this->xramSerializationBuffer = new char[sizeof(EthernetMessage) + 255];

		//this->debugLog("Initializing ethernet..");

		//eth.init(); //Use DHCP TODO distinguish between DHCP or static mode! (from SD or EEPROM)
		// set the mac in this hacky way
		int init_succ = eth.init("10.0.0.125", "255.255.255.0", "0.0.0.0");  // use static ip
		eth.connect();

	#ifdef DEBUG
		char* adr = eth.getIPAddress();
		char *debugMsg = new char[43];
		sprintf(debugMsg, "Initialized ethernet with ip: %s", adr);
		//this->debugLog(debugMsg);
	#endif
//this->em_thread.signal_set(ETHERNET_START_SIG);
	}

	EthernetManager::~EthernetManager() {
		//em_thread.terminate();
		this->eth.disconnect();
	}

	/*void EthernetManager::mainStarterThread(const void *args) {
		Thread::signal_wait(ETHERNET_START_SIG);
		EthernetManager *klass = (EthernetManager*) args;
		klass->run();
	}*/

	void EthernetManager::run() {
		if(sendBroadcast) {
			broadcast();
			sendBroadcast = false;
		}

		handleInbox();

		//handleOutqueue();
	}

	void EthernetManager::setup() {

		// initialize buffers
		this->recvBuffer = new char[130];
		this->xramSendBuffer = new char[sizeof(EthernetMessage) + 22];

		// setup sockets
		this->actionSocket.bind(13371);
		this->actionSocket.set_blocking(false, 1);
		this->broadcastSocket.init();
		this->broadcastSocket.set_broadcasting(true);
		this->broadcastEndpoint.set_address("255.255.255.255", 13370);
	}

	void EthernetManager::broadcast() {
		this->broadcastSocket.sendTo(this->broadcastEndpoint, this->deviceIdentifier, this->idLength);
	}

	void EthernetManager::handleInbox() {
		int received;

		// try to receive from action socket
		received = this->actionSocket.receiveFrom(client, recvBuffer, 2048);
		if(received > 5) {
			char *remoteAddress = client.get_address();
			EthernetMessage *msg = EthernetMessage::unserialize(recvBuffer);
			if(this->canbadgerSettings->isConnected &&
					(strcmp(this->canbadgerSettings->connectedTo, remoteAddress) == 0)) {
				switch(msg->type) {
					case CONNECT:
						// send ACK anyway, we are already connected
						this->canbadgerSettings->handleConnect(msg, remoteAddress);
						this->dataOutDestPort = (msg->data[3] << 24) | (msg->data[2] << 16) | (msg->data[1] << 8) | (msg->data[0]);
						this->outputEndpoint.set_address(remoteAddress, this->dataOutDestPort);
						this->sendACK();
						delete msg;
						break;
					case ACTION:
						if(msg->actionType == STOP_CURRENT_ACTION)
							this->canbadgerSettings->currentActionIsRunning = false;

						// append to action queue
						this->commandQueue->put(msg);
					default:
						break;
				}
			} else {
				// the connect message should contain a port number in DATA
				// the server will open that port for us so we can send data
				// so the message looks like this: 4 12345|
				//                           msgtype port delim
				if(msg->type == CONNECT) {
					this->canbadgerSettings->handleConnect(msg, remoteAddress);
					this->dataOutDestPort = (msg->data[3] << 24) | (msg->data[2] << 16) | (msg->data[1] << 8) | (msg->data[0]);
					this->outputEndpoint.set_address(remoteAddress, this->dataOutDestPort);
					this->sendACK();
				}
				delete msg;
			}
		}
	};

	void EthernetManager::handleOutqueue() {
		if(this->canbadgerSettings->isConnected) {
			uint16_t numMsgs = 0;
			osEvent evt = this->outQueue.get(0);
			while(evt.status == osEventMail && (numMsgs < 200)) {
				//if(numMsgs > 200)
				//	cancel = true;
				EthernetMessage *outMsg;
				outMsg = 0;
				outMsg = (EthernetMessage*) evt.value.p;
				if(outMsg != 0) {
					char* msgData = EthernetMessage::serialize(outMsg);
					this->actionSocket.sendTo(this->outputEndpoint, msgData, 6 + outMsg->dataLength);
					if(outMsg->data)
						delete[] outMsg->data;
					this->outQueue.free(outMsg); // clean up
					delete[] msgData;
				}
				numMsgs++;
				evt = this->outQueue.get(15);
			}

			// empty the xram
			unsigned int sentCount = 0;
			/* pre-allocate 21 bytes so we dont have to re-allocate things
			   this can be done, because xram transfers are only used by the canlogger, currently */
			while(this->ramStart != this->ramEnd) {
				if(sentCount > 200)
					break;

				if(this->ramStart > 0x1fff4) {
					this->ramStart = 0;
				}

				char headerBuf[6];
				ram_read(this->ramStart, headerBuf, 6);

				uint32_t dataLength = (headerBuf[5] << 24) | (headerBuf[4] << 16) | (headerBuf[3] << 8) | (headerBuf[2]);
				if(dataLength > 255) {
					continue;
				}

				ram_read(this->ramStart+6, xramSendBuffer+6, dataLength);
				memcpy(xramSendBuffer, headerBuf, 6);
				this->actionSocket.sendTo(this->outputEndpoint, xramSendBuffer, 6+dataLength);
				if(this->ramStart + 6 + dataLength > 0x20000) {
					// wrap around
					this->ramStart = 0;
				} else {
					this->ramStart += 6 + dataLength;
				}
				sentCount++;
			}
		}
	};

	void EthernetManager::sendACK() {
		// blindly sends an ack, acquiring and releasing the actionSocket mutex
		EthernetMessage ack;
		ack.type = ACK;
		ack.actionType = NO_TYPE;
		ack.dataLength = 0;
		char* msgData = EthernetMessage::serialize(&ack);
		int error = this->actionSocket.sendTo(this->outputEndpoint, msgData, 6);
		delete[] msgData;
	}

	void EthernetManager::sendNACK() {
		// blindly sends a nack, acquiring and releasing the actionSocket mutex
		EthernetMessage ack;
		ack.type = NACK;
		ack.actionType = NO_TYPE;
		ack.dataLength = 0;
		char* msgData = EthernetMessage::serialize(&ack);
		int error = this->actionSocket.sendTo(this->outputEndpoint, msgData, 6);
		delete[] msgData;
	}

	int EthernetManager::sendMessage(MessageType type, char *data, uint32_t dataLength) {
		osStatus error;
		EthernetMessage* out_msg = this->outQueue.calloc(15);
		if(out_msg != 0) {
			out_msg->type = type;
			out_msg->data = data;
			out_msg->dataLength = dataLength;
			error = this->outQueue.put(out_msg);
			if(error == osOK)
				return 0;
			else
				return -1;
		} else
			return -1;
	}

	int EthernetManager::sendFormattedDataMessage(char *msg, ...) {
		va_list argptr;
		va_start(argptr, msg);

		int msg_length = snprintf(NULL, 0, msg, argptr);
		char *data = new char[msg_length];
		sprintf(data, msg, argptr);
		va_end(argptr);

		osStatus error;
		EthernetMessage* out_msg = this->outQueue.alloc(15);
		if(out_msg != 0) {
			out_msg->type = DATA;
			out_msg->data = data;
			out_msg->dataLength = msg_length;
			error = this->outQueue.put(out_msg);
			if(error == osOK)
				return 0;
			else
				return -1;
		} else {
			delete[] data;
		}
		return -1;
	}

int EthernetManager::sendFormattedDebugMessage(char *msg, ...) {
	va_list argptr;
	va_start(argptr, msg);

	int msg_length = snprintf(NULL, 0, msg, argptr);
	char *data = new char[msg_length];
	sprintf(data, msg, argptr);
	va_end(argptr);

	osStatus error;
	EthernetMessage* out_msg = this->outQueue.alloc(15);
	if(out_msg != 0) {
		out_msg->type = DEBUG_MSG;
		out_msg->data = data;
		out_msg->dataLength = msg_length;
		error = this->outQueue.put(out_msg);
		if(error == osOK)
			return 0;
		else
			return -1;
	} else {
		delete[] data;
	}
	return -1;
}

int EthernetManager::sendMessageBlocking(MessageType type, ActionType atype, char *data, uint32_t dataLength) {
	if(this->canbadgerSettings->isConnected) {
		//EthernetMessage *msg = new EthernetMessage();
		EthernetMessage msg;
		msg.type = type;
		if(atype == NULL) {
			msg.actionType = NO_TYPE;
		} else {
			msg.actionType = atype;
		}
		msg.dataLength = dataLength;
		msg.data = data;
		//char* msgData = EthernetMessage::serialize(msg);
		char *msgData = EthernetMessage::serialize(&msg, this->xramSerializationBuffer);

		int error = this->actionSocket.sendTo(this->outputEndpoint, msgData, 6 + msg.dataLength);
		//if(msg->data)
		//	delete[] msg->data;
		//delete msg;
		return error;
	} else {
		return -1;
	}
}

int EthernetManager::sendMessagesBlocking(EthernetMessage **messages, size_t numMessages) {
	if(this->canbadgerSettings->isConnected) {
		char *packet = new char[130];
		int error = 0;
		for(unsigned int i = 0; i < numMessages; i++) {
			EthernetMessage *msg = messages[i];
			snprintf(packet, 129, "M|%d|%s", msg->type, msg->data);
			error = this->actionSocket.sendTo(this->outputEndpoint, packet, strlen(packet));
			if(error != 0)
				return -1;
		}
		return 0;
	}
	return -1;
}

int EthernetManager::sendRamFrame(MessageType type, char *data, uint32_t dataLength) {
	// serialize here, send later
	// use already allocated memory, as this is only used by canlogger right now
	EthernetMessage ethMsg;
	ethMsg.type = type;
	ethMsg.dataLength = dataLength;
	ethMsg.data = data;
	char *msgData = EthernetMessage::serialize(&ethMsg, this->xramSerializationBuffer);
	if(this->ramEnd + 6 + dataLength > 0x20000) {
		if(this->ramStart > 6 + dataLength) {
			// we have to wrap around
			ram_write(0, msgData, 6 + dataLength);
			this->ramEnd = 6 + dataLength;
			return 6 + dataLength;
		} else {
			// no space anymore, drop
			return -1;
		}
	} else {
		ram_write(this->ramEnd, msgData, 6 + dataLength);
		this->ramEnd += 6 + dataLength;
		return 6 + dataLength;
	}
	return -1;
}

	int EthernetManager::debugLog(char *debugMsg) {
		EthernetMessage * msg = this->outQueue.alloc();
		msg->type = DEBUG_MSG;
		int msg_len = strlen(debugMsg);
		msg->data = new char[msg_len];
		msg->dataLength = msg_len;
		strncpy(msg->data, debugMsg, msg_len-1);
		msg->data[msg_len-1] = 0;
		osStatus status = this->outQueue.put(msg);
		if(status == osOK)
			return 0;
		else
			return -1;
	}

	uint32_t EthernetManager::ram_write (uint32_t addr, char *buf, uint32_t len) {
		// legacy Zeug

		/*uint32_t i;
	    *ramCS1 = 0;
    	this->ram->write(CMD_WRITE);
	    this->ram->write((addr >> 16) & 0xff);
	    this->ram->write((addr >> 8) & 0xff);
	    this->ram->write(addr & 0xff);

	    for (i = 0; i < len; i ++) {
	    	this->ram->write(buf[i]);
	    }
	    *ramCS1 = 1;
	    return i;*/

		// we can make use of the canbadgers respective methods
		return (uint32_t)this->canbadgerSettings->cb->writeRAM(addr, len, (uint8_t*)buf);

	}

	uint32_t EthernetManager::ram_read (uint32_t addr, char *buf, uint32_t len) {
		// legacy Zeug

		/*uint32_t i;

	    *ramCS1 = 0;
	    this->ram->write(CMD_READ);
	    this->ram->write((addr >> 16) & 0xff);
	    this->ram->write((addr >> 8) & 0xff);
	    this->ram->write(addr & 0xff);

	    for (i = 0; i < len; i ++) {
	        buf[i] = this->ram->write(0);
	    }
	    *ramCS1 = 1;
	    return i;*/

		// we can make use of the canbadgers respective methods
		return (uint32_t)this->canbadgerSettings->cb->readRAM(addr, len, (uint8_t*)buf);
	}

void EthernetManager::resetRam() {
	this->ramStart = 0;
	this->ramEnd = 0;
}


// gets called when getting a settings update, we need to get the (possibly new) id so the broadcast is correct
void EthernetManager::reacquireIdentifier() {
	// get new id
	char *cbName = this->canbadgerSettings->getID();

	// update deviceIdentifier and idLength
	snprintf(this->deviceIdentifier, 7+strlen(cbName), "CB|%s|%d|",
			cbName, CB_VERSION);
	this->idLength = strlen(this->deviceIdentifier);
}

