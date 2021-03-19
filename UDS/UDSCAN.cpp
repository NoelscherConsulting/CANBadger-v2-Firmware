/*
* UDS LIBRARY USING ISO-TP (ISO 15765)
* Copyright (c) 2019 Javier Vazquez
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


#include "UDSCAN.h"

UDSCANHandler::UDSCANHandler(CAN *canbus, uint32_t CANFreq)
{
	_canbus=canbus;
	_canbus->frequency(CANFreq);
	tp = new TPHandler(_canbus);
	inSession=0;
	areFiltersActive=false;
}

UDSCANHandler::~UDSCANHandler()
{
	delete tp;
}
	
void UDSCANHandler::disableFilters()
{
	tp->disableFilters();
	areFiltersActive=false;
}

void UDSCANHandler::enableFilters()
{
	tp->enableFilters();
	areFiltersActive=true;
}

void UDSCANHandler::sendTesterPresent()
{
	if(inSession == 0)//if we are not in a session, why would we do dis
	{
		tick.detach();
		return;
	}
	uint8_t data[8]={UDS_TESTER_PRESENT,UDS_TESTER_PRESENT_ZERO_SUB};//since we want a reply, we set the suppresPosRspMsgIndicationBit to 0.
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
	if(data[0] != (UDS_TESTER_PRESENT + UDS_RESPONSE_OFFSET))
	{
		inSession=0;
		tick.detach();
	}
}


void UDSCANHandler::setTimeouts(uint32_t request, uint32_t response)
{
	tp->setTimeouts(request, response);
}


uint32_t UDSCANHandler::DiagSessionControl(uint8_t SessionType)//Used to start or change a diagnostic session type
{
	
  uint8_t data[8]={UDS_DIAGNOSTIC_SESSION_CONTROL,SessionType};
  uint8_t buffer[256];//it would be crazy if we ever got more than this on a reply to a diagsession
  uint32_t len=requestResponseClient(data,2,buffer);
  if (len != 0 && (len & 0xFF) != 0 && buffer[0] == (UDS_DIAGNOSTIC_SESSION_CONTROL + UDS_RESPONSE_OFFSET) )
  {
	if(inSession == 0)
	{
		inSession = 1;
		tick.attach(this,&UDSCANHandler::sendTesterPresent, 0.5);
	}
  }
  return len;
}

uint32_t UDSCANHandler::DiagSessionStartup(uint8_t SessionType, uint8_t *response_buffer)//Used to start or change a diagnostic session type
{

  uint8_t data[8]={UDS_DIAGNOSTIC_SESSION_CONTROL,SessionType};
  uint32_t len = requestResponseClient(data,2,response_buffer);
  if (len != 0 && (len & 0xFF) != 0 && response_buffer[0] == (UDS_DIAGNOSTIC_SESSION_CONTROL + UDS_RESPONSE_OFFSET) )
  {
	if(inSession == 0)
	{
		inSession = 1;
		tick.attach(this,&UDSCANHandler::sendTesterPresent, 0.5);
	}
  }
  return len;
}

uint32_t UDSCANHandler::communicationControl(uint8_t controlType, uint8_t communicationType, uint8_t *response, uint16_t nodeID)
{
	uint8_t request[5]={UDS_COMMUNICATION_CONTROL, controlType, communicationType, (uint8_t)(nodeID >> 8),(uint8_t)(nodeID)};
	if(controlType == 4 || controlType == 5)//if the request requires the NodeID
	{
		return requestResponseClient(request, 5, response);
	}
	else//but if it doesnt
	{
		return requestResponseClient(request, 3, response);
	}
}



uint32_t UDSCANHandler::ECUReset(uint8_t resetType, uint8_t *response)
{
	uint8_t request[2]={UDS_ECU_RESET, resetType};
	return requestResponseClient(request, 2, response);
}

uint32_t UDSCANHandler::readDataByID(uint16_t dataType, uint8_t *response)
{
	uint8_t request[3]={UDS_READ_DATA_BY_ID,(uint8_t)(dataType >> 8),(uint8_t)(dataType & 0xFF)};
	return requestResponseClient(request, 3, response);
}

uint32_t UDSCANHandler::writeDataByID(uint16_t dataType, uint8_t *data, uint32_t payLength, uint8_t *response)
{
	if(payLength > 4093)//Make sure we wont write OOB
	{
		return 0;
	}
	for (uint32_t a = (payLength + 2); a > 2; a--)
	{
		data[a] = data[(a - 3)];
	}
	data[0]=UDS_WRITE_DATA_BY_ID;
	data[1]=(uint8_t)(dataType >> 8);
	data[2]=(uint8_t)(dataType & 0xFF);
	return requestResponseClient(data, (payLength + 3), response);
}

uint32_t UDSCANHandler::routineControl(uint8_t requestType, uint16_t routineType, uint8_t *parameters, uint32_t paramLength, uint8_t *response)
{
	if(paramLength > 4092)//Make sure we dont write OOB
	{
		return 0;
	}
	for (uint32_t a = (paramLength + 3); a > 3; a--)
	{
		parameters[a] = parameters[(a - 4)];
	}
	parameters[0]=UDS_ROUTINE_CONTROL;
	parameters[1]=requestType;
	parameters[2]=(uint8_t)(routineType >> 8);
	parameters[3]=(uint8_t)(routineType & 0xFF);
	return requestResponseClient(parameters, (paramLength + 4), response);
}

uint32_t UDSCANHandler::requestDownload(uint8_t dataFormatID, uint64_t memAddress, uint32_t memSize, uint8_t *response)
{
		uint8_t request[16]={UDS_REQUEST_DOWNLOAD,dataFormatID};
		uint8_t addrAndFormatID=1;
		if(memAddress > 0xFF && memAddress < 0x10000)//calculate the number of memory address bytes
		{
			addrAndFormatID=2;
		}
		else if(memAddress > 0xFFFF && memAddress < 0x1000000)
		{
			addrAndFormatID=3;
		}
		else if(memAddress > 0xFFFFFF && memAddress < 0x100000000)
		{
			addrAndFormatID=4;
		}
		else if(memAddress > 0xFFFFFFFF && memAddress < 0x10000000000)
		{
			addrAndFormatID=5;
		}
		else if(memAddress > 0xFFFFFFFFFF && memAddress < 0x1000000000000)
		{
			addrAndFormatID=6;
		}
		else if(memAddress > 0xFFFFFFFFFFFF && memAddress < 0x100000000000000)
		{
			addrAndFormatID=7;
		}
		else if(memAddress > 0xFFFFFFFFFFFFFF)
		{
			addrAndFormatID=8;
		}
		if(memSize < 0x100)//calculate the number of memory size bytes
		{
			addrAndFormatID= (addrAndFormatID + 0x10);
		}
		else if(memSize > 0xFF && memSize < 0x10000)
		{
			addrAndFormatID= (addrAndFormatID + 0x20);
		}
		else if(memSize > 0xFFFF && memSize < 0x1000000)
		{
			addrAndFormatID= (addrAndFormatID + 0x30);
		}		
		else
		{
			addrAndFormatID= (addrAndFormatID + 0x40);
		}	
		request[2]=addrAndFormatID;
		uint8_t a =(addrAndFormatID & 0x0F);//get the number of memory address bytes
		uint8_t b =(addrAndFormatID >> 4) & 0x0F;//get the number of memsize bytes
		uint64_t tmpAddr=memAddress; //FFCAA0
		uint32_t tmpmemSize=memSize; //500
		for (uint8_t c = a; c > 0; c--)//place the memory address in the request
		{
			request[(c+2)]=(uint8_t)(tmpAddr & 0xFF);
			tmpAddr = (tmpAddr >> 8);
		}
		for (uint8_t c = b; c > 0; c--)//place the memory size in the request
		{
			request[(a+c+2)]=(uint8_t)(tmpmemSize & 0xFF);
			tmpmemSize = (tmpmemSize >> 8);
		}		
		return requestResponseClient(request, (a+b+3), response);
}

uint32_t UDSCANHandler::transferData(uint8_t blockNumber, uint8_t *data, uint16_t length, uint8_t *response, bool ignoreACK)
{
	if(length > 4094)//check to make sure we dont write OOB
	{
		return 0;
	}
	for(uint16_t a = (length + 1); a > 1; a--)
	{
		data[a] = data[(a - 2)];
	}
	data[0] = UDS_TRANSFER_DATA;
	data[1] = blockNumber;
	return requestResponseClient(data, (length+2), response, ignoreACK);
}

uint32_t UDSCANHandler::requestTransferExit(uint8_t *params, uint16_t paramLen, uint8_t *response)
{
	if(paramLen > 4095)//if its bigger than what can be sent
	{
		return 0;
	}
	for(uint16_t a = paramLen; a > 0; a--)
	{
		params[a] = params[(a - 1)];
	}
	params[0] = UDS_REQUEST_TRANSFER_EXIT;
	return requestResponseClient(params, (paramLen + 1), response);
}

uint32_t UDSCANHandler::getBlockSize(uint8_t *response)
{
	uint8_t howMany=(response[1] >> 4);//get how many bytes are used for the block size
	uint32_t toReturn=0;
	for(uint8_t a=0; a < howMany; a++)
	{
		toReturn = (toReturn << 8) + response[(a+2)];
	}
	return toReturn;
}

uint32_t UDSCANHandler::requestUpload(uint8_t dataFormatID, uint64_t memAddress, uint32_t memSize, uint8_t *response)
{
		uint8_t request[12]={UDS_REQUEST_UPLOAD,dataFormatID};
		uint8_t addrAndFormatID=1;
		if(memAddress > 0xFF && memAddress < 0x10000)//calculate the number of memory address bytes
		{
			addrAndFormatID=2;
		}
		else if(memAddress > 0xFFFF && memAddress < 0x1000000)
		{
			addrAndFormatID=3;
		}
		else if(memAddress > 0xFFFFFF && memAddress < 0x100000000)
		{
			addrAndFormatID=4;
		}
		else if(memAddress > 0xFFFFFFFF && memAddress < 0x10000000000)
		{
			addrAndFormatID=5;
		}
		else if(memAddress > 0xFFFFFFFFFF && memAddress < 0x1000000000000)
		{
			addrAndFormatID=6;
		}
		else if(memAddress > 0xFFFFFFFFFFFF && memAddress < 0x100000000000000)
		{
			addrAndFormatID=7;
		}
		else if(memAddress > 0xFFFFFFFFFFFFFF)
		{
			addrAndFormatID=8;
		}
		if(memSize < 0x100)//calculate the number of memory size bytes
		{
			addrAndFormatID= (addrAndFormatID + 0x10);
		}
		else if(memSize > 0xFF && memSize < 0x10000)
		{
			addrAndFormatID= (addrAndFormatID + 0x20);
		}
		else if(memSize > 0xFFFF && memSize < 0x1000000)
		{
			addrAndFormatID= (addrAndFormatID + 0x30);
		}		
		else
		{
			addrAndFormatID= (addrAndFormatID + 0x40);
		}	
		request[2]=addrAndFormatID;
		uint8_t a =(addrAndFormatID & 0x0F);//get the number of memory address bytes
		uint8_t b =(addrAndFormatID >> 4) & 0x0F;//get the number of memsize bytes
		uint64_t tmpAddr=memAddress;
		uint32_t tmpmemSize=memSize;
		for (uint8_t c = a; c > 0; c--)//place the memory address in the request
		{
			request[(a+2)]=(uint8_t)(tmpAddr & 0xFF);
			tmpAddr = (tmpAddr >> 8);
		}
		for (uint8_t c = b; c > 0; c--)//place the memory size in the request
		{
			request[(a+b+2)]=(uint8_t)(tmpmemSize & 0xFF);
			tmpAddr = (tmpmemSize >> 8);
		}		
		return requestResponseClient(request, (a+b+3), response);
}

uint32_t UDSCANHandler::requestSeed(uint8_t *payload, uint32_t paylength, uint8_t *response)
{
	if(paylength > 4095)//make sure we wont write OOB
	{
		return 0;
	}
	for (uint32_t a = paylength; a > 0; a--)
	{
		payload[a] = payload[(a - 1)];
	}
	payload[0]=UDS_SECURITY_ACCESS;
	return requestResponseClient(payload, (paylength + 1), response);
}


uint32_t UDSCANHandler::requestSeed(uint8_t level, uint8_t *response)
{

	uint8_t request[2]={UDS_SECURITY_ACCESS, level};
	return requestResponseClient(request, 2, response);
}


uint32_t UDSCANHandler::sendKey(uint8_t level, uint32_t key, uint8_t *response)
{

	uint8_t request[6]={UDS_SECURITY_ACCESS, (level + 1), (uint8_t)(key >> 24), (uint8_t)(key >> 16), (uint8_t)(key >> 8), (key & 0xFF)};
	return requestResponseClient(request, 6, response);
}


void UDSCANHandler::checkDTC(uint32_t DTCCode, char *destination)//to have some verbosity on the DTC error codes
{
	Conversions convert;
	char z[22];
	uint8_t maskbit=0;
	uint32_t fault=(DTCCode >> 8);
	uint8_t status=DTCCode & 0xFF;
	sprintf(destination,"Code: ");
	uint8_t tmpVal=0;
	tmpVal=convert.getBit(fault,15);//first we will get the letter
	tmpVal=(tmpVal << 1)+convert.getBit(fault,14);
	switch (tmpVal)
	{
		case 0:
		{
			sprintf(destination,"P");
			break;
		}
		case 1:
		{
			sprintf(destination,"C");
			break;
		}
		case 2:
		{
			sprintf(destination,"B");
			break;
		}
		case 3:
		{
			sprintf(destination,"U");
			break;
		}
	}
	tmpVal=convert.getBit(fault,13);
	tmpVal=(tmpVal << 1)+convert.getBit(fault,12);
	convert.itoa(tmpVal,z,1);
	strcat(destination,z);
	tmpVal=convert.getBit(fault,11);
	tmpVal=(tmpVal << 1)+convert.getBit(fault,10);
	tmpVal=(tmpVal << 1)+convert.getBit(fault,9);
	tmpVal=(tmpVal << 1)+convert.getBit(fault,8);
	convert.itox(tmpVal,z,1);
	strcat(destination,z);
	tmpVal=convert.getBit(fault,7);
	tmpVal=(tmpVal << 1)+convert.getBit(fault,6);
	tmpVal=(tmpVal << 1)+convert.getBit(fault,5);
	tmpVal=(tmpVal << 1)+convert.getBit(fault,4);
	convert.itox(tmpVal,z,1);
	strcat(destination,z);
	tmpVal=convert.getBit(fault,3);
	tmpVal=(tmpVal << 1)+convert.getBit(fault,2);
	tmpVal=(tmpVal << 1)+convert.getBit(fault,1);
	tmpVal=(tmpVal << 1)+convert.getBit(fault,0);
	convert.itox(tmpVal,z,1);
	strcat(destination,z);
	strcat(destination,"\n");
	strcat(destination,"TEST FAILED: ");
	maskbit = convert.getBit(status, UDS_DTC_STATUS_TEST_FAILED_BIT);
	if(maskbit == 0)//false
	{
		strcat(destination," NO\n");
	}
	else
	{
		strcat(destination,"YES\n");
	}
	strcat(destination,"CYCLE FAIL: ");
	maskbit = convert.getBit(status, UDS_DTC_STATUS_TEST_FAILED_THIS_OPERATION_CYCLE_BIT);
	if(maskbit == 0)//false
	{
		strcat(destination,"  NO\n");
	}
	else
	{
		strcat(destination," YES\n");
	}
	strcat(destination,"PENDING: ");
	maskbit = convert.getBit(status, UDS_DTC_STATUS_PENDING_DTC_BIT);
	if(maskbit == 0)//false
	{
		strcat(destination,"     NO\n");
	}
	else
	{
		strcat(destination,"    YES\n");
	}
	strcat(destination,"CONFIRMED: ");
	maskbit = convert.getBit(status, UDS_DTC_STATUS_CONFIRMED_DTC_BIT);
	if(maskbit == 0)//false
	{
		strcat(destination,"   NO\n");
	}
	else
	{
		strcat(destination,"  YES\n");
	}
	strcat(destination,"CMPLT CLEAR: ");
	maskbit = convert.getBit(status, UDS_DTC_STATUS_TEST_NOT_COMPLETE_SINCE_LAST_CLEAR_BIT);
	if(maskbit == 0)//false
	{
		strcat(destination,"NO\n");
	}
	else
	{
		strcat(destination,"YES\n");
	}
	strcat(destination,"FAIL CLEAR: ");
	maskbit = convert.getBit(status, UDS_DTC_STATUS_TEST_FAILED_SINCE_LAST_CLEAR_BIT);
	if(maskbit == 0)//false
	{
		strcat(destination,"  NO\n");
	}
	else
	{
		strcat(destination," YES\n");
	}
	strcat(destination,"CMPLT CYCLE: ");
	maskbit = convert.getBit(status, UDS_DTC_STATUS_TEST_NOT_COMPLETED_THIS_OPERATION_CYCLE_BIT);
	if(maskbit == 0)//false
	{
		strcat(destination," NO\n");
	}
	else
	{
		strcat(destination,"YES\n");
	}
	strcat(destination,"MIL RQST: ");
	maskbit = convert.getBit(status, UDS_DTC_STATUS_WARNING_INDICATOR_REQUESTED_BIT);
	if(maskbit == 0)//false
	{
		strcat(destination,"    NO\n");
	}
	else
	{
		strcat(destination,"   YES\n");
	}
	return;

}

void UDSCANHandler::checkError(uint8_t errorCode, uint8_t *destination)//to have some verbosity on the error codes
{
switch (errorCode)
    {
		case UDS_SUBFUNCTION_NOT_SUPPORTED:
        {
        		sprintf((char *)destination,"Subfunction not supported");
        		break;
        }
		case UDS_RESPONSE_TOO_LONG:
        {
        		sprintf((char *)destination,"Response too long");
        		break;
        }
        case UDS_REQUEST_SEQUENCE_ERROR:
        {
        		sprintf((char *)destination,"Request Sequence error");
        		break;
        }
        case UDS_NO_RESPONSE_FROM_SUBNET_COMPONENT:
        {
        		sprintf((char *)destination,"NR from sub-net component");
        		break;
        }
        case UDS_FAILURE_PREVENTS_EXECUTION_OF_REQUESTED_ACTION:
        {
        		sprintf((char *)destination,"Failure prevents execution of requested action");
        		break;
        }
        case UDS_UPLOAD_DOWNLOAD_NOT_ACCEPTED:
        {
        		sprintf((char *)destination,"Upload/Download not accepted");
        		break;
        }
        case UDS_WRONG_BLOCK_SEQUENCE_COUNTER:
        {
        		sprintf((char *)destination,"Wrong Block Sequence counter");
        		break;
        }
        case UDS_SUBFUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION:
        {
        		sprintf((char *)destination,"Sub-function not supported in active session");
        		break;
        }
        case UDS_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION:
        {
        		sprintf((char *)destination,"Service not supported in active session");
        		break;
        }
    	case UDS_SERVICE_NOT_SUPPORTED:
        {
        	sprintf((char *)destination,"Service not supported ");
            break;
        }
        case UDS_INCORRECT_MESSAGE_LENGTH_OR_INVALIDAD_FORMAT:
        {
        	sprintf((char *)destination,"Incorrect message length or Invalid Format");
            break;
        }
        case UDS_CONDITIONS_NOT_CORRECT:
        {
        	sprintf((char *)destination,"Conditions not correct");
            break;
        }
        case UDS_REQUEST_OUT_OF_RANGE:
        {
        	sprintf((char *)destination,"Request out of range");
            break;
        }
        case UDS_INVALID_KEY:
        {
        	sprintf((char *)destination,"Invalid Key");
            break;
        }
        case UDS_EXCEEDED_NUMBER_OF_ATTEMPTS:
        {
        	sprintf((char *)destination,"Exceeded number of attempts");
            break;
        }
        case UDS_REQUIRED_TIME_DELAY_NOT_EXPIRED:
        {
        	sprintf((char *)destination,"Time delay not expired");
            break;
        }
        case UDS_SECURITY_ACCESS_DENIED:
        {
        	sprintf((char *)destination,"Security Access required");
            break;
        }
        case UDS_BUSY_REPEAT_REQUEST:
        {
        	sprintf((char *)destination,"Busy, repeat request");
            break;
        }
        case UDS_ROUTINE_NOT_COMPLETE:
        {
        	sprintf((char *)destination,"Routine not complete");
            break;
        }
        case UDS_GENERAL_REJECT:
        {
        	sprintf((char *)destination,"General Error");
            break;
        }
        case UDS_DOWNLOAD_NOT_ACCEPTED:
        {
        	sprintf((char *)destination,"Download not Accepted");
            break;
        }
        case UDS_IMPROPER_DOWNLOAD_TIME:
        {
        	sprintf((char *)destination,"Improper download time");
            break;
        }
        case UDS_CANT_DOWNLOAD_TO_SPECIFIED_ADDRESS:
        {
        	sprintf((char *)destination,"Can't Download to specified address");
            break;
        }
        case UDS_CANT_DOWNLOAD_NUMBER_OF_BYTES_REQUESTED:
        {
        	sprintf((char *)destination,"Can't Download number of bytes requested");
            break;
        }
        case UDS_UPLOAD_NOT_ACCEPTED:
        {
        	sprintf((char *)destination,"Upload not accepted");
            break;
        }
        case UDS_IMPROPER_UPLOAD_TYPE:
        {
        	sprintf((char *)destination,"Improper Upload type");
            break;
        }
        case UDS_CANT_UPLOAD_FROM_SPECIFIED_ADDRESS:
        {
        	sprintf((char *)destination,"Can't Upload from specified address");
            break;
        }
        case UDS_CANT_UPLOAD_NUMBER_OF_BYTES_REQUESTED:
        {
        	sprintf((char *)destination,"Can't Upload number of bytes requested");
            break;
        }
        case UDS_TRANSFER_DATA_SUSPENDED:
        {
        	sprintf((char *)destination,"Transfer suspended");
            break;
        }
        case UDS_GENERAL_PROGRAMMING_FAILURE:
        {
        	sprintf((char *)destination,"General Programming failure");
            break;
        }
        case UDS_ILLEGAL_ADDRESS_IN_BLOCK_TRANSFER:
        {
        	sprintf((char *)destination,"Illegal address in Block Transfer");
            break;
        }
        case UDS_ILLEGAL_BYTE_COUNT_IN_BLOCK_TRANSFER:
        {
        	sprintf((char *)destination,"Illegal byte count in block transfer");
            break;
        }
        case UDS_ILLEGAL_BLOCK_TRANSFER_TYPE:
        {
        	sprintf((char *)destination,"Illegal Block Transfer type");
            break;
        }
        case UDS_BLOCK_TRANSFER_DATA_CHECKSUM_ERROR:
        {
        	sprintf((char *)destination,"Block Transfer data checksumm error");
            break;
        }
        case UDS_INCORRECT_BYTE_COUNT_DURING_BLOCK_TRANSFER:
        {
        	sprintf((char *)destination,"Incorrect byte count during block transfer");
            break;
        }
        case UDS_SERVICE_NOT_SUPPORTED_IN_ACTIVE_DIAGNOSTIC_MODE:
        {
        	sprintf((char *)destination,"Service not supported in active Diagnostics Mode");
            break;
        }
        case UDS_RESPONSE_PENDING:
        {
        	sprintf((char *)destination,"Response pending");
            break;
        }
        case UDS_RPM_TOO_HIGH:
        {
        	sprintf((char *)destination,"RPM too high");
            break;
        }
        case UDS_RPM_TOO_LOW:
        {
        	sprintf((char *)destination,"RPM too low");
            break;
        }
        case UDS_ENGINE_IS_RUNNING:
        {
        	sprintf((char *)destination,"Engine is running");
            break;
        }
        case UDS_ENGINE_IS_NOT_RUNNING:
        {
        	sprintf((char *)destination,"Engine is not running");
            break;
        }
        case UDS_ENGINE_RUN_TIME_TOO_LOW:
        {
        	sprintf((char *)destination,"Engine run time too low");
            break;
        }
        case UDS_TEMPERATURE_TOO_HIGH:
        {
        	sprintf((char *)destination,"Temperature too high");
            break;
        }
        case UDS_TEMPERATURE_TOO_LOW:
        {
        	sprintf((char *)destination,"Temperature too low");
            break;
        }
        case UDS_VEHICLE_SPEED_TOO_HIGH:
        {
        	sprintf((char *)destination,"Vehicle speed too high");
            break;
        }
        case UDS_VEHICLE_SPEED_TOO_LOW:
        {
        	sprintf((char *)destination,"Vehicle speed too low");
            break;
        }
        case UDS_THROTTLE_TOO_HIGH:
        {
        	sprintf((char *)destination,"Throttle too high");
            break;
        }
        case UDS_THROTTLE_TOO_LOW:
        {
        	sprintf((char *)destination,"Throttle speed too low");
            break;
        }
        case UDS_TRANSMISSION_RANGE_NOT_IN_NEUTRAL:
        {
        	sprintf((char *)destination,"Transmission not in neutral");
            break;
        }
        case UDS_TRANSMISSION_RANGE_NOT_IN_GEAR:
        {
        	sprintf((char *)destination,"Transmission not in gear");
            break;
        }
        case UDS_BRAKE_SWITCH_NOT_CLOSED:
        {
        	sprintf((char *)destination,"Brake switch not closed");
            break;
        }
        case UDS_SHIFTER_LEVER_NOT_IN_PARK:
        {
        	sprintf((char *)destination,"Shifter level not in park");
            break;
        }
        case UDS_TORQUE_CONVERTER_CLUTCH_LOCKED:
        {
        	sprintf((char *)destination,"Torque Converter clutch locked");
            break;
        }
        case UDS_VOLTAGE_TOO_HIGH:
        {
        	sprintf((char *)destination,"Voltage too high");
            break;
        }
        case UDS_VOLTAGE_TOO_LOW:
        {
        	sprintf((char *)destination,"Voltage too low");
            break;
        }
        default:
        {
        	sprintf((char *)destination,"Unknown error: 0x%x ",errorCode);
            break;
        }
    }
}

void UDSCANHandler::setSessionStatus(bool setSession)
{
	if(setSession == true && inSession == 0)
	{
		inSession = 1;
		tick.attach(this,&UDSCANHandler::sendTesterPresent, 0.5);
	}
	else if(setSession == false && inSession == 1)
	{
		inSession=0;
		tick.detach();
	}
	else
	{
		return;
	}
	return;
}

uint32_t UDSCANHandler::clearDTCs(uint32_t groupOfDTC)
{
	uint8_t request[32]={UDS_CLEAR_DIAGNOSTIC_INFORMATION, (uint8_t)(groupOfDTC >> 16),(uint8_t)(groupOfDTC >> 8),(uint8_t)groupOfDTC};
	return requestResponseClient(request, 4, request);
}


uint32_t UDSCANHandler::readDTCs(uint8_t *reply, uint8_t dtcMask)
{
/*First two bytes of reply will be a mirror of the request
 * Third byte is a mask of supported DTCs status
 * After that, it has the following format for all DTCs, concatenated:
 * -First byte is DTC high byte
 * -Second byte is DTC middle byte
 * -Third byte is DTC low byte
 * -Fourth byte is Status of DTC
 */

	uint8_t request[3]={UDS_READ_DTC_INFORMATION, UDS_REPORT_DTC_BY_STATUS_MASK, dtcMask};
	return requestResponseClient(request, 3, reply);
}

