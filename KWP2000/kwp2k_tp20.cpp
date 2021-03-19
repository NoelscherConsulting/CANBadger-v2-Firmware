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
#include "kwp2k_tp20.h"


KWP2KTP20Handler::KWP2KTP20Handler(CAN *canbus, uint8_t interface, uint32_t CANFreq)
{
	_canbus=canbus;
	_canbus->frequency(CANFreq);
	tp = new TP20Handler(_canbus, interface);
	inSession=0;
	areFiltersActive=true;
	ownID=0;
	rID=0;
}

KWP2KTP20Handler::~KWP2KTP20Handler()
{
	delete tp;
}


uint8_t KWP2KTP20Handler::channelSetup(const uint32_t requestAddress, uint8_t target_ID, uint8_t appType)
{
	uint32_t a = tp->channelSetup(requestAddress, target_ID, appType);
	if(a == 1)
	{
		inSession=1;
	}
	return a;
}


void KWP2KTP20Handler::checkError(uint8_t errorCode, uint8_t *destination)
{
	KWP2KCommon kwpp;
	kwpp.checkError(errorCode, destination);
}

void KWP2KTP20Handler::attachToSession(uint32_t localID,uint32_t remoteID, uint8_t tpCounter)
{
	ownID=localID;
	rID = remoteID;
	tp->resumeSession(localID, remoteID, tpCounter);
	inSession=1;

}

void KWP2KTP20Handler::sendTesterPresent()
{

	if(inSession == 0)//if we are not in a session, why would we do dis
	{
		tick.detach();
		return;
	}
	if(tp->stillInSession() == false)
	{
		tick.detach();
		inSession = 0;
		return;
	}
	uint8_t data[8]={KWP_TESTER_PRESENT};//since we want a reply, we set the suppresPosRspMsgIndicationBit to 0.
	if(tp->requestResponseClient(data,1,data) == 0)
	{
		inSession=0;
		tick.detach();
	}
	/*if(data[0] != (KWP_TESTER_PRESENT + KWP_RESPONSE_OFFSET))//sometimes tester present is not supported, but it doesnt mean that session is expired
	{
		inSession=0;
		tick.detach();
	}*/
}


void KWP2KTP20Handler::setTimeouts(uint32_t request, uint32_t response)
{
	tp->setTimeouts(request, response);
}


uint32_t KWP2KTP20Handler::startComms(uint8_t *response, uint8_t SessionType)//Used to start or change a diagnostic session type
{
	uint8_t data[8]={KWP_START_DIAG_SESSION,SessionType};
	uint32_t len=requestResponseClient(data,2,response);
	if (len != 0 && (len & 0xFF) != 0 && response[0] == (KWP_START_DIAG_SESSION + KWP2K_RESPONSE_OFFSET) )
	{
		if(inSession == 0)
		{
			inSession = 1;
			tick.attach(this,&KWP2KTP20Handler::sendTesterPresent, 0.5);
		}
	}
	return len;
}

uint8_t KWP2KTP20Handler::silentCloseChannel()
{
	inSession = 0;
	tick.detach();
	uint8_t a = tp->getCurrentCounter();
	tp->closeChannel(true);
	return a;
}

bool KWP2KTP20Handler::closeChannel()
{
	inSession = 0;
	tick.detach();
	return tp->closeChannel();
}

bool KWP2KTP20Handler::stopComms()
{
	uint8_t data[16]={KWP_STOP_COMMUNICATIONS};
	uint32_t len=requestResponseClient(data, 1, data);
	if(inSession == 1)
	{
		inSession = 0;
		tick.detach();
	}
	if (len != 0 && (len & 0xFF) != 0 && data[0] == (KWP_STOP_COMMUNICATIONS + KWP2K_RESPONSE_OFFSET) )
	{
		return true;
	}
	return false;
}

uint32_t KWP2KTP20Handler::ECUReset(uint8_t resetType, uint8_t *response)
{
	uint8_t request[2]={KWP_ECU_RESET, resetType};
	return requestResponseClient(request, 2, response);
}

