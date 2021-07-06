/*
* Command Handler CANBadger library
* Copyright (c) 2019 Noelscher Consulting GmbH
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
 * Contains functions to handle incoming commands over Ethernet
 * that function like a basic API
 */

#include "command_handler.hpp"
#include "canbadger_settings_constants.h"

bool handleEthernetMessage(EthernetMessage *msg, CANbadger *canbadger)
{
	EthernetManager *ethMan = canbadger->getEthernetManager();
	CanbadgerSettings *settings = canbadger->getCanbadgerSettings();
	settings->currentActionIsRunning = true;
	switch(msg->actionType)
	{
		case LOG_RAW_CAN_TRAFFIC:
			if(msg->dataLength > 0) {
				return canLogging(canbadger, msg->data[0]);
			} else
				return canLogging(canbadger, false);
			break;
		case SETTINGS:
			if(msg->dataLength == 0) {
				// if the message holds no data, its a request to send the current settings
				uint8_t data[CBS_COMP_SETT_BUFF_SIZE] = {0};
				size_t data_len = settings->getSettingsPayload(data);
				ethMan->sendMessageBlocking(DATA, SETTINGS, (char*)data, data_len);
			} else {
				// if the message holds data, then we update our settings accordingly
				// check if the payload has the correct length, and id is not too long
				if(msg->dataLength <= CBS_MAX_ID_LEN + CBS_MAX_IP_LEN + 26 && msg->data[0] <= 18 ) {
					settings->receiveSettingsPayload((uint8_t*)msg->data, true);
				} else {
					ethMan->sendNACK();
				}
			}
			break;
		case EEPROM_WRITE:
		{

			uint8_t length = msg->dataLength;
			if(length > 0) {
				// write received filename into EEPROM
				writeFilenameToEEPROM(canbadger, length, msg->data);
			} else {
				// write current settings into EEPROM
				settings->storeInEEPROM();
			}


		}
			break;
		case RESET:
			// reset the canbadger, e.g. to start with new settings
			NVIC_SystemReset();
			break;
		case START_UDS:
			{
				//check if message data contains a UDSSessionArgument
				uint32_t supposedLength = 14;
				if(msg->dataLength != supposedLength) // sizeof(UDSSessionArgumen) is 20 due to alignment, we only use/transfer 14 Bytes though
				{
					//TODO send NACK
					ethMan->sendNACK();
					return false;
				}

				if(canbadger->getUDSHandler() != NULL) { //check for existing session and end it
					canbadger->getUDSHandler()->endSession();
					canbadger->setUDSHandler(NULL);
				}

				UDSSessionArgument args;
				args.interface = msg->data[0];
				args.localID = parse32(msg->data, 1, "LE");
				args.remoteID = parse32(msg->data, 5, "LE");
				args.format =  (CANFormat) msg->data[9];
				args.enablePadding = msg->data[10];
				args.paddingByte = msg->data[11];
				args.addressType = msg->data[12];
				args.targetDiagSession = msg->data[13];

				return startUDSSession(canbadger, &args);
				break;
			}
		case UDS:
			{
				//check if we have a UDSHandler active
				UDSCANHandler *uds_handler = canbadger->getUDSHandler();
				if(uds_handler == NULL || !(uds_handler->sessionStatus()))
				{
					// we have no handler or are not in a session
					ethMan->sendNACK();
					return false;
				}

				//parse UDSRequest from message
				UDSRequest req;
				req.SID = msg->data[0] + (msg->data[1] << 8);
				req.dataLength = parse32(msg->data, 2, "LE");


				uint32_t supposedLength = req.dataLength + 6; // header + following data is all in msg->data
				if(msg->dataLength != supposedLength)
				{
					ethMan->sendNACK();
					return false;
				}

				uint8_t request_data[req.dataLength];
				memcpy(request_data, msg->data + 6, req.dataLength);


				//send request and send response back to server
				return handleUDSRequest(canbadger, &req, request_data);

				break;
			}
		case START_TP:
			//TODO
			break;
		case HIJACK:
			//DEBUG
			{
				uint32_t supposedLength = 12; //2 IDs (a 4Byte) + 2 lvls (a 2Byte)
				if(msg->dataLength != supposedLength)
				{
					ethMan->sendNACK();
					return false;
				}
				SecHijackRequest hj_req;
				hj_req.localID = parse32(msg->data, 0, "LE");
				hj_req.remoteID = parse32(msg->data, 4, "LE");
				hj_req.securityAccessLevel = msg->data[8] + (msg->data[9] << 8);
				hj_req.diagSessionLevel = msg->data[10] + (msg->data[11] << 8);

				return UDSSecurityHijack(canbadger, &hj_req);

				break;
			}
		// SD
		case UPDATE_SD:
			// the server is requesting the contents of the SD in the CanBadger
			if(msg->dataLength == 0)
			{
				size_t outputBufferSize = 200;
				char contents[outputBufferSize];
				char dir[outputBufferSize];
				strcpy(dir, "/");
				return sendSDContents(canbadger, dir, contents, outputBufferSize);
			} else {  // if the UPDATE_SD action holds content we receive a data upload
				FileHandler* sd = canbadger->getFileHandler();

				// if we have no open file this is the transmission start
				if(!sd->isFileOpen(1)) {
					if(!sd->openFile((const char*) msg->data, O_WRONLY | O_CREAT, 1)) {
						ethMan->sendNACK();
						return false;
					}
					// expecting packet number 0 next
					canbadger->generalCounter4 = 0;
					ethMan->sendACK();
					return true;
				}

				// if a file is already opened, we write the message content to it

				// first 5 Bytes are packet_number and it's content size
				uint32_t packet_number = parse32(msg->data, 0, "LE");
				uint8_t content_size = (uint8_t)msg->data[4];

				// compare packet_number received and expected one (general counter keeps track)
				if (canbadger->generalCounter4 == packet_number) {
					// expected packet is the next one
					canbadger->generalCounter4 += 1;
				} else {
					// stop the transmission
					ethMan->sendNACK();
					sd->closeFile(1);
					return false;
				}

				// received valid file content, just write it to file
				bool write_succ = sd->write(msg->data + 5, content_size, 1);

				// abort on write failure, else send ACK to receive next data
				write_succ ? ethMan->sendACK() : ethMan ->sendNACK();
				if (!write_succ) {
					sd->closeFile(1);
				}
				return write_succ;

			}
		case DOWNLOAD_FILE:
			{
				/*
				 * output format is 4 byte package number (BigEndian), 2 byte length of following data (BigEndian), data
				 */

				size_t packet_size = 200;
				char outputBuffer[packet_size + 6];
				size_t read = 0;
				uint32_t packetCount = 0;
				FileHandler* sd = canbadger->getFileHandler();

				// check if the file is on the SD
				//expect nullterminated filepath in message
				if(!sd->doesFileExist((const char*)msg->data)) {
					ethMan->sendNACK();
					return false;
				}
				if(!sd->openFile((const char*)msg->data, O_RDONLY, 1)) {
					ethMan->sendNACK();
					return false;
				}

				// send the file in packets with a leading 4 byte packet number and 2 byte data length
				while(true) {
					read = sd->read(outputBuffer+6, packet_size, 1);

					// add packet number
					outputBuffer[0] = (packetCount >> 24) & 0xFF;
					outputBuffer[1] = (packetCount >> 16) & 0xFF;
					outputBuffer[2] = (packetCount >> 8) & 0xFF;
					outputBuffer[3] = packetCount & 0xFF;

					// add read bytes as length
					outputBuffer[4] = (read >> 8) & 0xFF;
					outputBuffer[5] = read & 0xFF;

					ethMan->sendMessageBlocking(DATA, NO_TYPE, outputBuffer, read+6);

					if(read < packet_size) { break; }
				}

				// close file and send ACK to confirm end of transmission
				sd->closeFile(1);
				ethMan->sendACK();
				return true;
			}
			break;
		case DELETE_FILE:
			{
				FileHandler* sd = canbadger->getFileHandler();
				// check if the given filepath even exists
				if(!sd->doesFileExist((const char*)msg->data)) {
					ethMan->sendNACK();
					return false;
				}
				sd->removeFileNoninteractive((const char*)msg->data);
				ethMan->sendACK();
				return true;
			}

			break;
		// MITM
		case RECEIVE_RULES:
			// get a permantent mitm object readz to receive rules directly to XRam
			canbadger->getNewMITM();
			ethMan->sendACK();
			break;
		case ADD_RULE:
			// add rule to persistent mitm
			if(canbadger->addMITMRule(msg->data)) {
				ethMan->sendACK();
			} else {
				ethMan->sendNACK();
			}
			break;
		case MITM:
			// rules should have been loaded into xram with RECEIVE_RULES and ADD_RULE messages
			return canbadger->startMITM();
		case ENABLE_MITM_MODE:
		{
			// get the rulefile name from the message data
			char filename[30] = {0};
			if(msg->dataLength == 0) {
				strcat(filename, "/MITM/rules.txt");
			} else {
				strcat(filename, "/MITM/");
				strcat(filename, (const char*)msg->data);
			}

			//check if the rules file exists
			FileHandler* sd = canbadger->getFileHandler();
			if(!sd->doesFileExist((const char*)filename)) {
				ethMan->sendNACK();
				return false;
			}
			ethMan->sendACK();

			// call the canbadgers MITMMode
			canbadger->MITMMode(filename,  CANStandard);
			return true;

			break;
		}
		// REPLAY
		case START_REPLAY: {
			// send the message out of the given interface
			uint32_t frame_id;
			uint8_t interface;
			uint8_t payload_length;
			uint8_t payload[8];
			memcpy(&interface, msg->data, 1);
			frame_id = parse32(msg->data, 1, "LE");
			payload_length = (msg->dataLength) - 5;
			memcpy(payload, (msg->data) + 5, payload_length);

			CANFormat format = CANStandard;
			if((frame_id & 0x80000000) != 0) {
				// is extended ID
				format = CANExtended;
			}
			bool result = canbadger->sendCANFrame(frame_id, payload, payload_length, interface, format, CANData, 1000);

			if(result) {
				ethMan->sendACK();
			} else {
				ethMan->sendNACK();
			}
			break;
		}
		case STOP_CURRENT_ACTION: {
			// ensure current action running is false when we received stop
			settings->currentActionIsRunning = false;

			// we need to handle this case in this thread as the main thread is probably busy right now
			// clean up canbadger
			if(canbadger->getUDSHandler() != NULL) {
				canbadger->getUDSHandler()->endSession();
				canbadger->setUDSHandler(NULL);
			}

			// close file if one is opened by data transfer
			FileHandler* sd = canbadger -> getFileHandler();
			if(sd->isFileOpen(1)) {
				sd->closeFile(1);
				ethMan->sendACK();
			}
			break;
		}
		case RELAY: {
			// set or toggle the extension pins where relays can be added to the canbadger
			GPIOHandler* relays = canbadger->getGPIOHandler();
			if(msg->dataLength > 2) { break; }
			if(msg->dataLength == 0) {
				// send current gpio state
				uint8_t state = relays->gpioState();
				char answer[2] = {0};
				answer[0] = state % 2;
				answer[1] = (state >> 1) % 2;
				ethMan->sendMessageBlocking(DATA, RELAY, answer, 2);
				break;
			}
			uint8_t relay_no = msg->data[0];
			if (msg->dataLength == 2) {
				if(msg->data[1] > 0) {
					relays->gpioOn(relay_no);
				} else {
					relays->gpioOff(relay_no);
				}
			} else {
				relays->gpioToggle(relay_no);
			}

			break;
		}
		case LED: {
			// set the LED to the color specified in the 1st byte (0 = off, 1 = red, 2 = green, 3 = orange)
			// if 2nd byte was send it is interpreted as a blink command
			if(msg->dataLength < 1 || msg->dataLength > 2) { break; }
			uint8_t color = msg->data[0];
			if(msg->dataLength == 2) {
				uint8_t times = msg->data[1];
			canbadger->blinkLED(times, color);
			} else {
				canbadger->setLED(color);
			}

			break;
		}
		default:
			break;
		}
	return true;
}