uint32_t UDSCANHandler::readNumberOfDTCs(uint8_t *reply, uint8_t dtcMask)//number of DTCs will be in bytes 5 and 6 starting from 1
{
	uint8_t request[3]={UDS_READ_DTC_INFORMATION, UDS_REPORT_NUMBER_OF_DTC_BY_STATUS_MASK, dtcMask};
	return requestResponseClient(request, 3, reply);
}

uint32_t UDSCANHandler::sendKey(uint8_t level, uint8_t *keyBytes, uint32_t keyLength, uint8_t *response)
{
	if(keyLength > 4094)//make sure we dont write OOB
	{
		return 0;
	}
	for (uint32_t a = (keyLength + 1); a > 1; a--)
	{
		keyBytes[a] = keyBytes[(a - 2)];
	}
	keyBytes[0]=UDS_SECURITY_ACCESS;
	keyBytes[1]=(level + 1);
	return requestResponseClient(keyBytes, (keyLength + 2), response);
}


uint32_t UDSCANHandler::writeMemoryByAddress(uint64_t memAddress, uint32_t memSize, uint8_t *request)
{
		uint8_t request2[3000]={UDS_WRITE_MEMORY_BY_ADDRESS};
		uint8_t addrAndFormatID=1;
		if(memAddress > 0xFF && memAddress < 0x10000)//calculate the number of memory address bytes
		{
			addrAndFormatID=2;
		}
		else if(memAddress > 0xFFFF && memAddress < 0x1000000)
		{
			addrAndFormatID=3;
		}
		else if(memAddress > 0xFFFFFF && memAddress < 0x100000000)
		{
			addrAndFormatID=4;
		}
		else if(memAddress > 0xFFFFFFFF && memAddress < 0x10000000000)
		{
			addrAndFormatID=5;
		}
		else if(memAddress > 0xFFFFFFFFFF && memAddress < 0x1000000000000)
		{
			addrAndFormatID=6;
		}
		else if(memAddress > 0xFFFFFFFFFFFF && memAddress < 0x100000000000000)
		{
			addrAndFormatID=7;
		}
		else if(memAddress > 0xFFFFFFFFFFFFFF)
		{
			addrAndFormatID=8;
		}

		if(memSize < 0x100)//calculate the number of memory size bytes
		{
			addrAndFormatID= (addrAndFormatID + 0x10);
		}
		else if(memSize > 0xFF && memSize < 0x10000)
		{
			addrAndFormatID= (addrAndFormatID + 0x20);
		}
		else if(memSize > 0xFFFF && memSize < 0x1000000)
		{
			addrAndFormatID= (addrAndFormatID + 0x30);
		}		
		else
		{
			addrAndFormatID= (addrAndFormatID + 0x40);
		}	
		request2[1]=addrAndFormatID;
		uint8_t a =(addrAndFormatID & 0x0F);//get the number of memory address bytes
		uint8_t b =(addrAndFormatID >> 4) & 0x0F;//get the number of memsize bytes
		uint64_t tmpAddr=memAddress; //FFCAA0
		uint32_t tmpmemSize=memSize; //500
		for (uint8_t c = a; c > 0; c--)//place the memory address in the request
		{
			request2[(c+1)]=(uint8_t)(tmpAddr & 0xFF);
			tmpAddr = (tmpAddr >> 8);
		}
		for (uint8_t c = b; c > 0; c--)//place the memory size in the request
		{
			request2[(a+c+1)]=(uint8_t)(tmpmemSize & 0xFF);
			tmpmemSize = (tmpmemSize >> 8);
		}		
		for(uint16_t c = (a+b+2); c < ((a+b+2) + memSize); c++)
		{
			request2[c] = request[(c - (a+b+2))];
		}	
		return requestResponseClient(request2, (a+b+2+memSize), request);		
	
	
}

