/*
* TP 2.0 (SAE J2819-2008) LIBRARY
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


//This library WILL require you to use filters due to the fact that it uses RX interrupt, otherwise it wont work at all if theres any other traffic.
#include "mbed.h"
#include "tp20.h"

/***Timer for session timeout***/
Timer TPtimer;


TP20Handler::TP20Handler(CAN *canbus, uint8_t interface)
{
	_canbus=canbus;
	_cb = new CANbadger_CAN(_canbus);
	ownID=0x200;//standard for TP2.0 channel negotiation
	rID=0;
	requestTimeout=TP20_DEFAULT_REQUEST_TIMEOUT;
	responseTimeout=TP20_DEFAULT_RESPONSE_TIMEOUT;
	frameCount=0;
	inSession=0;
	areFiltersActive=false;
	needFilters=true;
	blockSize=0;
	waitTime=0;
	_interface = interface;
}

TP20Handler::~TP20Handler()
{
	if(inSession == true)
	{
		closeChannel();
	}
	delete _cb;
}


void TP20Handler::setTransmissionParameters(bool useFilters)
{
	needFilters=useFilters;
}

void TP20Handler::increaseFrameCounter()//just to keep the frame counter within range
{
    frameCount++;
    if(frameCount >0xF)
    {
        frameCount=0;
    }
}

bool TP20Handler::stillInSession()
{
	if (inSession == 1)
	{
		return true;
	}
	return false;
}

uint8_t TP20Handler::channelSetup(const uint32_t requestAddress, uint8_t target_ID, uint8_t appType) //the input here is the logical address of the module. the ECU would be 0x01, for example
{
  if(areFiltersActive == true)
  {
	  disableFilters();
  }
  uint8_t data[8]={target_ID,TP20_SETUP_CHANNEL,0x00,0x7,0x00,0x03,0x01};
  ownID=requestAddress;//should be between 0x200 and 0x2EF
  if(!_cb->sendCANFrame(ownID, data, 7, CANStandard, CANData, requestTimeout))
  {
	  return 0;
  }
  if(!_cb->getCANFrame((requestAddress + target_ID), data, responseTimeout))
  {
	  if(!_cb->sendCANFrame(ownID, data, 7, CANStandard, CANData, requestTimeout))
	  {
		  return 0;
	  }
	  if(!_cb->getCANFrame((requestAddress + target_ID), data, responseTimeout))
	  {
		  return 0;
	  }
  }
  if(data[1] != 0xD0)
  {
    return data[1];
  }
  else//we grab the IDs to be used
  {
      rID= data[3];
      rID=rID << 8;
      rID=rID + data[2];
      ownID=data[5];
      ownID=ownID << 8;
      ownID=ownID + data[4];
  }
  //now we exchange timing parameters
  uint8_t data1[8]={TP20_CONNECTION_SETUP,0xF,0xCA,0xFF,0x05,0xFF};//block size 0xF, T1 = 1 second, T3 = 500uSeconds
  wait(0.001);//throttle down a bit
  if(!_cb->sendCANFrame(ownID, data1, 6, CANStandard, CANData, requestTimeout))
  {
	  return false;
  }
  if(!_cb->getCANFrame(rID, data, responseTimeout))
  {
	  return false;
  }
  if(data[0] != 0xA1)
  {
    return false;
  }
  blockSize=data[1];
/*  if(data[2] != TP20_NO_TIMEOUT)//no session timeout
  {
	  responseTimeout=((uint32_t)(decodeTimeout(data[2]) * 1000) * 2);//we duplicate the provided one just to make sure we account for high traffic
  }
  else
  {*/
	  responseTimeout=TP20_DEFAULT_RESPONSE_TIMEOUT;
//  }
  waitTime=decodeTimeout(data[4]);//get the wait time
  inSession=1;
  frameCount=0;
  if(needFilters == true)
  {
	  enableFilters();
  }
  sendCA(1);
  return 1;
}