bool canLogging(CANbadger *canbadger, bool enableBridgeMode)
{
	Timer *timer = canbadger->getTimer();
	canbadger->setCANBadgerStatus(CAN1_STANDARD, 1);
	canbadger->setCANBadgerStatus(CAN1_EXTENDED, 0);
	if(enableBridgeMode) {
		canbadger->setCANBadgerStatus(CAN1_TO_CAN2_BRIDGE, 1);
		canbadger->setCANBadgerStatus(CAN2_TO_CAN1_BRIDGE, 1);
		canbadger->setCANBadgerStatus(CAN2_STANDARD, 1);
		canbadger->setCANBadgerStatus(CAN2_EXTENDED, 0);
	}
	EthernetManager *ethManager = canbadger->getEthernetManager();
	CanbadgerSettings *cbSettings = canbadger->getCanbadgerSettings();

	bool wasCANBridgeEnabled=false;
	bool wasKLINEBridgeEnabled=false;
	uint32_t frmCount=0;//to keep track of processed frames
	canbadger->generalCounter1=0;//we initialize the buffer pointer for the interrupts and the pending packets
	canbadger->generalCounter2=0;
	canbadger->generalCounter3=0;//used as our buffer Pointer
	uint8_t data[269];//we will store the retrieved data from buffer here. 269 is the maximum length we will ever get

	if(!(canbadger->getCANBadgerStatus(CAN_BRIDGE_ENABLED)) && (canbadger->getCANBadgerStatus(CAN1_LOGGING) || canbadger->getCANBadgerStatus(CAN2_LOGGING)))//enable the bridges so logging happens
	{
		if(!(canbadger->CANBridge(1)))
		{
			return false;//we are looking for a lot of conditions, but if somehow they are not met, then go back
		}
	}
	else if(canbadger->getCANBadgerStatus(CAN_BRIDGE_ENABLED))
	{
		wasCANBridgeEnabled=true;//bridge was already enabled
	}
	if(!(canbadger->getCANBadgerStatus(KLINE_BRIDGE_ENABLED)) && (canbadger->getCANBadgerStatus(KLINE1_LOGGING) || canbadger->getCANBadgerStatus(KLINE2_LOGGING)))
	{
		/*if(!KLINEBridge(1)) //need to create the kline bridge function
		{
			return false;//we are looking for a lot of conditions, but if somehow they are not met, then go back
		}*/
	}
	else if(canbadger->getCANBadgerStatus(KLINE_BRIDGE_ENABLED))
	{
		wasKLINEBridgeEnabled=true;//bridge was already enabled
	}
	timer->stop();
	timer->reset();
	timer->start();
	while(cbSettings->currentActionIsRunning)//log while we have not gotten a stop action
	{
		if(canbadger->generalCounter2 > 0)//if we have pending data to retrieve from RAM
		{
			canbadger->readTmpBuffer(canbadger->generalCounter3, 1, data);
			while(data[0] == 0)//to skip bytes intentionally left blank or if there was a read attempt that was interrupted
			{
				canbadger->generalCounter3++;//we position the counter on the next byte
				if(canbadger->generalCounter3 == 4096)//if we have reached the end of the buffer space
				{
					canbadger->generalCounter3 = 0;//reset it to make it circular!
				}
				canbadger->readTmpBuffer(canbadger->generalCounter3, 1, data);
			}
			canbadger->readTmpBuffer((canbadger->generalCounter3 + 13),1,data);//retrieve the length of the payload
			uint32_t pLen=(data[0] + 14);//calculate the total length of the packet including header
			canbadger->readTmpBuffer(canbadger->generalCounter3,pLen,data);
			canbadger->generalCounter3 = (canbadger->generalCounter3 + pLen);//add the total length, and subtract one so we are pointing at the last byte of the previous packet
			canbadger->generalCounter2--;//remove the packet we just wrote from the queue
			//ethManager->sendRamFrame(DATA, (char*)data, pLen);
			ethManager->sendMessageBlocking(DATA, NO_TYPE, (char*)data, pLen);
			frmCount++;
		}

		// run the EthernetManagers loop once
		ethManager->run();

		// check if we received new messages
		osEvent evt = canbadger->commandQueue->get(0);
		if(evt.status == osEventMail) {
			EthernetMessage *msg;
			msg = 0;
			msg = (EthernetMessage*) evt.value.p;
			if(msg != 0) {
				// check for ACTIONS we can execute while logging, disregard others
				// valid messages are: STOP_CURRENT_ACTION, START_REPLAY, RESET, RELAY, LED
				switch(msg->actionType)
				{

					case RESET:
						// close tcp connection before calling the handler again
						ethManager->closeConnection();
					case STOP_CURRENT_ACTION:
					case RELAY:
					case LED:
						handleEthernetMessage(msg, canbadger);
						break;
					case START_REPLAY:  // REPLAY needs special handling because logging is currently active
					{
						// send the message out of the given interface
						handleEthernetMessage(msg, canbadger);
						break;
					}
					default:
						break;
				}
			}
			canbadger->commandQueue->free(msg);
			delete msg;
		}
	}

	timer->stop();
	bool CAN1Logging=false;
	bool CAN2Logging=false;
	bool KLINE1Logging=false;
	bool KLINE2Logging=false;
	if(enableBridgeMode) {
			canbadger->setCANBadgerStatus(CAN1_TO_CAN2_BRIDGE, 0);
			canbadger->setCANBadgerStatus(CAN2_TO_CAN1_BRIDGE, 0);
			canbadger->setCANBadgerStatus(CAN2_LOGGING,0);
		}
	if(canbadger->getCANBadgerStatus(CAN1_LOGGING))//Disable logging so we dont end up getting more frames while we close the log
	{
		canbadger->setCANBadgerStatus(CAN1_LOGGING,0);
		CAN1Logging=true;
	}
	if(canbadger->getCANBadgerStatus(CAN2_LOGGING))//check on which interfaces to log. bitwise encoded
	{
		canbadger->setCANBadgerStatus(CAN2_LOGGING,0);
		CAN2Logging=true;
	}
	if(canbadger->getCANBadgerStatus(KLINE1_LOGGING))//check on which interfaces to log. bitwise encoded
	{
		canbadger->setCANBadgerStatus(KLINE1_LOGGING,0);
		KLINE1Logging=true;
	}
	if(canbadger->getCANBadgerStatus(KLINE2_LOGGING))//check on which interfaces to log. bitwise encoded
	{
		canbadger->setCANBadgerStatus(KLINE2_LOGGING,0);
		KLINE2Logging=true;
	}

	if(wasCANBridgeEnabled == false)//if bridge was not enabled before logging, we disable it
	{
		canbadger->CANBridge(0);
	}
	if(wasKLINEBridgeEnabled == false)//if bridge was not enabled before logging, we disable it
	{
		//KLINEBridge(0);
	}

	if(CAN1Logging == true)//If Logging was enabled, re-enable it
	{
		canbadger->setCANBadgerStatus(CAN1_LOGGING,1);
	}
	if(CAN2Logging == true)
	{
		canbadger->setCANBadgerStatus(CAN2_LOGGING,1);
	}
	if(KLINE1Logging == true)
	{
		canbadger->setCANBadgerStatus(KLINE1_LOGGING,1);
	}
	if(KLINE2Logging == true)
	{
		canbadger->setCANBadgerStatus(KLINE2_LOGGING,1);
	}
	return 1;

}