uint32_t UDSCANHandler::readMemoryByAddress(uint64_t memAddress, uint32_t memSize, uint8_t *response)//highly recommended to not read more than 4095 as it is extremely likely server wont do it
{
	uint8_t request[16]={UDS_READ_MEMORY_BY_ADDRESS};
	uint8_t addrAndFormatID=1;
	if(memAddress > 0xFF && memAddress < 0x10000)//calculate the number of memory address bytes
	{
		addrAndFormatID=2;
	}
	else if(memAddress > 0xFFFF && memAddress < 0x1000000)
	{
		addrAndFormatID=3;
	}
	else if(memAddress > 0xFFFFFF && memAddress < 0x100000000)
	{
		addrAndFormatID=4;
	}
	else if(memAddress > 0xFFFFFFFF && memAddress < 0x10000000000)
	{
		addrAndFormatID=5;
	}
	else if(memAddress > 0xFFFFFFFFFF && memAddress < 0x1000000000000)
	{
		addrAndFormatID=6;
	}
	else if(memAddress > 0xFFFFFFFFFFFF && memAddress < 0x100000000000000)
	{
		addrAndFormatID=7;
	}
	else if(memAddress > 0xFFFFFFFFFFFFFF)
	{
		addrAndFormatID=8;
	}
	if(memSize < 0x100)//calculate the number of memory size bytes
	{
		addrAndFormatID= (addrAndFormatID + 0x10);
	}
	else if(memSize > 0xFF && memSize < 0x10000)
	{
		addrAndFormatID= (addrAndFormatID + 0x20);
	}
	else if(memSize > 0xFFFF && memSize < 0x1000000)
	{
		addrAndFormatID= (addrAndFormatID + 0x30);
	}
	else
	{
		addrAndFormatID= (addrAndFormatID + 0x40);
	}
	request[1]=addrAndFormatID;
	uint8_t a =(addrAndFormatID & 0x0F);//get the number of memory address bytes
	uint8_t b =(addrAndFormatID >> 4) & 0x0F;//get the number of memsize bytes
	uint64_t tmpAddr=memAddress;
	uint32_t tmpmemSize=memSize;
	for (uint8_t c = a; c > 0; c--)//place the memory address in the request
	{
		request[(c+1)]=(uint8_t)(tmpAddr & 0xFF);
		tmpAddr = (tmpAddr >> 8);
	}
	for (uint8_t c = b; c > 0; c--)//place the memory size in the request
	{
		request[(a+c+1)]=(uint8_t)(tmpmemSize & 0xFF);
		tmpmemSize = (tmpmemSize >> 8);
	}
	return requestResponseClient(request, (a+b+2), response);
}


bool UDSCANHandler::sessionStatus()
{
	return inSession;
}

void UDSCANHandler::endSession()
{
	if(inSession==1)
	{
		tick.detach();
		inSession=0;
	}
}
	


uint32_t UDSCANHandler::requestResponseClient(uint8_t *rqst, uint16_t len, uint8_t *response, bool ignoreACK)
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
		tick.attach(this,&UDSCANHandler::sendTesterPresent, 0.5);
	}
	return readOp;
}

void UDSCANHandler::setTransmissionParameters(uint32_t sourceID, uint32_t targetID, CANFormat doFrameFormat, bool doUseFullFrame, uint8_t doBsByte, uint8_t doVariant, bool useFilters)
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

uint32_t UDSCANHandler::read(uint8_t *response, bool ignoreACK)
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

bool UDSCANHandler::write(uint8_t *request, uint16_t len)
{
	return tp->write(request,len);
}
