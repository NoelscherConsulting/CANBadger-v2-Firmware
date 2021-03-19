/*
* Diagnostics SID Scanner (for functionality)
* Copyright (c) 2020 Javier Vazquez
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

#include "DiagScan.h"

DiagSCAN::DiagSCAN(CAN *canbus)
{
	_canbus=canbus;
}

DiagSCAN::~DiagSCAN()
{

}


void DiagSCAN::setTransmissionParameters(uint32_t sourceID, uint32_t targetID, CANFormat doFrameFormat, bool doUseFullFrame, uint8_t doBsByte)
{
	txID = sourceID;
	rxID = targetID;
	_frameFormat = doFrameFormat;
	_useFullFrame = doUseFullFrame;
	_paddingByte = doBsByte;
}

bool DiagSCAN::setCommsType(uint8_t commsType)
{
	if(commsType > 3)
	{
		return false;
	}
	_commsType = commsType;
	return true;
}

bool DiagSCAN::setCANSpeed(uint32_t speed)
{
	return _canbus->frequency(speed);
}


uint32_t DiagSCAN::ScanActiveUDSIDsFromLog(uint32_t *IDList, const char *fileName, FileHandler *sd, Buttons *buttons)//need to add support for extended addressing (NOT extended ID)
{

	if(!sd->doesFileExist(fileName))//if the file you want to parse doesnt exist, then nope
	{
		return 0xFFFFFFFE;//SD error
	}
	if(!sd->openFile(fileName, O_RDONLY))
	{
		return 0xFFFFFFFE;//SD Error
	}
	uint16_t IDCount=0;//to keep track of the stored IDs
    uint32_t IDs[512]={0};//Used to store already found IDs to filter them. Dont expect to find more than 512 IDS in a common-sense bus environment
    CANMessage can_msg(0,CANAny);
    uint32_t channels=0;//to store how many uds channels were detected
    uint32_t arrayPointer = 0;//used to know where the pointer for the array is
	char ch[100]={0};
	uint32_t r = 1;
    while(buttons->isButtonPressed(4) == false && IDCount < 512)//we will do this until the back button is pressed or we run out of space
    {
   		r=sd->read(ch, 14);//get the header
   		if(r < 14)
   		{
   			break;
   		}
   		uint32_t canID=ch[5];//get the CAN ID
   		canID = ( canID << 8) + ch[6];
   		canID = ( canID << 8) + ch[7];
   		canID = ( canID << 8) + ch[8];
   		uint8_t payload[8]={0};
   		uint8_t payLen=ch[13];
   		r=sd->read((char*)payload, payLen);//get the payload
   		if(r != payLen)//if the length is not correct
   		{
   			break;
   		}
        uint8_t filtered=0;
        for (uint32_t a=0;a<IDCount;a++)//make sure the ID we got is not filtered
        {
          if(canID == IDs[a])
          {
              filtered=1;
          }
        }
        if(((payload[0] & 0xF0) != 0 && (payload[0] & 0xF0) != 0x10  && (payload[0] & 0xF0) != 0x20  && (payload[0] & 0xF0) != 0x30) && filtered == 0)//if it is not an ISO-TP frame and was not filtered
        {
        	IDs[IDCount]=canID;//ad it to the filtered list
        	IDCount++;
        	filtered=1;
        }
        if(filtered==0)//if its not a filtered ID, it already looks like UDS.
        {
            if(canID==0x7DF)//if we just grab a channel broadcast used in diag, there will be multiple replies
            {
                IDs[IDCount]=canID;//Add the broadcast ID to the filter
                IDCount++;
                uint32_t currentFilePosition = sd->getFilePosition();
                uint8_t request=payload[1];
                if((payload[0] & 0xF0) == 0x10)//if it is a multiframe request, which would be weird
                {
                	request=payload[2];
                }
                bool found = false;
                while(1)
                {
					r=sd->read(ch, 14);//get the header
					if(r < 14)
					{
						break;
					}
					canID=ch[5];//get the CAN ID
					canID = ( canID << 8) + ch[6];
					canID = ( canID << 8) + ch[7];
					canID = ( canID << 8) + ch[8];
					payLen=ch[13];
					r=sd->read((char*)payload, payLen);//get the payload
					if(r != payLen)//if we reached the end
					{
						break;
					}
					//add algo to find if same request was made later to avoid false positive replies
					filtered=false;
			        for (uint32_t a=0;a<IDCount;a++)//make sure the ID we got is not filtered
			        {
			          if(canID == IDs[a])
			          {
			              filtered=1;
			          }
			        }
			        if(((payload[0] & 0xF0) != 0 && (payload[0] & 0xF0) != 0x10  && (payload[0] & 0xF0) != 0x20  && (payload[0] & 0xF0) != 0x30) && filtered == 0)//if it is not an ISO-TP frame and was not filtered
			        {
			        	IDs[IDCount]=canID;//add it to the filtered list
			        	IDCount++;
			        	filtered=1;
			        }
			        if (((payload[0] & 0xF0) == 0 && payload[1] == request) || ((payload[0] & 0xF0) == 0x10 && payload[2]  == request))//if we see the same request again, the following responses might not be for this request
			        {
			        	break;
			        }
					if (((payload[0] & 0xF0) == 0 && payload[1] == (request + 0x40)) || ((payload[0] & 0xF0) == 0x10 && payload[2]  == (request + 0x40)) || (payload[0] == 0x03 && payload[1] == 0x7F && payload [2]  == request))//but if we got a positive reply to the request
					{
                		IDList[arrayPointer]=0x7DF;//add the broadcast ID
                		arrayPointer++;
						IDList[arrayPointer]=canID;//if valid, we add it to the entry
                		arrayPointer++;
                		IDs[IDCount]=canID;//we also add it to the filter list
                		IDCount++;
                		found=true;
					}
				}
				if(!found)
				{
            		IDList[arrayPointer]=0x7DF;//add the broadcast ID
            		arrayPointer++;
					IDList[arrayPointer]=0;//if not found, there was no reply. This can happen when the server doesnt reply because its faulty, off, or doesnt exist.
					arrayPointer++;
					break;
				}
                sd->lseekFile(0,currentFilePosition);//rewind the file in case we missed other requests
            }
            else if (((payload[0] & 0xF0) == 0 && payload[1] < 0x40) || ((payload[0] & 0xF0) == 0x10 && payload[2] < 0x40))//try to see if we get a valid request UDS frame
            {
            	if((payload[0] & 0xF0) == 0)//if we get a single frame stream
                {
                	if(payload[0] <= 6 && (payload[0] + 1) <= payLen)//verify a valid length
                	{
                		IDList[arrayPointer]=canID;//if valid, we add it to the entry
                		arrayPointer++;
                		IDs[IDCount]=canID;//we also add it to the filter list
                		IDCount++;
						//now we will check for the reply
						bool found = false;
						uint8_t requestSID = payload[1];
						uint32_t currentFilePosition = sd->getFilePosition();
						while(!found)
						{
							r=sd->read(ch, 14);//get the header
							if(r < 14)
							{
								break;
							}
							canID=ch[5];//get the CAN ID
							canID = ( canID << 8) + ch[6];
							canID = ( canID << 8) + ch[7];
							canID = ( canID << 8) + ch[8];
							payLen=ch[13];
							r=sd->read((char*)payload, payLen);//get the payload
							if(r != payLen)//if the length is not correct
							{
								break;
							}
							//add algo to find if same request was made later to avoid false positive replies
							filtered=false;
					        for (uint32_t a=0;a<IDCount;a++)//make sure the ID we got is not filtered
					        {
					          if(canID == IDs[a])
					          {
					              filtered=1;
					          }
					        }
					        if(((payload[0] & 0xF0) != 0 && (payload[0] & 0xF0) != 0x10  && (payload[0] & 0xF0) != 0x20  && (payload[0] & 0xF0) != 0x30) && filtered == 0)//if it is not an ISO-TP frame and was not filtered
					        {
					        	IDs[IDCount]=canID;//add it to the filtered list
					        	IDCount++;
					        	filtered=1;
					        }
					        if (((payload[0] & 0xF0) == 0 && payload[1] == requestSID) || ((payload[0] & 0xF0) == 0x10 && payload[2]  == requestSID))//if we see the same request again, the following responses might not be for this request
					        {
					        	break;
					        }
							if (((payload[0] & 0xF0) == 0 && payload[1] == (requestSID + 0x40)) || ((payload[0] & 0xF0) == 0x10 && payload[2]  == (requestSID + 0x40)) || (payload[0] == 0x03 && payload[1] == 0x7F && payload [2]  == requestSID))//but if we got a positive reply to the request
							{
		                		IDList[arrayPointer]=canID;//if valid, we add it to the entry
		                		arrayPointer++;
		                		IDs[IDCount]=canID;//we also add it to the filter list
		                		IDCount++;
		                		found=true;
							}
						}
						if(!found)
						{
							IDList[arrayPointer]=0;//if not found, there was no reply. This can happen when the server doesnt reply because its faulty, off, or doesnt exist.
							arrayPointer++;
							break;
						}
						sd->lseekFile(0,currentFilePosition);//rewind the file in case we missed other requests
                	}

                }
            	else if((payload[0] & 0xF0) == 0x10 && payLen == 8)//Beginning of multiframe, length must be 8
            	{
            		IDList[arrayPointer]=canID;//if valid, we add it to the entry
            		arrayPointer++;
            		IDs[IDCount]=canID;//we also add it to the filter list
            		IDCount++;
					//now we will check for the reply
					bool found = false;
					uint8_t requestSID = payload[2];
					uint32_t currentFilePosition = sd->getFilePosition();
					while(!found)
					{
						r=sd->read(ch, 14);//get the header
						if(r < 14)
						{
							break;
						}
						canID=ch[5];//get the CAN ID
						canID = ( canID << 8) + ch[6];
						canID = ( canID << 8) + ch[7];
						canID = ( canID << 8) + ch[8];
						payLen=ch[13];
						r=sd->read((char*)payload, payLen);//get the payload
						if(r != payLen)//if the length is not correct
						{
							break;
						}
						//add algo to find if same request was made later to avoid false positive replies
						filtered=false;
				        for (uint32_t a=0;a<IDCount;a++)//make sure the ID we got is not filtered
				        {
				          if(canID == IDs[a])
				          {
				              filtered=1;
				          }
				        }
				        if(((payload[0] & 0xF0) != 0 && (payload[0] & 0xF0) != 0x10  && (payload[0] & 0xF0) != 0x20  && (payload[0] & 0xF0) != 0x30) && filtered == 0)//if it is not an ISO-TP frame and was not filtered
				        {
				        	IDs[IDCount]=canID;//add it to the filtered list
				        	IDCount++;
				        	filtered=1;
				        }
				        if (((payload[0] & 0xF0) == 0 && payload[1] == requestSID) || ((payload[0] & 0xF0) == 0x10 && payload[2]  == requestSID))//if we see the same request again, the following responses might not be for this request
				        {
				        	break;
				        }
						if (((payload[0] & 0xF0) == 0 && payload[1] == (requestSID + 0x40)) || ((payload[0] & 0xF0) == 0x10 && payload[2]  == (requestSID + 0x40)) || (payload[0] == 0x03 && payload[1] == 0x7F && payload [2]  == requestSID))//but if we got a positive reply to the request
						{
	                		IDList[arrayPointer]=canID;//if valid, we add it to the entry
	                		arrayPointer++;
	                		IDs[IDCount]=canID;//we also add it to the filter list
	                		IDCount++;
	                		found=true;
						}
					}
					if(found)
					{
						IDList[arrayPointer]=canID;//if valid, we add it to the entry
						arrayPointer++;
					}
					else
					{
						IDList[arrayPointer]=0;//if not found, there was no reply. This can happen when the server doesnt reply because its faulty, off, or doesnt exist.
						arrayPointer++;
						break;
					}
					sd->lseekFile(0,currentFilePosition);//rewind the file in case we missed other requests
            	}
            }
        }
    }
    sd->closeFile();//close the file
    return channels;
}



uint32_t DiagSCAN::scanUDSID(uint32_t targetID, uint8_t sessionType, uint32_t tTimeout, uint8_t variantType)//works with standard and extended IDs
{
	uint32_t firstID=0;
	uint16_t hitCount=0;//to know how many times a valid reply was found
	TPHandler tp(_canbus);//create the ISO TP object
	tp.setTransmissionParameters(targetID, targetID, _frameFormat, _useFullFrame, _paddingByte, variantType);//set the parameters accordingly
	tp.setTimeouts(500,tTimeout);//500ms for TX is quite a bit
	tp.disableFilters();//disable filtering
	uint8_t request[8]={UDS_DIAGNOSTIC_SESSION_CONTROL,UDS_DIAGNOSTIC_DEFAULT_SESSION};//This is standard for UDS.
	if(!tp.write(request,2))//write the request
	{
		return 0;
	}
	//now we will manually inspect the replies
	uint32_t ignoredIDs[100];//create a list of IDs to be ignored
	uint32_t ignoreCount=0;//pointer to the id array
	CANMessage can_msg(0,CANAny);
	uint32_t currentMS=0;
	for(uint8_t a=0; a < 200; a++)//we will inspect 200 frames
	{
		while(_canbus->read(can_msg) == 0 && currentMS<(tTimeout * 10))
    	{
    		currentMS++;
    		wait(0.0001);
    	}
    	if(currentMS == (tTimeout * 10))//if we didnt grab any valid traffic
    	{
    		if(hitCount == 0)
    		{
    			return 0;//then return no reply
    		}
    		else if(hitCount == 1)
    		{
    			return firstID;
    		}
    		else
    		{
    			return (hitCount + 0xC0000000);//C0 in MSB indicates broadcast
    		}
    	}
    	bool isValid=true;
    	for(uint32_t r=0; r < ignoreCount; r++)
    	{
    		if(ignoredIDs[r] == can_msg.id)//if the ID is in the list
    		{
    			isValid=false;//we flag so we dont check
    		}
    	}
    	if(isValid == true)//if it is a new ID
    	{
    		currentMS=0;//reset the timeout
    		if(variantType == 0)//if using standard addressing
			{
				if(can_msg.data[1] == 0x50 && can_msg.data[2] == sessionType)//if we got a positive reply
				{
					if(hitCount == 0)
					{
						firstID = (can_msg.id + 0x80000000);//this means success in establishing a diag session
					}
					hitCount++;
				}
				else
				{
					ignoredIDs[ignoreCount] = can_msg.id;
					ignoreCount++;
				}
			}
    		else//if using extended addressing
			{
				if(can_msg.data[0] == (targetID & 0xFF) && can_msg.data[2] == 0x50 && can_msg.data[3] == sessionType)//if we got a positive reply
				{
					if(hitCount == 0)
					{
						firstID = (can_msg.id + 0x80000000);//this means success in establishing a diag session
					}
					hitCount++;
				}
				else
				{
					ignoredIDs[ignoreCount] = can_msg.id;
					ignoreCount++;
				}
			}
    	}
    	if(ignoreCount == 100)
    	{
    		if(hitCount == 0)
    		{
    			return 0;//then return no reply
    		}
    		else if(hitCount == 1)
    		{
    			return firstID;
    		}
    		else
    		{
    			return (hitCount + 0xC0000000);//C0 in MSB indicates broadcast
    		}
    	}
	}
	if(hitCount == 0)
	{
		return 0;//then return no reply
	}
	else if(hitCount == 1)
	{
		return firstID;
	}
	else
	{
		return (hitCount + 0xC0000000);//C0 in MSB indicates broadcast
	}
}

bool DiagSCAN::scanTP20ID(uint32_t channel, uint8_t ecuID)
{
	KWP2KTP20Handler tp20(_canbus);
	if(tp20.channelSetup(channel, ecuID) == 1)
	{
		tp20.closeChannel();
		return true;
	}
	return false;
}

bool DiagSCAN::scanTP20Channel(uint32_t channel)
{
	for(uint8_t a=0; a< 0xF0; a++)//F0 is already broadcast ID
	{
		if(scanTP20ID(channel,a) == 1)
		{
			return true;
		}
	}
	return false;
}
