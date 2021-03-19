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
*
*/

/*
 * Contains functions to handle incoming commands over Ethernet
 * that function like a basic API
 */

#ifndef CANBADGER_COMMAND_HANDLER_HPP_
#define CANBADGER_COMMAND_HANDLER_HPP_

#include "ethernet_message.hpp"
#include "canbadger.h"
#include "can_helper.h"
#include "fileHandler.h"


// forward declarations
class CANbadger;
class FileHandler;

bool handleEthernetMessage(EthernetMessage *msg, CANbadger *canbadger);

bool canLogging(CANbadger *canbadger, bool enableBridgeMode);

bool startUDSSession(CANbadger *canbadger, UDSSessionArgument *args);

bool handleUDSRequest(CANbadger *canbadger, UDSRequest *req, uint8_t *request_data);

bool UDSSecurityHijack(CANbadger *canbadger, SecHijackRequest *hj_req);

bool sendSDContents(CANbadger *canbadger, char* dirname, char* contents, size_t outputBufferSize);

size_t parseDirContents(CANbadger *canbadger, char *contentBuffer, char *lookupDir, size_t bufferSize);

void writeFilenameToEEPROM(CANbadger *canbadger, uint8_t length, char *filename);

static uint32_t parse32(char *data, uint8_t offset, const char *endianess);

static void removeLastDir(char* dirPath);

#endif /* CANBADGER_COMMAND_HANDLER_HPP_ */
