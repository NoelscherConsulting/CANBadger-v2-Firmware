/*
* KWP2000 on CAN LIBRARY USING TP
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


#include "mbed.h"
#include "kwp2k_can.h"

KWP2KCANHandler::KWP2KCANHandler(CAN *canbus, uint32_t CANFreq)
{
	_canbus=canbus;
	_canbus->frequency(CANFreq);
	tp = new TPHandler(_canbus);
	inSession=0;
	areFiltersActive=false;
	ownID=0;
	rID=0;
}

KWP2KCANHandler::~KWP2KCANHandler()
{
	delete tp;
}


void KWP2KCANHandler::attachToSession(uint32_t localID,uint32_t remoteID)
{
	ownID=localID;
	rID = remoteID;
	tp->resumeSession(localID, remoteID);
	inSession=1;
	tick.attach(this,&KWP2KCANHandler::sendTesterPresent, 0.5);
}


void KWP2KCANHandler::checkError(uint8_t errorCode, uint8_t *destination)
{
	KWP2KCommon kwpp;
	kwpp.checkError(errorCode, destination);
}
	
void KWP2KCANHandler::sendTesterPresent()
{
	if(inSession == 0)//if we are not in a session, why would we do dis
	{
		tick.detach();
		return;
	}
	uint8_t data[8]={KWP_TESTER_PRESENT,KWP_TESTER_PRESENT_REPLY_REQUIRED};//since we want a reply, we set the suppresPosRspMsgIndicationBit to 0.
	if(!tp->write(data,2))
	{
		inSession=0;
		tick.detach();
	}
	if(tp->read(data) == 0)
	{
		inSession=0;
		tick.detach();
	}
	if(data[0] != (KWP_TESTER_PRESENT + KWP_RESPONSE_OFFSET))
	{
		inSession=0;
		tick.detach();
	}
}


void KWP2KCANHandler::setTimeouts(uint32_t request, uint32_t response)
{
	tp->setTimeouts(request, response);
}


uint32_t KWP2KCANHandler::startComms(uint8_t *response, uint8_t SessionType)//Used to start or change a diagnostic session type
{
	uint8_t data[8]={KWP_START_DIAG_SESSION,SessionType};
	uint32_t len=requestResponseClient(data,2,response);
	if (len != 0 && (len & 0xFF) != 0 && response[0] == (KWP_START_DIAG_SESSION + KWP2K_RESPONSE_OFFSET) )
	{
		if(inSession == 0)
		{
			inSession = 1;
			tick.attach(this,&KWP2KCANHandler::sendTesterPresent, 0.5);
		}
	}
	return len;
}

void KWP2KCANHandler::setSessionStatus(bool status)
{
	if(status == false)
	{
		if(inSession == 1)
		{
			inSession = 0;
			tick.detach();
		}
	}
	else
	{
		if(inSession == 0)
		{
			inSession = 1;
			tick.attach(this,&KWP2KCANHandler::sendTesterPresent, 0.5);
		}
	}
}

bool KWP2KCANHandler::stopComms()
{
	uint8_t data[16]={KWP_STOP_DIAG_SESSION};
	uint32_t len=requestResponseClient(data, 1, data);
	if(inSession == 1)
	{
		inSession = 0;
		tick.detach();
	}
	if (len != 0 && (len & 0xFF) != 0 && data[0] == (KWP_STOP_DIAG_SESSION + KWP2K_RESPONSE_OFFSET) )
	{
		return true;
	}
	return false;
}

uint32_t KWP2KCANHandler::ECUReset(uint8_t resetType, uint8_t *response)
{
	uint8_t request[2]={KWP_ECU_RESET, resetType};
	return requestResponseClient(request, 2, response);
}

uint32_t KWP2KCANHandler::readECUID(uint8_t idType, uint8_t *response)
{
	uint8_t request[2]={KWP_READ_ECU_ID, idType};
	return requestResponseClient(request, 2, response);
}

uint32_t KWP2KCANHandler::readDataByLocalID(uint8_t dataType, uint8_t *response)
{
	uint8_t request[4]={KWP_READ_DATA_BY_LOCAL_ID, dataType, KWP_TRANSMISSION_MODE_SINGLE, 0xFF};//yes, the mode single makes the maximumnumberofresponsestosend useless, so what?
	return requestResponseClient(request, 4, response);
}

uint32_t KWP2KCANHandler::writeDataByLocalID(uint8_t dataType, uint8_t *data, uint32_t payLength, uint8_t *response)
{
	if(payLength > 4094)//Make sure we wont write OOB
	{
		return 0;
	}
	for (uint32_t a = (payLength + 1); a > 1; a--)
	{
		data[a] = data[(a - 2)];
	}
	data[0]=KWP_WRITE_DATA_BY_LOCAL_ID;
	data[1]=dataType;
	return requestResponseClient(data, (payLength + 2), response);
}

uint32_t KWP2KCANHandler::readDataByCommonID(uint16_t dataType, uint8_t *response)
{
	uint8_t request[5]={KWP_READ_DATA_BY_COMMON_ID, (uint8_t)(dataType >> 8), (uint8_t)(dataType), KWP_TRANSMISSION_MODE_SINGLE, 0xFF};
	return requestResponseClient(request, 5, response);
}

uint32_t KWP2KCANHandler::writeDataByCommonID(uint16_t dataType, uint8_t *data, uint32_t payLength, uint8_t *response)
{
	if(payLength > 4093)//Make sure we wont write OOB
	{
		return 0;
	}
	for (uint32_t a = (payLength + 2); a > 2; a--)
	{
		data[a] = data[(a - 3)];
	}
	data[0]=KWP_WRITE_DATA_BY_COMMON_ID;
	data[1]=(uint8_t)(dataType >> 8);
	data[2]=(uint8_t)(dataType);
	return requestResponseClient(data, (payLength + 3), response);
}


uint32_t KWP2KCANHandler::startRoutineByLocalID(uint8_t routineID, uint8_t *params, uint32_t payLength, uint8_t *response)
{
	if(payLength > 4094)//Make sure we dont write OOB
	{
		return 0;
	}
	for (uint32_t a = (payLength + 1); a > 1; a--)
	{
		params[a] = params[(a - 2)];
	}
	params[0]=KWP_START_ROUTINE_BY_LOCAL_ID;
	params[1]=routineID;
	return requestResponseClient(params, (payLength + 2), response);
}

uint32_t KWP2KCANHandler::stopRoutineByLocalID(uint8_t routineID, uint8_t *data, uint32_t payLength, uint8_t *response)
{
	if(payLength > 4094)//Make sure we dont write OOB
	{
		return 0;
	}
	for (uint32_t a = (payLength + 1); a > 1; a--)
	{
		data[a] = data[(a - 2)];
	}
	data[0]=KWP_STOP_ROUTINE_BY_LOCAL_ID;
	data[1]=routineID;
	return requestResponseClient(data, (payLength + 2), response);
}

uint32_t KWP2KCANHandler::requestRoutineResultsByLocalID(uint8_t routineID, uint8_t *response)
{
	uint8_t toSend[2]={KWP_REQUEST_ROUTINE_RESULTS_BY_LOCAL_ID,routineID};
	return requestResponseClient(toSend, 2, response);
}

uint32_t KWP2KCANHandler::startRoutineByAddress(uint32_t address, uint8_t *params, uint32_t payLength, uint8_t *response)
{
	if(payLength > 4092)//Make sure we dont write OOB
	{
		return 0;
	}
	for (uint32_t a = (payLength + 3); a > 3; a--)
	{
		params[a] = params[(a - 4)];
	}
	params[0]=KWP_START_ROUTINE_BY_ADDRESS;
	params[1]=(uint8_t)(address >> 16);
	params[2]=(uint8_t)(address >> 8);
	params[3]=(uint8_t)(address);
	return requestResponseClient(params, (payLength + 4), response);
}

uint32_t KWP2KCANHandler::stopRoutineByAddress(uint32_t address, uint8_t *params, uint32_t payLength, uint8_t *response)
{
	if(payLength > 4092)//Make sure we dont write OOB
	{
		return 0;
	}
	for (uint32_t a = (payLength + 3); a > 3; a--)
	{
		params[a] = params[(a - 4)];
	}
	params[0]=KWP_STOP_ROUTINE_BY_ADDRESS;
	params[1]=(uint8_t)(address >> 16);
	params[2]=(uint8_t)(address >> 8);
	params[3]=(uint8_t)(address);
	return requestResponseClient(params, (payLength + 4), response);
}

uint32_t KWP2KCANHandler::requestRoutineResultsByAddress(uint32_t address, uint8_t *response)
{
	uint8_t toSend[4]={KWP_REQUEST_ROUTINE_RESULTS_BY_ADDRESS,(uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)(address)};
	return requestResponseClient(toSend, 4, response);
}



uint32_t KWP2KCANHandler::readMemoryByAddress(uint32_t memAddress, uint8_t memSize, uint8_t *response, uint8_t transmissionMode)
{
	uint8_t toSend[6]={KWP_READ_MEMORY_BY_ADDRESS,(uint8_t)(memAddress >> 16),(uint8_t)(memAddress >> 8),(uint8_t)(memAddress), memSize, transmissionMode};
	return requestResponseClient(toSend, 6, response);
}

uint32_t KWP2KCANHandler::writeMemoryByAddress(uint8_t *data, uint32_t memAddress, uint8_t memSize, uint8_t *response)
{
	for (uint32_t a = (memSize + 4); a > 4; a--)
	{
		data[a] = data[(a - 5)];
	}
	data[0]=KWP_WRITE_MEMORY_BY_ADDRESS;
	data[1]=(uint8_t)(memAddress >> 16);
	data[2]=(uint8_t)(memAddress >> 8);
	data[3]=(uint8_t)(memAddress);
	data[4]=memSize;
	return requestResponseClient(data, (memSize + 5), response);
}

//The parameters for requestUpload are defined by manufacturer, so all we can do is just append  them to the SID request
uint32_t KWP2KCANHandler::requestUpload(uint8_t *data, uint32_t payLength, uint8_t *response)
{
	if(payLength > 4095)//Make sure we dont write OOB
	{
		return 0;
	}
	for (uint32_t a = payLength; a > 0; a--)
	{
		data[a] = data[(a - 1)];
	}
	data[0]=KWP_REQUEST_UPLOAD;
	return requestResponseClient(data, (payLength + 1), response);
}

//The parameters for requestDownload are defined by manufacturer, so all we can do is just append  them to the SID request
uint32_t KWP2KCANHandler::requestDownload(uint8_t *data, uint32_t payLength, uint8_t *response)
{
	if(payLength > 4095)//Make sure we dont write OOB
	{
		return 0;
	}
	for (uint32_t a = payLength; a > 0; a--)
	{
		data[a] = data[(a - 1)];
	}
	data[0]=KWP_REQUEST_DOWNLOAD;
	return requestResponseClient(data, (payLength + 1), response);
}

//The parameters for requestTransferExit are defined by manufacturer, so all we can do is just append  them to the SID request
uint32_t KWP2KCANHandler::requestTransferExit(uint8_t *data, uint32_t payLength, uint8_t *response)
{
	if(payLength > 4095)//Make sure we dont write OOB
	{
		return 0;
	}
	for (uint32_t a = payLength; a > 0; a--)
	{
		data[a] = data[(a - 1)];
	}
	data[0]=KWP_REQUEST_TRANSFER_EXIT;
	return requestResponseClient(data, (payLength + 1), response);
}

//The parameters for transferData are defined by manufacturer, so all we can do is just append  them to the SID request
uint32_t KWP2KCANHandler::transferData(uint8_t *data, uint32_t payLength, uint8_t *response)
{
	if(payLength > 4095)//Make sure we dont write OOB
	{
		return 0;
	}
	for (uint32_t a = payLength; a > 0; a--)
	{
		data[a] = data[(a - 1)];
	}
	data[0]=KWP_TRANSFER_DATA;
	return requestResponseClient(data, (payLength + 1), response);
}

//The parameters for clearDiagnosticInformation are defined by manufacturer, so all we can do is just append  them to the SID request
uint32_t KWP2KCANHandler::clearDTCs(uint8_t *data, uint32_t payLength, uint8_t *response)
{
	if(payLength > 4095)//Make sure we dont write OOB
	{
		return 0;
	}
	for (uint32_t a = payLength; a > 0; a--)
	{
		data[a] = data[(a - 1)];
	}
	data[0]=KWP_CLEAR_DTC;
	return requestResponseClient(data, (payLength + 1), response);
}

//The parameters for readDiagnosticTroubleCodes are defined by manufacturer, so all we can do is just append  them to the SID request
uint32_t KWP2KCANHandler::readDTCs(uint8_t *data, uint32_t payLength, uint8_t *response)
{
	if(payLength > 4095)//Make sure we dont write OOB
	{
		return 0;
	}
	for (uint32_t a = payLength; a > 0; a--)
	{
		data[a] = data[(a - 1)];
	}
	data[0]=KWP_READ_DTC;
	return requestResponseClient(data, (payLength + 1), response);
}


uint8_t KWP2KCANHandler::getDTCList(uint8_t *source, uint8_t *destination)
{
	uint8_t toReturn=source[0];// grab the ammount of DTCs
	for(uint16_t a=0; a < toReturn; a++)
	{
		destination[a] = source[(a+1)];
	}
	return toReturn;
}

void KWP2KCANHandler::checkDTC(uint16_t DTCCode, uint8_t statusByte, char *destination)
{
	KWP2KCommon kwpp;
	kwpp.checkDTC(DTCCode, destination);
	if(statusByte == 0xE0)//this would mean MIL is on
	{
		strcat(destination,"MIL ON\n");
	}
}




uint32_t KWP2KCANHandler::requestSeed(uint8_t *payload, uint32_t paylength, uint8_t *response)
{
	if(paylength > 4095)//make sure we wont write OOB
	{
		return 0;
	}
	for (uint32_t a = paylength; a > 0; a--)
	{
		payload[a] = payload[(a - 1)];
	}
	payload[0]=KWP_SECURITY_ACCESS;
	return requestResponseClient(payload, (paylength + 1), response);
}


uint32_t KWP2KCANHandler::requestSeed(uint8_t level, uint8_t *response)
{

	uint8_t request[2]={KWP_SECURITY_ACCESS, level};
	return requestResponseClient(request, 2, response);
}


uint32_t KWP2KCANHandler::sendKey(uint8_t level, uint32_t key, uint8_t *response)
{

	uint8_t request[6]={KWP_SECURITY_ACCESS, (level + 1), (uint8_t)(key >> 24), (uint8_t)(key >> 16), (uint8_t)(key >> 8), (key & 0xFF)};
	return requestResponseClient(request, 6, response);
}


uint32_t KWP2KCANHandler::sendKey(uint8_t level, uint8_t *keyBytes, uint32_t keyLength, uint8_t *response)
{
	if(keyLength > 4094)//make sure we dont write OOB
	{
		return 0;
	}
	for (uint32_t a = (keyLength + 1); a > 1; a--)
	{
		keyBytes[a] = keyBytes[(a - 2)];
	}
	keyBytes[0]=KWP_SECURITY_ACCESS;
	keyBytes[1]=(level + 1);
	return requestResponseClient(keyBytes, (keyLength + 2), response);
}


bool KWP2KCANHandler::sessionStatus()
{
	return inSession;
}

void KWP2KCANHandler::endSession()
{
	if(inSession==1)
	{
		tick.detach();
		inSession=0;
	}
}
	
uint32_t KWP2KCANHandler::requestResponseClient(uint8_t *rqst, uint16_t len, uint8_t *response, bool ignoreACK)
{
	if(inSession != 0)
	{
		tick.detach();
	}
	if(!write(rqst,len))
	{
		return 0;
	}
	uint32_t readOp = read(response, ignoreACK);
	if(inSession != 0)
	{
		tick.attach(this,&KWP2KCANHandler::sendTesterPresent, 0.5);
	}
	return readOp;
}

void KWP2KCANHandler::setTransmissionParameters(uint32_t sourceID, uint32_t targetID, CANFormat doFrameFormat, bool doUseFullFrame, uint8_t doBsByte, uint8_t doVariant, bool useFilters)
{
	tp->setTransmissionParameters(sourceID, targetID, doFrameFormat, doUseFullFrame, doBsByte, doVariant, useFilters);
	if(useFilters == true)//update filter status
	{
		areFiltersActive=true;
	}
	else
	{
		areFiltersActive=false;
	}
}

uint32_t KWP2KCANHandler::read(uint8_t *response, bool ignoreACK)
{
	while(1)//we do this so we wait until we get a reply in case of error 0x78
	{
		uint32_t a = tp->read(response);
		if(a == 0)//if no response
		{
			return 0;
		}
		if(response[0] == 0x7F && response[2] != 0x78)
		{
			uint32_t reason = response[1];
			reason = (reason << 8);
			reason = (reason + response[2]);
			reason = (reason << 16);
			return reason;
		}
		else if(response[0] == 0x7F && response[2] == 0x78)//if it is waiting for the reply
		{
			 if (ignoreACK == true)
			 {
				 return 3;
			 }
		}
		else
		{
			return a;
		}
	}
}

bool KWP2KCANHandler::write(uint8_t *request, uint16_t len)
{
	return tp->write(request,len);
}

void KWP2KCANHandler::disableFilters()
{
	tp->disableFilters();
	areFiltersActive=false;
}

void KWP2KCANHandler::enableFilters()
{
	tp->enableFilters();
	areFiltersActive=true;
}