bool TP20Handler::closeChannel(bool silent)
{
	inSession=0;
	sendCA(0);
	if(!silent)
	{
		wait(waitTime);
		uint8_t data[8]={TP20_CONNECTION_DISCONNECT,0,0,0,0,0,0,0};
		if(!_cb->sendCANFrame(ownID, data, 1, CANStandard, CANData, requestTimeout))
		{
			if(areFiltersActive==true)
			{
				disableFilters();
			}
			return false;
		}
		if(!_cb->getCANFrame(rID, data, responseTimeout))
		{
			if(areFiltersActive==true)
			{
				disableFilters();
			}
			return false;
		}
		if (data[0] != TP20_CONNECTION_DISCONNECT)
		{
			if(areFiltersActive==true)
			{
				disableFilters();
			}
			return false;
		}
	}
    if(areFiltersActive==true)
    {
    	disableFilters();
    }
	return true;
}

void TP20Handler::resumeSession(uint32_t localID, uint32_t remoteID, uint8_t currentCount)
{
	ownID = localID;
	rID = remoteID;
	frameCount = currentCount;
	inSession=1;
	if(needFilters == true)
	{
		enableFilters();
	}
	sendCA(1);
}

uint8_t TP20Handler::getCurrentCounter()
{
	return frameCount;
}

uint8_t TP20Handler::TPGetACK()
{
	uint8_t data[8];
	while(1)
	{
		if(!_cb->getCANFrame(rID, data, responseTimeout))
		{
			checkSessionStatus();
			return 0;
		}
		if (data[0] == TP20_CONNECTION_TEST)
		{
			uint8_t data2[8]={TP20_CONNECTION_ACK,0xF,0xCA,0xFF,0x05,0xFF};
			wait(waitTime);
			if(!_cb->sendCANFrame(ownID, data2, 6, CANStandard, CANData, requestTimeout))
			{
				return false;
			}
			wait(waitTime);
			TPtimer.reset();
		}
		else if (data[0] == TP20_CONNECTION_DISCONNECT)//if we did something to upset the DUT
		{
			inSession=0;
			uint8_t data[8]={TP20_CONNECTION_DISCONNECT,0,0,0,0,0,0,0};
			if(!_cb->sendCANFrame(ownID, data, 1, CANStandard, CANData, requestTimeout))
			{
				if(areFiltersActive==true)
				{
					disableFilters();
				}
				return 0;
			}
		}
		else if((data[0] & 0xF0) == TP20_ACK)
		{
			return 1;
		}
		else if((data[0] & 0xF0) == TP20_NACK)
		{
			return 0x90;
		}
	}
}


bool TP20Handler::TPSendACK(uint8_t ackCounter)//ackType depends on if final ack or beginning ack
{
  uint8_t dataa[8]={0};
  dataa[0]=(1 + ackCounter);
  if(dataa[0]>0xF)
  {
      dataa[0]=0;
  }
  dataa[0]= (dataa[0] + TP20_ACK);
  if(!_cb->sendCANFrame(ownID, dataa, 1, CANStandard, CANData, requestTimeout))
  {
	  return false;
  }
  return true;
}

uint32_t TP20Handler::requestResponseClient(uint8_t *rqst, uint16_t len, uint8_t *response)
{
	if(inSession != 0)
	{
		sendCA(0);
	}
	if(!write(rqst,len))
	{
		return 0;
	}
	uint32_t a= read(response);
	if(inSession != 0)
	{
		sendCA(1);
	}
	return a;
}