uint32_t KWP2KTP20Handler::readECUID(uint8_t idType, uint8_t *response)
{
	uint8_t request[2]={KWP_READ_ECU_ID, idType};
	return requestResponseClient(request, 2, response);
}

uint32_t KWP2KTP20Handler::readDataByLocalID(uint8_t dataType, uint8_t *response)
{
	uint8_t request[4]={KWP_READ_DATA_BY_LOCAL_ID, dataType, KWP_TRANSMISSION_MODE_SINGLE, 0xFF};//yes, the mode single makes the maximumnumberofresponsestosend useless, so what?
	return requestResponseClient(request, 2, response);
}

uint32_t KWP2KTP20Handler::writeDataByLocalID(uint8_t dataType, uint8_t *data, uint32_t payLength, uint8_t *response)
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

uint32_t KWP2KTP20Handler::readDataByCommonID(uint16_t dataType, uint8_t *response)
{
	uint8_t request[5]={KWP_READ_DATA_BY_COMMON_ID, (uint8_t)(dataType >> 8), (uint8_t)(dataType)};//, KWP_TRANSMISSION_MODE_SINGLE, 0xFF};
	return requestResponseClient(request, 3, response);
}

uint32_t KWP2KTP20Handler::writeDataByCommonID(uint16_t dataType, uint8_t *data, uint32_t payLength, uint8_t *response)
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


uint32_t KWP2KTP20Handler::startRoutineByLocalID(uint8_t routineID, uint8_t *params, uint32_t payLength, uint8_t *response)
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

uint32_t KWP2KTP20Handler::stopRoutineByLocalID(uint8_t routineID, uint8_t *data, uint32_t payLength, uint8_t *response)
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

uint32_t KWP2KTP20Handler::requestRoutineResultsByLocalID(uint8_t routineID, uint8_t *response)
{
	uint8_t toSend[2]={KWP_REQUEST_ROUTINE_RESULTS_BY_LOCAL_ID,routineID};
	return requestResponseClient(toSend, 2, response);
}

uint32_t KWP2KTP20Handler::startRoutineByAddress(uint32_t address, uint8_t *params, uint32_t payLength, uint8_t *response)
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

uint32_t KWP2KTP20Handler::stopRoutineByAddress(uint32_t address, uint8_t *params, uint32_t payLength, uint8_t *response)
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

uint32_t KWP2KTP20Handler::requestRoutineResultsByAddress(uint32_t address, uint8_t *response)
{
	uint8_t toSend[4]={KWP_REQUEST_ROUTINE_RESULTS_BY_ADDRESS,(uint8_t)(address >> 16), (uint8_t)(address >> 8), (uint8_t)(address)};
	return requestResponseClient(toSend, 4, response);
}



uint32_t KWP2KTP20Handler::readMemoryByAddress(uint32_t memAddress, uint8_t memSize, uint8_t *response, uint8_t transmissionMode)
{
	uint8_t toSend[6]={KWP_READ_MEMORY_BY_ADDRESS,(uint8_t)(memAddress >> 16),(uint8_t)(memAddress >> 8),(uint8_t)(memAddress), memSize, transmissionMode};
	return requestResponseClient(toSend, 6, response);
}

uint32_t KWP2KTP20Handler::writeMemoryByAddress(uint8_t *data, uint32_t memAddress, uint8_t memSize, uint8_t *response)
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
uint32_t KWP2KTP20Handler::requestUpload(uint8_t *data, uint32_t payLength, uint8_t *response)
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
uint32_t KWP2KTP20Handler::requestDownload(uint8_t *data, uint32_t payLength, uint8_t *response)
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
uint32_t KWP2KTP20Handler::requestTransferExit(uint8_t *data, uint32_t payLength, uint8_t *response)
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
uint32_t KWP2KTP20Handler::transferData(uint8_t *data, uint32_t payLength, uint8_t *response)
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
uint32_t KWP2KTP20Handler::clearDTCs(uint8_t *data, uint32_t payLength, uint8_t *response)
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
uint32_t KWP2KTP20Handler::readDTCs(uint8_t *data, uint32_t payLength, uint8_t *response)
{
	if(payLength > 4095)//Make sure we dont write OOB
	{
		return 0;
	}
	for (uint32_t a = payLength; a > 0; a--)
	{
		data[a] = data[(a - 1)];
	}
	data[0]=KWP_READ_DTC_BY_STATUS;
	return requestResponseClient(data, (payLength + 1), response);
}


