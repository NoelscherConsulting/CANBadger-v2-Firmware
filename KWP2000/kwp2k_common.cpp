/*
* KWP2000 Common procedures LIBRARY USING TP
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

#include "kwp2k_common.h"






KWP2KCommon::KWP2KCommon(){}
KWP2KCommon::~KWP2KCommon(){}


void KWP2KCommon::checkDTC(uint16_t DTCCode, char *destination)
{
	Conversions convert;
	char z[22];
	uint32_t fault=DTCCode;
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
	return;
}


void KWP2KCommon::checkError(uint8_t errorCode, uint8_t *destination)//to have some verbosity on the error codes
{
	switch (errorCode)
    {
		case KWP_GENERAL_REJECT:
        {
        		sprintf((char *)destination,"General Reject");
        		break;
        }
		case KWP_SERVICE_NOT_SUPPORTED:
        {
        		sprintf((char *)destination,"Service not supported");
        		break;
        }
        case KWP_SUBFUNCTION_NOT_SUPPORTED:
        {
        		sprintf((char *)destination,"Subfunction not supported");
        		break;
        }
        case KWP_BUSY_REPEAT_REQUEST:
        {
        		sprintf((char *)destination,"Busy, repeat request");
        		break;
        }
        case KWP_CONDITIONS_NOT_CORRECT:
        {
        		sprintf((char *)destination,"Conditions not correct");
        		break;
        }
        case KWP_ROUTINE_NOT_COMPLETE:
        {
        		sprintf((char *)destination,"Routine not complete");
        		break;
        }
        case KWP_REQUEST_OUT_OF_RANGE:
        {
        		sprintf((char *)destination,"Request out of range");
        		break;
        }
        case KWP_SECURITY_ACCESS_DENIED:
        {
        		sprintf((char *)destination,"Security Access denied");
        		break;
        }
        case KWP_INVALID_KEY:
        {
        		sprintf((char *)destination,"Invalid Key");
        		break;
        }
    	case KWP_EXCEEDED_NUMBER_OF_ATTEMPTS:
        {
        	sprintf((char *)destination,"Exceeded number of attempts");
            break;
        }
        case KWP_REQUIRED_TIME_DELAY_NOT_EXPIRED:
        {
        	sprintf((char *)destination,"Required time delay not expired");
            break;
        }
        case KWP_DOWNLOAD_NOT_ACCEPTED:
        {
        	sprintf((char *)destination,"Download not accepted");
            break;
        }
        case KWP_IMPROPER_DOWNLOAD_TYPE:
        {
        	sprintf((char *)destination,"Improper download type");
            break;
        }
        case KWP_CANNOT_DOWNLOAD_TO_SPECIFIED_ADDRESS:
        {
        	sprintf((char *)destination,"Cannot download to specified address");
            break;
        }
        case KWP_CANNOT_DOWNLOAD_NUMBER_OF_BYTES_REQUESTED:
        {
        	sprintf((char *)destination,"Cannot download number of bytes requested");
            break;
        }
        case KWP_UPLOAD_NOT_ACCEPTED:
        {
        	sprintf((char *)destination,"Upload not accepted");
            break;
        }
        case KWP_IMPROPER_UPLOAD_TYPE:
        {
        	sprintf((char *)destination,"Improper upload type");
            break;
        }
        case KWP_CANNOT_UPLOAD_FROM_SPECIFIED_ADDRESS:
        {
        	sprintf((char *)destination,"Cannot upload from specified address");
            break;
        }
        case KWP_CANNOT_UPLOAD_NUMBER_OF_BYTES_REQUESTED:
        {
        	sprintf((char *)destination,"Cannot upload number of bytes requested");
            break;
        }
        case KWP_TRANSFER_SUSPENDED:
        {
        	sprintf((char *)destination,"Transfer suspended");
            break;
        }
        case KWP_TRANSFER_ABORTED:
        {
        	sprintf((char *)destination,"Transfer aborted");
            break;
        }
        case KWP_ILLEGAL_ADDRESS_IN_BLOCK_TRANSFER:
        {
        	sprintf((char *)destination,"Illegal address in block transfer");
            break;
        }
        case KWP_ILLEGAL_BYTE_COUNT_IN_BLOCK_TRANSFER:
        {
        	sprintf((char *)destination,"Illegal byte count in block transfer");
            break;
        }
        case KWP_ILLEGAL_BLOCK_TRANSFER_TYPE:
        {
        	sprintf((char *)destination,"Illegal block transfer type");
            break;
        }
        case KWP_BLOCK_TRANSFER_DATA_CHECKSUM_ERROR:
        {
        	sprintf((char *)destination,"Block transfer data checksum error");
            break;
        }
        case KWP_INCORRECT_BYTE_COUNT_DURING_TRANSFER:
        {
        	sprintf((char *)destination,"Incorrect byte count during transfer");
            break;
        }
        case KWP_SERVICE_NOT_SUPPORTED_IN_ACTIVE_DIAG_MODE:
        {
        	sprintf((char *)destination,"Service not supported in active diag mode");
            break;
        }
        default:
        {
        	sprintf((char *)destination,"Unknown error: 0x%x ",errorCode);
            break;
        }
    }
}