bool startUDSSession(CANbadger *canbadger, UDSSessionArgument *args)
{
	UDSResponse toServer;
	if((canbadger->getCANBadgerStatus(CAN1_INT_ENABLED) == 1 && (args->interface == 1)) || (canbadger->getCANBadgerStatus(CAN2_INT_ENABLED) == 1 && (args->interface == 2)) || canbadger->getCANBadgerStatus(CAN1_TO_CAN2_BRIDGE) == 1 || canbadger->getCANBadgerStatus(CAN1_TO_CAN2_BRIDGE) == 1) //need to make sure nothing that would interfere is enabled
	{
		return false;
	}

	CAN *can = canbadger->getCANClient(args->interface);
	if(can == NULL)
	{
		return false;
	}

	UDSCANHandler *uds = new UDSCANHandler(can);
	canbadger->setUDSHandler(uds);

	uds->setTransmissionParameters(args->localID, args->remoteID, args->format, args->enablePadding, args->paddingByte, args->addressType);

	canbadger->generalCounter1 = uds->DiagSessionStartup(args->targetDiagSession, canbadger->tmpBuffer);


	if(canbadger->generalCounter1 == 0)
	{
		// Timeout
		return false;
	}
	else if((canbadger->generalCounter1 & 0xFFFF0000) != 0)
	{
		// return non successful UDS Response
		toServer.SID = 0x10;
		toServer.success = false;
		toServer.dataLength = 8;
	}
	else
	{
		// return successful UDS response
		toServer.SID = 0x10;
		toServer.success = true;
		toServer.dataLength = canbadger->generalCounter1;
	}
	// send UDS response to server via EthernetMessage


	// sizeof(UDSResponse) is 8 due to Alignment, we use 7 Byte and put them in directly
	char em_data[7 + toServer.dataLength];

	em_data[0] = toServer.SID & 0xFF;
	em_data[1] = (toServer.SID >> 8) & 0xFF;
	em_data[2] = toServer.success;
	em_data[3] = toServer.dataLength & 0xFF;
	em_data[4] = (toServer.dataLength >> 8) & 0xFF;
	em_data[5] = (toServer.dataLength >> 16) & 0xFF;
	em_data[6] = (toServer.dataLength >> 24) & 0xFF;
	memcpy(em_data + 7, canbadger->tmpBuffer, toServer.dataLength);

	canbadger->getEthernetManager()->sendMessageBlocking(DATA, NO_TYPE, em_data, 7 + toServer.dataLength);

	return true;
}