uint8_t KWP2KTP20Handler::getDTCList(uint8_t *source, uint8_t *destination)
{
	uint8_t toReturn=source[1];// grab the ammount of DTCs
	for(uint16_t a=0; a < (toReturn * 3); a++)
	{
		destination[a] = source[(a+2)];
	}
	return toReturn;
}

void KWP2KTP20Handler::checkDTC(uint16_t DTCCode, uint8_t statusByte, char *destination)
{
	KWP2KCommon kwpp;
	kwpp.checkDTC(DTCCode, destination);
	if(statusByte == 0xE0)//this would mean MIL is on
	{
		strcat(destination,"MIL ON\n");
	}
}



uint32_t KWP2KTP20Handler::requestSeed(uint8_t *payload, uint32_t paylength, uint8_t *response)
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


uint32_t KWP2KTP20Handler::requestSeed(uint8_t level, uint8_t *response)
{

	uint8_t request[2]={KWP_SECURITY_ACCESS, level};
	return requestResponseClient(request, 2, response);
}


uint32_t KWP2KTP20Handler::sendKey(uint8_t level, uint32_t key, uint8_t *response)
{

	uint8_t request[6]={KWP_SECURITY_ACCESS, (uint8_t)(level + 1), (uint8_t)(key >> 24), (uint8_t)(key >> 16), (uint8_t)(key >> 8), (uint8_t)(key & 0xFF)};
	return requestResponseClient(request, 6, response);
}


uint32_t KWP2KTP20Handler::sendKey(uint8_t level, uint8_t *keyBytes, uint32_t keyLength, uint8_t *response)
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


bool KWP2KTP20Handler::sessionStatus()
{
	return inSession;
}

void KWP2KTP20Handler::endSession()
{
	if(inSession==1)
	{
		tick.detach();
		inSession=0;
	}
}

uint32_t KWP2KTP20Handler::requestResponseClient(uint8_t *rqst, uint16_t len, uint8_t *response, bool ignoreACK)
{
	if(inSession != 0)
	{
		tick.detach();
	}
	uint32_t readOp = tp->requestResponseClient(rqst, len, response);
	if(inSession != 0)
	{
		tick.attach(this,&KWP2KTP20Handler::sendTesterPresent, 0.5);
	}
	if(response[0] == 0x7F)//if we got an error
	{
		uint32_t reason = response[1];
		reason = (reason << 8);
		reason = (reason + response[2]);
		reason = (reason << 16);
		return reason;
	}
	return readOp;
}

void KWP2KTP20Handler::setTransmissionParameters(bool useFilters)
{
	tp->setTransmissionParameters(useFilters);
	if(useFilters == true)//update filter status
	{
		areFiltersActive=true;
	}
	else
	{
		areFiltersActive=false;
	}
}

uint32_t KWP2KTP20Handler::read(uint8_t *response, bool ignoreACK)
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

bool KWP2KTP20Handler::write(uint8_t *request, uint16_t len)
{
	return tp->write(request,len);
}

void KWP2KTP20Handler::disableFilters()
{
	if(areFiltersActive==true)
	{
		tp->disableFilters();
		areFiltersActive=false;
	}
}

void KWP2KTP20Handler::enableFilters()
{
	if(areFiltersActive==false)
	{
		tp->enableFilters();
		areFiltersActive=true;
	}
}