uint32_t TP20Handler::read(uint8_t *response) 
{
	uint32_t cnt=0;
    uint8_t data[8];
    uint8_t conTestHit=0;//used to know if target should timeout
    while(1)//wait for the reply until timeout
    {
	   if(!_cb->getCANFrame(rID, data, responseTimeout))
	   {
		   return 0;
	   }
	   if (data[0] == TP20_CONNECTION_TEST)//need to expect channel test while doing stuff
	   {
			uint8_t data2[8]={TP20_CONNECTION_ACK,0xF,0xCA,0xFF,0x05,0xFF};
			wait(waitTime);
			if(!_cb->sendCANFrame(ownID, data2, 6, CANStandard, CANData, requestTimeout))
			{
				return 0;
			}
			conTestHit++;
	   }
	   else if(data[3] == 0x7F && data[5] == 0x78)//a wait message
	   {

		   uint8_t ttype = (data[0]& 0xF0);//check for the frame type
		   if(ttype == TP20_ACK_LAST && data[1])// != 0x80)
		   {
			   uint8_t ackCounter = (data[0]& 0x0F);//Store the counter for ACK
			   wait(waitTime);
	    	   TPSendACK(ackCounter);
		   }
		   conTestHit=0;
	   }
	   else
	   {
		   break;
	   }
	   if(conTestHit == 5)//this would be around 5 secs, and would mean that diagnostics died
	   {
		   return 0;
	   }
    }
    conTestHit=0;
    uint8_t ttype = (data[0]& 0xF0);//check for the frame type
    uint8_t ackCounter = (data[0]& 0x0F);//Store the counter for ACK
    if(ttype == TP20_ACK_LAST || ttype == TP20_NOACK_LAST) //if single frame
    {
    	for(cnt=0;cnt<data[2];cnt++)//grab the data
    	{
           response[cnt]=data[cnt+3];
    	}
    	if(ttype == TP20_ACK_LAST)
    	{
    	   wait(waitTime);
    	   TPSendACK(ackCounter);
    	}
    	return data[2];//return the length
    }
    else if (ttype == TP20_ACK_FOLLOW || ttype == TP20_NOACK_FOLLOW)
    {
       uint8_t lnn=data[2];//get the length for returning it later
       for (cnt=0;cnt<5;cnt++)//grab the last 5 bytes
       {
           response[cnt]=data[(cnt+3)];
       }
       if((data[0] & 0xF0) == TP20_ACK_FOLLOW)
       {
           ackCounter = (data[0]& 0x0F);//Store the counter for ACK
           wait(waitTime);
           TPSendACK(ackCounter);
           conTestHit=0;
       }
       while((data[0] & 0xF0) != TP20_ACK_LAST && (data[0] & 0xF0) != TP20_NOACK_LAST)
       {
    	   while(1)//wait for the reply or until timeout
    	   {
    		   if(!_cb->getCANFrame(rID, data, responseTimeout))
    		   {
    			   return 0;
    		   }
    		   if (data[0] == TP20_CONNECTION_TEST)//need to expect channel test while doing stuff
    		   {
    				uint8_t data2[8]={TP20_CONNECTION_ACK,0xF,0xCA,0xFF,0x05,0xFF};
    				wait(waitTime);
    				if(!_cb->sendCANFrame(ownID, data2, 6, CANStandard, CANData, requestTimeout))
    				{
    					return 0;
    				}
    				conTestHit++;
    				if(conTestHit == 3)//this would be around 3 secs
    				{
    				   return 0;
    				}
    		   }
			   else if(data[3]==0x7F && data[5]==0x78)//if we get a "wait" request
			   {
				   uint8_t ackCounter = (data[0]& 0x0F);//Store the counter for ACK
				   if((data[0]& 0xF0) == TP20_ACK_LAST)//if they want an ack
				   {
					   wait(waitTime);
					   TPSendACK(ackCounter);
				   }
				   conTestHit=0;
			   }
    		   else//but if we got a frame from the rID
    		   {
    			   break;
    		   }
    	   }
    	   if((data[0] & 0xF0) == TP20_ACK_LAST || (data[0] & 0xF0) == TP20_NOACK_LAST)
    	   {
    		   uint8_t finalcount=(lnn - cnt);
    		   for (uint8_t cnt1=0;cnt1<finalcount;cnt1++)//grab the last 7 bytes
    		   {
				   response[cnt]=data[(cnt1 + 1)];
				   cnt++; //dont forget to increase the array counter!
			   }
			   if((data[0] & 0xF0) == TP20_ACK_LAST)//send an ACK if requested
			   {
				  ackCounter = (data[0]& 0x0F);//Store the counter for ACK
				  wait(waitTime);
				  TPSendACK(ackCounter);
			   }
			   return lnn;
    	   }
    	   else
    	   {
    		   for (uint8_t cnt1=1;cnt1<8;cnt1++)//grab the last 7 bytes
    		   {
    			   response[cnt]=data[cnt1];
    			   cnt++; //dont forget to increase the array counter!
    		   }
    		   if((data[0] & 0xF0) == TP20_ACK_FOLLOW)
    		   {
    			   ackCounter = (data[0]& 0x0F);//Store the counter for ACK
    			   wait(waitTime);
    			   TPSendACK(ackCounter);
    		   }
    		   conTestHit=0;
    	   }
       }
    }
    return 0; //should never get here
}