bool handleUDSRequest(CANbadger *canbadger, UDSRequest *req, uint8_t *request_data)
{
	UDSResponse toServer;
	uint8_t data[1 + req->dataLength];

	UDSCANHandler *uds = canbadger->getUDSHandler();
	data[0] = req->SID;
	memcpy(data + 1, request_data, req->dataLength);

	canbadger->generalCounter1 = uds->requestResponseClient(data, 1 + req->dataLength, canbadger->tmpBuffer);


	if(canbadger->generalCounter1 == 0)
	{
		// Timeout
		return false;
	}
	else if((canbadger->generalCounter1 & 0xFFFF0000) != 0)
	{
		// return non successful UDS Response
		toServer.SID = req->SID;
		toServer.success = false;
		toServer.dataLength = 8;
	}
	else
	{
		// return successful UDS response
		toServer.SID =req->SID;
		toServer.success = true;
		toServer.dataLength = canbadger->generalCounter1;
	}
	// send UDS response to server via EthernetMessage


	// sizeof(UDSResponse) is 8 due to Alignment, we use 7 Byte and put them in directly
	char em_data[7 + toServer.dataLength];

	em_data[0] = toServer.SID & 0xFF;
	em_data[1] = (toServer.SID >> 8) & 0xFF;
	em_data[2] = toServer.success;
	em_data[3] = toServer.dataLength & 0xFF;
	em_data[4] = (toServer.dataLength >> 8) & 0xFF;
	em_data[5] = (toServer.dataLength >> 16) & 0xFF;
	em_data[6] = (toServer.dataLength >> 24) & 0xFF;
	memcpy(em_data + 7, canbadger->tmpBuffer, toServer.dataLength);

	canbadger->getEthernetManager()->sendMessageBlocking(DATA, NO_TYPE, em_data, 7 + toServer.dataLength);

	return true;
}

