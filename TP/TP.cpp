/*
* ISO-TP (ISO 15765) LIBRARY
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




//work on filter logic for uds and the other diag protocols. probably add a function to add or remove filters inside tp and tp20 libs so they can be called by handlers

#include "mbed.h"
#include "tp.h"

TPHandler::TPHandler(CAN *canbus)
{
	_canbus=canbus;
	_cb = new CANbadger_CAN(_canbus);
	useFullFrame = 1; 
	bsByte = 0x00;
	variant = 0;
	ownID=0;
	rID=0;
	frameFormat=CANStandard;
	requestTimeout=TP_DEFAULT_REQUEST_TIMEOUT;
	responseTimeout=TP_DEFAULT_RESPONSE_TIMEOUT;
	areFiltersActive=false;
}

TPHandler::~TPHandler()
{
	if(areFiltersActive == true)
	{
		disableFilters();
	}
	delete _cb;
}
	

void TPHandler::resumeSession(uint32_t localID, uint32_t remoteID)
{
	if(areFiltersActive == true)
	{
		disableFilters();
	}
	ownID = localID;
	rID = remoteID;
	if(areFiltersActive == false)
	{
		enableFilters();
	}
}


void TPHandler::disableFilters()
{
	if(frameFormat == CANExtended)//adjust IDS for extended filtering
	{
		rID = (rID + 0x80000000);
		ownID = (ownID + 0x80000000);
	}
	_cb->CANx_ObjectSetFilter(ARM_CAN_FILTER_ID_EXACT_REMOVE, rID, rID, 1);//clean up
	_cb->CANx_ObjectSetFilter(ARM_CAN_FILTER_ID_EXACT_REMOVE, rID, rID, 2);
	_cb->CANx_ObjectSetFilter(ARM_CAN_FILTER_ID_EXACT_REMOVE, ownID, ownID, 1);
	_cb->CANx_ObjectSetFilter(ARM_CAN_FILTER_ID_EXACT_REMOVE, ownID, ownID, 2);
	_cb->disableCANFilters();
	areFiltersActive=false;
	if(frameFormat == CANExtended)//restore IDS
	{
		rID = (rID - 0x80000000);
		ownID = (ownID - 0x80000000);
	}
}

void TPHandler::enableFilters()
{
	if(frameFormat == CANExtended)//adjust IDS for extended filtering
	{
		rID = (rID + 0x80000000);
		ownID = (ownID + 0x80000000);
	}
	_cb->CANx_ObjectSetFilter(ARM_CAN_FILTER_ID_EXACT_ADD, rID, rID, 1);//enable filtering for ease of traffic handling
	_cb->CANx_ObjectSetFilter(ARM_CAN_FILTER_ID_EXACT_ADD, rID, rID, 2);
	_cb->CANx_ObjectSetFilter(ARM_CAN_FILTER_ID_EXACT_ADD, ownID, ownID, 1);
	_cb->CANx_ObjectSetFilter(ARM_CAN_FILTER_ID_EXACT_ADD, ownID, ownID, 2);
	_cb->enableCANFilters();
	areFiltersActive=true;
	if(frameFormat == CANExtended)//restore IDS
	{
		rID = (rID - 0x80000000);
		ownID = (ownID - 0x80000000);
	}
}

void TPHandler::setTimeouts(uint32_t request, uint32_t response)
{
	requestTimeout = request;
	responseTimeout = response;	
}


	

void TPHandler::setTransmissionParameters(uint32_t sourceID, uint32_t targetID, CANFormat doFrameFormat, bool doUseFullFrame, uint8_t doBsByte, uint8_t doVariant, bool useFilters)
{
	if(areFiltersActive == true && useFilters == true)
	{
		disableFilters();//disable previous ones before enabling the new ones
		areFiltersActive = false;
	}
	ownID=sourceID;
	rID=targetID;
	frameFormat=doFrameFormat;
	useFullFrame=doUseFullFrame;
	bsByte=doBsByte;
	variant=doVariant;
	if(areFiltersActive == false && useFilters == true)
	{
		enableFilters();//disable previous ones before enabling the new ones
	}
	else if(areFiltersActive == true && useFilters == false)
	{
		disableFilters();//disable previous ones before enabling the new ones
	}
}

uint32_t TPHandler::read(uint8_t *response)
{
	   uint32_t ln=0x1;//for storing the payload length
	   uint32_t cnt2=0; //to know how much data is left
	   uint8_t skipTester=0;//used to indicate if the first byte should be skipped because extended adressing
	   uint8_t payload[8];//to store the can frame payload
	   bool wasFrameSent=false;//used to know if a frame was sent
	   while(ln>cnt2)//1 second timeout and wait until data is received should be standard
	   {
		   	uint8_t frameLn=_cb->getCANFrame(rID, payload, responseTimeout);
		   	if(frameLn == 0)
			{
		   		return 0;
			}
			if(variant == 1)//if we are using extended addressing
			{
				skipTester=1;
			}
			if((payload[(0 + skipTester)] & 0xF0) == 0)//Single frame event
			{
				if(variant == 1 && payload[0] != (ownID & 0xFF))//we check for correct address
				{
					return 0;
				}
				for(uint8_t a=0;a<payload[(0 + skipTester)];a++)//this returns the payload
				{
					response[a]=payload[(a + (1 + skipTester))];
				}
				return (payload[(0 + skipTester)]);
			}
			else if((payload[(0 + skipTester)] & 0xF0) == 0x10 && frameLn == 8)//Start of multiframe with ACK request. I like turtles btw.
			{
				if(variant == 1 && payload[0] != (ownID & 0xFF))//we check for correct address
				{
					return 0;
				}
				uint8_t tmp[8]={0};//we send an ack
				if(useFullFrame)
				{
					memset(tmp,bsByte,8);
				}
				ln= (payload[(0 + skipTester)] & 0x0F);//we grab the transmission length
				ln=(ln << 8);//we grab the length
				ln=(ln + (payload[(1 + skipTester)]));
				for(uint8_t a=(2  + skipTester);a<8;a++)//grab the first part of the response
				{
					response[cnt2]=payload[a];
					cnt2++;
				}
				if(variant == 1)//if using mode 2, we add the target address on first byte
				{
					 tmp[0]=(rID &0xFF);
					 tmp[1]=0x30; //flow status = Continue to send
					 tmp[2]=0x00; //block size = unlimited
					 tmp[3]=0x00; //separation time = 0 (none)
				}
				else
				{
					 tmp[0]=0x30;
					 tmp[1]=0x00;
					 tmp[2]=0x00;
				}
				if(useFullFrame)
				{
					wasFrameSent = _cb->sendCANFrame(ownID, tmp, 8, frameFormat, CANData, requestTimeout);
				}
				else
				{
					wasFrameSent = _cb->sendCANFrame(ownID, tmp, (3 + skipTester), frameFormat, CANData, requestTimeout);
				}
				if(wasFrameSent == false)
				{
					return 0;
				}
			}
			else if ((payload[(0 + skipTester)] & 0xF0) == 0x20)//Continuation of multiframe
			{
				if((ln-cnt2) > (6 - skipTester))
				{
					 for(uint8_t a=(1  + skipTester);a<8;a++)
					 {
							 response[cnt2]=payload[a];
							 cnt2++;
					 }
				}
				else
				{
					uint8_t tmp=(ln-cnt2);
					for(uint8_t a=(1 + skipTester);a<(tmp+1+skipTester);a++)
					{
						 response[cnt2]=payload[a];
						 cnt2++;
					}
					return ln;
				}
			}
			else
			{
					return 0; //not a valid TP frame
			}
	}
	return ln;
}

bool TPHandler::write(uint8_t *request, uint16_t len)
{

	if(len > 0xFFF || (frameFormat == CANStandard && ownID > 0x7FF))//limit for TP transmission length and prevent to attempt to send an extended ID with Standard format
	{
		return false;
	}
	uint8_t data[8]={0};//array containing data to be sent in frames
	uint32_t cnt=0;//counter for array
	uint32_t TPframeCounter=0;//counter for frames
	uint8_t skipTester=0;
	bool wasFrameSent=false;//used to know if a frame was sent
	if(variant == 1)
	{
		skipTester=1;
	}
	if((len < 8 && variant != 1) || (len < 7 && variant == 1))//Case for single frame. Variant 1 requires one extra byte
	{
			if(useFullFrame == 1)
			{
				memset(data,bsByte,8);//if we are going to use a full frame, lets prepare the payload
			}

			if(variant == 1)
			{
				data[0]=(rID & 0xFF);//ad the remote address on the first byte
				data[1]=len;//add the length
				for (uint8_t cnt2=0;cnt2<len;cnt2++)//then grab the rest of the payload straight away
				{
					data[(cnt2+2)]=request[cnt2];
				}		
			}
			else//default case when variant == 0
			{
				data[0]=len;//on single frame, the length is stored on the first byte
				for (uint8_t cnt2=0;cnt2<len;cnt2++)//then grab the rest of the payload straight away
				{
					data[(cnt2+1)]=request[cnt2];
				}
			}
			if(useFullFrame)
			{
				wasFrameSent = _cb->sendCANFrame(ownID, data, 8, frameFormat, CANData, requestTimeout);
			}
			else
			{
				wasFrameSent = _cb->sendCANFrame(ownID, data, (len + 1 + skipTester), frameFormat, CANData, requestTimeout);
			}
			if(wasFrameSent == false)
			{
				return 0;
			}
			return 1;
	}
	else//if payload len requires multiframe
	{
		if(variant != 1)
		{
			data[0]=(0x10 + ((len >> 8) & 0x0F));//first byte is always 10 for multiframe, which is also an ACK request plus the upper 4 bits of length

			data[1]=(len & 0xFF);//second byte should be the lower byte of payload length
			for (cnt=0;cnt<6;cnt++)
			{
				data[(cnt+2)]=request[cnt];
			}
		}
		else
		{
				data[0]=(rID & 0xFF);
				data[1]=(0x10 + ((len >> 8) & 0x0F));//second byte is always 10 for multiframe, which is also an ACK request
				data[2]=(len & 0xFF);
				for (cnt=0;cnt<5;cnt++)
				{
					data[(cnt+3)]=request[cnt];
				}
		}
		TPframeCounter++;//increase the frame counter
		wasFrameSent = _cb->sendCANFrame(ownID, data, 8, frameFormat, CANData, requestTimeout);
		if(wasFrameSent == false)
		{
			return 0;
		}
		uint8_t flowStatus = TP_WAIT;//lets set it so the cycle runs once at least
		uint8_t blockSize = 0;
		uint8_t separationTime = 0;
		while(flowStatus == TP_WAIT)
		{
		   	if(!_cb->getCANFrame(rID, data, responseTimeout))
			{
		   		return 0;
			}
			if((data[(0 + skipTester)] & 0xF0) != 0x30)//if its not an ACK or timeout
			{
				return 0;
			}
			flowStatus = (data[(0 + skipTester)] & 0x0F);//grab the parameters
			blockSize = data[(1 + skipTester)];
			separationTime = data[(2 + skipTester)];
		}
		uint16_t blockCounter=(6 - skipTester);//if block size is not unlimited, then we will have to use this. We already sent a few bytes, so we initialize it
		while (cnt<len)
		{
			memset(data,0,8);//clear the frame
			uint8_t remainingLen=0;
			bool getACK=false;//to know if we triggered the block size
			if(variant != 1)//standard ID (ISO-TP wise, not CAN)
			{
				data[0]=(0x20 + TPframeCounter);
				if ((len-cnt)>6)//if its the full payload, just parse the whole thing
				{
					for(uint8_t a=1;a<8;a++)
					{
						data[a]=request[cnt];
						cnt++;
						remainingLen++;
						if(blockSize != TP_BLOCK_SIZE_UNLIMITED)
						{
							blockCounter++;
							if(blockCounter == blockSize)//if we hit the block size
							{
								blockCounter=0;
								getACK=true;
								break;
							}
						}
					}
				}
				else
				{
					remainingLen=len-cnt;
					for(uint8_t a=0;a<remainingLen;a++)
					{
						data[a+1]=request[cnt];
						cnt++;
						if(blockSize != TP_BLOCK_SIZE_UNLIMITED)
						{
							blockCounter++;
							if(blockCounter == blockSize)//if we hit the block size
							{
								blockCounter=0;
								getACK=true;
								break;
							}
						}
					}
				}
			}
			else//extended ID (ISO-TP wise, not CAN)
			{
				if ((len-cnt)>5)//if its the full payload, just parse the whole thing
				{
					for(uint8_t a=2;a<8;a++)
					{
						data[a]=request[cnt];
						cnt++;
						remainingLen++;
						if(blockSize != TP_BLOCK_SIZE_UNLIMITED)
						{
							blockCounter++;
							if(blockCounter == blockSize)//if we hit the block size
							{
								blockCounter=0;
								getACK=true;
								break;
							}
						}
					}
				}
				else
				{
					if(useFullFrame == 1)
					{
						memset(data,bsByte,8);//if we are going to use a full frame, lets prepare the payload
					}
					remainingLen=len-cnt;
					for(uint8_t a=0;a<remainingLen;a++)
					{
						data[a+2]=request[cnt];
						cnt++;
						if(blockSize != TP_BLOCK_SIZE_UNLIMITED)
						{
							blockCounter++;
							if(blockCounter == blockSize)//if we hit the block size
							{
								blockCounter=0;
								getACK=true;
								break;
							}
						}
					}
				}
				remainingLen++;
				data[0] = (rID & 0xFF);
				data[1]=(0x20 + TPframeCounter);
			}
			if(useFullFrame)
			{
				wasFrameSent = _cb->sendCANFrame(ownID, data, 8, frameFormat, CANData, requestTimeout);
			}
			else
			{
				wasFrameSent = _cb->sendCANFrame(ownID, data, (remainingLen + 1), frameFormat, CANData, requestTimeout);
			}
			if(wasFrameSent == false)
			{
				return 0;
			}
			if(getACK == true)//if we expect an ACK
			{
				flowStatus = TP_WAIT;
				while(flowStatus == TP_WAIT)
				{
				   	if(!_cb->getCANFrame(rID, data, responseTimeout))
					{
				   		return 0;
					}
					if((data[(0 + skipTester)] & 0xF0) != 0x30)//if its not an ACK or timeout
					{
						return 0;
					}
					flowStatus = (data[(0 + skipTester)] & 0x0F);//grab the parameters
					blockSize = data[(1 + skipTester)];
					separationTime = data[(2 + skipTester)];
				}
			}
			TPframeCounter++;
			if(TPframeCounter>0x0F)//don't forget to bring a towel! (and reset the counter too)
			{
				TPframeCounter=0;
			}
			if(separationTime != TP_NO_WAIT_TIME)//if there is a separation time
			{
				wait(getSeparationTime(separationTime));
			}
		}
	}
	return 1;	
}


float TPHandler::getSeparationTime(uint8_t value)
{
	if(value < 80)//absolute value, milliseconds
	{
		return ((float)value / 1000);
	}
	else if(value > 0xF0 && value < 0xFA)
	{
		return ((float)(value & 0x0F) / 10000);
	}
	else
	{
		return 0;
	}
}


uint32_t TPHandler::requestResponseClient(uint8_t *rqst, uint16_t len, uint8_t *response, bool ignoreACK)
{
	if(!write(rqst,len))
	{
		return 0;
	}
	uint32_t readOp = read(response);
	return readOp;
}