void TP20Handler::setTimeouts(uint32_t request, uint32_t response)
{
	requestTimeout = request;
	responseTimeout = response;	
}




bool TP20Handler::write(uint8_t *request, uint8_t len, uint8_t doACK) //sends a payload. lnn is the length, and doack is 0 for no ack and 1 for ack
{
	uint8_t rqtype;
    wait(waitTime);
    if (len < 6)//if its a single frame
    {
        uint8_t data[8]={0,0,0,0,0,0,0,0};
		data[2] = len;
        for(uint8_t a=0;a<len;a++)
		{
			data[(a + 3)]=request[a];
		}
    	if(doACK==0)
        {
           rqtype = TP20_NOACK_LAST;
        }
        else
        {
        	rqtype = TP20_ACK_LAST;
        }
        data[0]=(uint8_t)(rqtype + frameCount);
        uint8_t retries=0;
        while(retries < 3)
        {
			if(!_cb->sendCANFrame(ownID, data, (len + 3), CANStandard, CANData, requestTimeout))
			{
				return false;
			}
			increaseFrameCounter(); //increase the counter
			if(doACK==1)
			{
				uint8_t r = TPGetACK();
				if (r == 0)
				{
					return false;
				}
				else if(r == 1)
				{
					return true;
				}
				else
				{
					retries++;
					wait(waitTime);
				}
			}
			else
			{
				return true;
			}
        }
    }
    else
    {
        uint8_t cnt=0;
        uint8_t sendcnt=0; //used to store sent frames for expecting ack.
        rqtype=TP20_NOACK_FOLLOW;
        while(cnt<len)
        {
            if (cnt == 0)
            {
                uint8_t data[8]={(uint8_t)(rqtype + frameCount),0,0,0,0,0,0,0};
				data[2] = len;
                for(uint8_t a=0;a<5;a++)
				{
					data[(a + 3)]=request[a];
				}
                wait(waitTime);
        		if(!_cb->sendCANFrame(ownID, data, 8, CANStandard, CANData, requestTimeout))
        		{
        			return false;
        		}
				cnt = cnt+5;
                increaseFrameCounter(); //increase the counter
                sendcnt++;
            }
            else if((len-cnt) < 8) //last frame
            {
                if(doACK==1)
                {
                    rqtype = TP20_ACK_LAST;
                }
                uint8_t data[8]={(uint8_t)(rqtype + frameCount),request[cnt],request[cnt+1],request[cnt+2],request[cnt+3],request[cnt+4],request[cnt+5],request[cnt+6]}; //componse the single frame
                increaseFrameCounter(); //increase the counter
                wait(waitTime);
        		if(!_cb->sendCANFrame(ownID, data, (1+(len-cnt)), CANStandard, CANData, requestTimeout))
        		{
        			return false;
        		}
                if(doACK==1)
                {
                	if (TPGetACK() == 0)
                    {
                    	return false;
                    }
                    else
                    {
                    	return true;
                    }
                }
                else
                {
                	return true;
                }
                
            }
            else //middle frame
            {
            	if(sendcnt == (blockSize - 1))
            	{
            		rqtype=TP20_ACK_FOLLOW;
            	}
            	else
            	{
            		rqtype=TP20_NOACK_FOLLOW;
            	}
            	uint8_t data[8]={(uint8_t)(rqtype + frameCount),request[cnt],request[cnt+1],request[cnt+2],request[cnt+3],request[cnt+4],request[cnt+5],request[cnt+6]}; //componse the single frame
                increaseFrameCounter(); //increase the counter
                wait(waitTime);
        		if(!_cb->sendCANFrame(ownID, data, 8, CANStandard, CANData, requestTimeout))
        		{
        			return false;
        		}
                cnt = cnt+7;
                sendcnt++;
                if (sendcnt == blockSize)
                {
                	if (TPGetACK() == 0)
                    {
                    	return false;
                    }
                    sendcnt=0;
                }
            }
        }
    }
    return 0; //should never get here
}