bool UDSSecurityHijack(CANbadger *canbadger, SecHijackRequest *hj_req)
{
	CANFormat format = (CANFormat) 0;
	CANMessage can1_msg(0, format);
	CANMessage can2_msg(0, format);

	EthernetManager *ethManager = canbadger->getEthernetManager();
	CanbadgerSettings *cbSettings = canbadger->getCanbadgerSettings();

	CAN *can1 = canbadger->getCANClient(0);
	CAN *can2 = canbadger->getCANClient(1);

	if(can1 == NULL || can2 == NULL)
	{
		return false;
	}

	uint8_t pwn=0;//used as a counter for actions
	uint8_t lvl = 0;//to grab the security level
	uint8_t cnnt=0;//to count frames and discard a false channel negociation
	uint32_t seed=0;
	uint32_t key=0;

	while(cbSettings->currentActionIsRunning)	//do unless aborted (or we return)
	{
		if(can1->read(can1_msg) && can1_msg.id == hj_req->remoteID)//target side
		{
			if(pwn == 1)//grab the reply to request
			{
				if((can1_msg.data[1] == 0x67 && can1_msg.data[2] == lvl) || (can1_msg.data[2] == 0x67 && can1_msg.data[3] == lvl))//single and multiframe
				{
					seed = can1_msg.data[3];
					seed = ((seed << 8) + can1_msg.data[4]);
					seed = ((seed << 8) + can1_msg.data[5]);
					seed = ((seed << 8) + can1_msg.data[6]);
					pwn++;
				}
			}
			else if(pwn == 3)//grab the reply to key
			{
				if((can1_msg.data[1] == 0x67 && can1_msg.data[2] == (lvl + 1)) || (can1_msg.data[2] == 0x67 && can1_msg.data[3] == (lvl + 1)))
				{
					//initialize UDSHandler
					UDSCANHandler *uds = new UDSCANHandler(can1);
					canbadger->setUDSHandler(uds);
					uds->setTransmissionParameters(hj_req->localID, hj_req->remoteID, (CANFormat) 0, true, 0x0);//so far we dont support extended addressing for hijack
					uds->setSessionStatus(true);

					canbadger->localID = hj_req->localID;
					canbadger->remoteID = hj_req->remoteID;

					//send the SecHijackResponse
					char em_data[3];
					em_data[0] = true;
					em_data[1] = canbadger->currentDiagSession & 0xFF;
					em_data[2] = (canbadger->currentDiagSession >> 8) & 0xFF;

					ethManager->sendMessageBlocking(DATA, NO_TYPE, em_data, 3);

					return 1;
				}
				else if(can1_msg.data[0] == 0x3 && can1_msg.data[1] == 0x7F && can1_msg.data[2] == 0x27 && can1_msg.data[3] == 0x35)//authentication failed
				{
					pwn = 0;
					seed = 0;
					key = 0;
					cnnt = 0;
				}
			}
			if(pwn > 0)
			{
				cnnt++;
				if (cnnt > 100)//timeout
				{
					pwn = 0;
					cnnt = 0;
				}
			}
			uint8_t timeout=0;
			while (!can2->write(CANMessage(can1_msg.id, reinterpret_cast<char*>(&can1_msg.data), can1_msg.len)) && timeout < 100)
			{
				timeout++;
			}//make sure the msg goes out
		}
		if(can2->read(can2_msg) && can2_msg.id == hj_req->localID)//tool side
		{
			if(can2_msg.data[0] == 0x2 && can2_msg.data[1] == 0x10 && can2_msg.len >= 3)//if a start diag session was requested
			{
				//need to persist the session somehow to later start UDSHandler
				canbadger->currentDiagSession = can2_msg.data[2];//we grab the session type for later use
			}
			if(pwn  == 0)//first we need to grab the request
			{
				if(can2_msg.data[0] == 0x2 && can2_msg.data[1] == 0x27 && hj_req->securityAccessLevel == 0)
				{
					if(hj_req->diagSessionLevel !=0)
					{
						uint8_t data[8]={0x02,0x10,hj_req->diagSessionLevel,0x0,0x0,0x0,0x0,0x0};
						uint32_t timeout=0;
						if(canbadger->getCANBadgerStatus(CAN2_USE_FULLFRAME) == true)
						{
							while (!can2->write(CANMessage(hj_req->localID, reinterpret_cast<char*>(data), 8, CANData, format)) && timeout < 10000)
							{
								wait(0.0001);
								timeout++;
							}//make sure the msg goes out
						}
						else
						{
							while (!can2->write(CANMessage(hj_req->localID, reinterpret_cast<char*>(data), 3, CANData, format)) && timeout < 10000)
							{
								wait(0.0001);
								timeout++;
							}//make sure the msg goes out
						}
					}
					pwn++;
					lvl = can2_msg.data[2];
				}
				else if(can2_msg.data[0] == 0x2 && can2_msg.data[1] == 0x27 && hj_req->securityAccessLevel == can2_msg.data[2])
				{
					pwn++;
					lvl = can2_msg.data[2];
				}
			}
			else if(pwn == 2)//then we grab the reply from tool
			{
				if(can2_msg.data[0] == 0x6 && can2_msg.data[1] == 0x27 && can2_msg.data[2] == (lvl + 1))
				{
					key = can2_msg.data[3];
					key = ((key << 8) + can2_msg.data[4]);
					key = ((key << 8) + can2_msg.data[5]);
					key = ((key << 8) + can2_msg.data[6]);
					pwn++;
				}
			}
			if(pwn > 0)
			{
				cnnt++;
				if (cnnt > 100)//timeout
				{
					pwn = 0;
					cnnt = 0;
				}
			}
			uint8_t timeout=0;
			while (!can1->write(CANMessage(can2_msg.id, reinterpret_cast<char*>(&can2_msg.data), can2_msg.len)) && timeout < 100)
			{
			 timeout++;
			}//make sure the msg goes out
		}
	}
	return 0;
}