void TP20Handler::sendCA(uint8_t wat)
{
	if(wat == 0)
	{
		while(((LPC_CAN1->GSR & 4) == 0 || (LPC_CAN2->GSR & 4) == 0)){}//hack to fix a silicon bug.
		while(_cb->available(_interface))//to make sure that there is nothing before we enable the interrupt, so it doesnt get lost
		{
			keepChannelAlive();
		}
		_canbus->attach(0);
		tick.detach();
		TPtimer.stop();
		TPtimer.reset();
		return;

	}
	else
	{
		while(((LPC_CAN1->GSR & 4) == 0 || (LPC_CAN2->GSR & 4) == 0)){}//hack to fix a silicon bug.
		while(_cb->available(_interface))//to make sure that there is nothing before we enable the interrupt, so it doesnt get lost
		{
			keepChannelAlive();
		}
		if(inSession==0)
		{
			return;
		}
		_canbus->attach(this,&TP20Handler::keepChannelAlive);
		tick.attach(this,&TP20Handler::checkSessionStatus, 1.0);
		TPtimer.start();
		return;
	}
}

void TP20Handler::checkSessionStatus()
{
	if(TPtimer.read_ms() > TP20_DEFAULT_SESSION_TIMEOUT)
	{
		closeChannel();
	}
	return;
}

void TP20Handler::keepChannelAlive()
{
	uint8_t data[8];
	if(!_cb->getCANFrame(rID, data, responseTimeout))
	{
		checkSessionStatus();
		return;
	}
	if (data[0] == TP20_CONNECTION_TEST)
	{
		uint8_t data2[8]={TP20_CONNECTION_ACK,0xF,0xCA,0xFF,0x05,0xFF};
		wait(waitTime);
		if(!_cb->sendCANFrame(ownID, data2, 6, CANStandard, CANData, requestTimeout))
		{
			return;
		}
		wait(waitTime);
		TPtimer.reset();
		return;
	}
	else if (data[0] == TP20_CONNECTION_DISCONNECT)//if we did something to upset the DUT
	{
		inSession=0;
		sendCA(0);
	    uint8_t data[8]={TP20_CONNECTION_DISCONNECT,0,0,0,0,0,0,0};
	    if(!_cb->sendCANFrame(ownID, data, 1, CANStandard, CANData, requestTimeout))
	    {
	        if(areFiltersActive==true)
	        {
	        	disableFilters();
	        }
	    	return;
	    }
	    return;
	}
}

void TP20Handler::disableFilters()
{
	_cb->CANx_ObjectSetFilter(ARM_CAN_FILTER_ID_EXACT_REMOVE, rID, rID, 1);//clean up
	_cb->CANx_ObjectSetFilter(ARM_CAN_FILTER_ID_EXACT_REMOVE, rID, rID, 2);
	_cb->CANx_ObjectSetFilter(ARM_CAN_FILTER_ID_EXACT_REMOVE, ownID, ownID, 1);
	_cb->CANx_ObjectSetFilter(ARM_CAN_FILTER_ID_EXACT_REMOVE, ownID, ownID, 2);
	_cb->disableCANFilters();
	areFiltersActive=false;
}

void TP20Handler::enableFilters()
{
	_cb->CANx_ObjectSetFilter(ARM_CAN_FILTER_ID_EXACT_ADD, rID, rID, 1);//enable filtering for ease of traffic handling
	_cb->CANx_ObjectSetFilter(ARM_CAN_FILTER_ID_EXACT_ADD, rID, rID, 2);
	_cb->CANx_ObjectSetFilter(ARM_CAN_FILTER_ID_EXACT_ADD, ownID, ownID, 1);
	_cb->CANx_ObjectSetFilter(ARM_CAN_FILTER_ID_EXACT_ADD, ownID, ownID, 2);
	_cb->enableCANFilters();
	areFiltersActive=true;
}



float TP20Handler::decodeTimeout(uint8_t timeByte)
{
	Conversions convert;
	uint8_t value=convert.getBit(timeByte,7);
	value = (value << 1) + convert.getBit(timeByte,6);
	switch (value)
	{
		case 0:
		{
			return ((float)(timeByte & 0x3F) / 10000.0);
		}
		case 1:
		{
			return ((float)(timeByte & 0x3F) / 1000.0);
		}
		case 2:
		{
			return ((float)(timeByte & 0x3F) / 100.0);
		}
		case 3:
		{
			return ((float)(timeByte & 0x3F) / 10.0);
		}
	}
	return 0;
}