// transfer the contents of the SD filesystem to the server
/*
 * if no SD is inserted, payload is just a null byte
 *
 * SD contents will be sent directory wise
 *
 * format is(directory and file names null terminated):
 * 		PARENT_DIR [ TYPE_BYTE FILE/DIR_NAME ]
 * 	with TYPE_BYTE 0xF0 for directories and 0x0F for files
 */
bool sendSDContents(CANbadger *canbadger, char* dirname, char* contents, size_t outputBufferSize) {
	if(!(canbadger->isSDInserted)) {
		// send only null to signal no SD
		char nothing = '\0';
		canbadger->getEthernetManager()->sendMessageBlocking(DATA, NO_TYPE, &nothing, 1);
		return false;
	}

	// send the contents of the given folder
	size_t content_size = parseDirContents(canbadger, contents, dirname, outputBufferSize);
	if(content_size > strlen(dirname) + 1) {  // only if there actually are contents
		canbadger->getEthernetManager()->sendMessageBlocking(DATA, NO_TYPE, contents, content_size);
	}


	// send the contents of the folders next
	// reuse content buffer to iterate through folders
	char* subdir;

	subdir = strchr(contents, '\0') + 1;

	while(*subdir == (char)0xF0 && (size_t)(subdir-contents) < content_size) {
		// check for buffer overflow
		if(strlen(dirname) + strlen(subdir) >= outputBufferSize) {
			subdir = strchr(subdir + 2, '\0') + 1;
			continue;
		}

		if(strlen(dirname) > 1) { strcat(dirname, "/"); }
		strcat(dirname, (const char*)++subdir);  // append subdir to directory epath

		//let the subdir send its contents
		sendSDContents(canbadger, dirname, contents, outputBufferSize);

		// fill the content buffer with this directories content again and find next subdirectory
		removeLastDir(dirname);
		content_size = parseDirContents(canbadger, contents, dirname, outputBufferSize);
		subdir = strchr(subdir + 1, '\0') + 1;
	}

	// send null to signal end of transmission if we are root
	if(strcmp(dirname, "/") == 0) {
		char nothing = '\0';
		canbadger->getEthernetManager()->sendMessageBlocking(DATA, NO_TYPE, &nothing, 1);
	}

	return true;
}

size_t parseDirContents(CANbadger *canbadger, char *contentBuffer, char *lookupDir, size_t bufferSize) {

	uint8_t written = 0;
	size_t nameLen = 0;
	bool isDir = false;
	vector<std::string> contents;
	canbadger->listAllFiles(lookupDir, &contents);

	nameLen = strlen(lookupDir);
	strcpy(contentBuffer, lookupDir);
	written = nameLen + 1;

	for(vector<std::string>::iterator it=contents.begin(); it != contents.end(); it++) {
		const char *dirname = (*(it)).c_str();
		if(strcmp("System Volume Information", dirname) == 0) {continue;}

		// check if it is a directory or file (but check for buffer overflows)
		if(strlen(lookupDir) + strlen(dirname) + 1 >= bufferSize) {
			isDir = false;
		} else {
			strcat(lookupDir, "/");
			strcat(lookupDir, dirname);
			isDir = canbadger->doesDirExist(lookupDir);
			removeLastDir(lookupDir);
		}

		// make sure to not write out of buffer
		nameLen = strlen(dirname);
		if(written + nameLen + 2 > bufferSize) {break;}

		isDir ? contentBuffer[written++] = 0xF0 : contentBuffer[written++] = 0x0F;
		strcpy(contentBuffer + written, dirname);
		written += nameLen + 1;
	}
	return written;
}

void writeFilenameToEEPROM(CANbadger *canbadger, uint8_t length, char *filename) {
	uint8_t *lpnt = &length;
	canbadger->writeEEPROM(CBS_COMP_SETT_BUFF_SIZE, 1, lpnt);
	canbadger->writeEEPROM(CBS_COMP_SETT_BUFF_SIZE + 1, length, (uint8_t*) filename);
}

//helper to parse uint32_t's from buffer, for e.g. message argument parsing
static uint32_t parse32(char *data, uint8_t offset, const char *endianess)
{
	// "BE" of "be" signal BigEndian, else we assume LittleEndian
	if(strcmp(endianess, "BE") == 0 || strcmp(endianess, "be") == 0) {
		return (data[offset] << 24) + (data[offset+1] << 16) + (data[offset+2] << 8) + data[offset+3];
	} else {
		return data[offset] + (data[offset+1] << 8) + (data[offset+2] << 16) + (data[offset+3] << 24);
	}
}

static void removeLastDir(char *dirPath) {
	//find the last / in the string
	char* last = strrchr(dirPath, '/');


	if(dirPath == last) {
		// if it is only on directory from root or root then return root
		*(last+1) = '\0';
	} else {
		// end directory before last /
		*last = '\0';
	}
}
