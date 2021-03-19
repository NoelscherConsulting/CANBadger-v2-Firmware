/*
* CanBadger CAN MITM library
* Copyright (c) 2019 Javier Vazquez
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


/*
MITM loads rules from /MITM/filename.txt and allocates them in RAM.
An index is allocated in tmpBuffer (IRAM) containing the offsets in XRAM for the rules.
The structure is as follows:
IRAM index:
	-Target ID (2 bytes)
	-Rule offset in XRAM (3 bytes)

XRAM Rules:
	-Condition bytes (2 bytes)
	-Target payload (8 bytes)
	-Action bytes (2 bytes)
	-Action payload (8 bytes)

Rules are consecutively stored in XRAM, using 0xFFFF in the Condition bytes to indicate that there are no more rules following.
*/



#include "CAN_MITM.h"

CAN_MITM::CAN_MITM(CANbadger *_canbadger, CAN *canbus1, CAN *canbus2, CANFormat format,  uint8_t *BSBuffr, Ser23LC1024 *ram, DigitalIn *backButton)
{
	canbadger=_canbadger;
	_canbus1=canbus1;
	_canbus2=canbus2;
	_ram=ram;
	frameFormat = format;
	_backButton = backButton;
	BSBuffer = BSBuffr;
}

CAN_MITM::~CAN_MITM()
{
}

void CAN_MITM::doMITM()
{
	CANMessage can1_msg(0,CANAny);
	CANMessage can2_msg(0,CANAny);
	uint32_t checkTable=0;
	EthernetManager *ethMan = canbadger->getEthernetManager();

	while(1)
	{

		// run the EthernetManagers Loop
		ethMan->run();

		// check for break conditions

		if(canbadger->ethernet_manager == NULL) {
			// check for back button press
			if(_backButton->read() != 0) { break; }
		} else {
			// check if current action is running
			CanbadgerSettings *settings = canbadger->getCanbadgerSettings();
			if(!settings->currentActionIsRunning) {
				break;
			}
		}

/*		if(!uartMode && !settings->currentActionIsRunning)
			break;*/

		if(_canbus1->read(can1_msg) && can1_msg.id != 0 )
		{
			checkTable=tableLookUp(can1_msg.id);
			if(checkTable == 0xFFFFFFFF)//if the ID has no rules
			{
				uint8_t timeout=0;
				while(!_canbus2->write(CANMessage(can1_msg.id, reinterpret_cast<char*>(&can1_msg.data), can1_msg.len, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					wait(0.0001);
					timeout++;
				}//make sure the msg goes out
			}
			else
			{
				if (!checkRule(1,checkTable,can1_msg.id,can1_msg.data,can1_msg.len))//check for rules
				{
					uint8_t timeout=0;
					while(!_canbus2->write(CANMessage(can1_msg.id, reinterpret_cast<char*>(&can1_msg.data), can1_msg.len, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
					{
						timeout++;
						wait(0.0001);
					}//make sure the msg goes out
				}
			}
		}
		if(_canbus2->read(can2_msg) && can2_msg.id != 0)
		{
			checkTable=tableLookUp(can2_msg.id);
			if(checkTable == 0xFFFFFFFF)//if the ID has no rules
			{
				uint8_t timeout=0;
				while(!_canbus1->write(CANMessage(can2_msg.id, reinterpret_cast<char*>(&can2_msg.data), can2_msg.len, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			else
			{
				if(!checkRule(2,checkTable,can2_msg.id,can2_msg.data,can2_msg.len))//check for rules
				{
					uint8_t timeout=0;
					while(!_canbus1->write(CANMessage(can2_msg.id, reinterpret_cast<char*>(&can2_msg.data), can2_msg.len, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
					{
						timeout++;
						wait(0.0001);
					}//make sure the msg goes out
				}
			}
		}
	}
}

uint32_t CAN_MITM::allocRAM(uint32_t ttargetID, uint32_t tOffset)
{
	if(ttargetID > 0x7FF && frameFormat == CANStandard)//nope, we dont trust the user
	{
		frameFormat = CANExtended;//so we correct stuff for them
	}
	uint32_t maddr = 0;
	if(tOffset != 0)
	{
		//_ram->read((tOffset - 7), 7, BSBuffer);
		maddr = BSBuffer[(tOffset + 4) - 7];
		maddr = ((maddr << 8) + BSBuffer[(tOffset + 5) - 7]);
		maddr = ((maddr << 8) + BSBuffer[(tOffset + 6) - 7]);
		maddr = (maddr + 1024);
		uint8_t allocdata[7]={(ttargetID >> 24),(ttargetID >> 16),(ttargetID >> 8), ttargetID, (maddr >> 16), (maddr >> 8), maddr};
		for(uint8_t a = 0; a < 7; a++)
		{
			BSBuffer[a + tOffset] = allocdata[a];
		}
		//_ram->write(tOffset, 7, allocdata);
	}
	else
	{
		maddr = 0;
		uint8_t allocdata[7]={(ttargetID >> 24),(ttargetID >> 16),(ttargetID >> 8), ttargetID, (maddr >> 16), (maddr >> 8), maddr};
		for(uint8_t a = 0; a < 7; a++)
		{
			BSBuffer[a + tOffset] = allocdata[a];
		}
		//_ram->write(tOffset, 7, allocdata);
	}
	return maddr;
}

uint32_t CAN_MITM::tableLookUp(uint32_t canID)//will return the pointer to the address where the rules are stored.
{
	uint32_t a=0;
	while(a < 0x800)
	{
		uint32_t checkedID = BSBuffer[a];
		checkedID = ((checkedID << 8) + BSBuffer[a+1]);
		checkedID = ((checkedID << 8) + BSBuffer[a+2]);
		checkedID = ((checkedID << 8) + BSBuffer[a+3]);
		if(canID == checkedID)
		{
			uint32_t b = BSBuffer[a+4];
			b = ((b << 8) + BSBuffer[a+5]);
			b = ((b << 8) + BSBuffer[a+6]);
			return b;
		}
		else if(checkedID ==  0xFFFFFFFF)
		{
			return 0xFFFFFFFF;//if not found, return this value
		}
		a = a + 7;
	}
	return 0xFFFFFFFF;//if not found, return this value
}

bool CAN_MITM::addRule(uint32_t offset, uint32_t cType, uint8_t *tPayload, uint32_t Action, uint8_t *aPayload)//checks if rules already exists and adds them if they dont
{
	uint32_t c=0;
	while(c < 51)//max number of rules per ID to be able to store them in XRAM when fetching later
	{
		uint8_t BSBuffer2[23]={0};
		_ram->read((offset + (c * 20)),20, BSBuffer2);//grab a rule from RAM
		uint16_t condCheck= BSBuffer2[0];
		condCheck= ((condCheck << 8) + BSBuffer2[1]);
		if(condCheck == 0xFFFF)//free space for rule, assuming it was not found earlier
		{
			uint8_t ruleW[22]={(cType >> 8), cType, tPayload[0], tPayload[1], tPayload[2], tPayload[3], tPayload[4], tPayload[5], tPayload[6], tPayload[7],(Action >> 8), Action, aPayload[0], aPayload[1],aPayload[2],aPayload[3],aPayload[4],aPayload[5],aPayload[6], aPayload[7], 0xFF, 0xFF};
			_ram->write((offset + (c * 20)), 22, ruleW);//write the rule with the two termination bytes
			return true;
		}
		else //check if the rule was already added
		{
			uint8_t tPayloadA[8] = {BSBuffer2[2],  BSBuffer2[3], BSBuffer2[4], BSBuffer2[5], BSBuffer2[6], BSBuffer2[7], BSBuffer2[8], BSBuffer2[9]};
			uint32_t ActionA = BSBuffer2[10];
			ActionA = ((ActionA << 8) + BSBuffer2[11]);
			uint8_t aPayloadA[8] = {BSBuffer2[12],  BSBuffer2[13], BSBuffer2[14], BSBuffer2[15], BSBuffer2[16], BSBuffer2[17], BSBuffer2[18], BSBuffer2[19]};
			if(cType == condCheck && Action == ActionA && memcmp(tPayload,tPayloadA,8) == 0 && memcmp(aPayload,aPayloadA,8) == 0)//if rule already exists
			{
				return true;
			}
		}
		c++;
	}
	return false;
}

uint32_t CAN_MITM::getLastIDEntryOffset()
{
	uint32_t a=0;
	while(a < 0x800)
	{
		uint32_t checkedID = BSBuffer[a];
		checkedID = ((checkedID << 8) + BSBuffer[a+1]);
		checkedID = ((checkedID << 8) + BSBuffer[a+2]);
		checkedID = ((checkedID << 8) + BSBuffer[a+3]);
		if(checkedID ==  0xFFFFFFFF)
		{
			return a;
		}
		a = a + 7;
	}
	return 0xFFFFFFFF;//if no more space left, return this value
}

bool CAN_MITM::checkRule(uint8_t busno, uint32_t offset, uint32_t ID, uint8_t *data, uint8_t leng)//checks if there are rules for specific payload and checks for condition matches
{
	uint8_t condType = 0;
	uint8_t condByteMask=0;
	uint8_t tmpbf[23]={0};
	uint16_t cnt=0;//to handle rule
	while(cnt < 51)//max rule storage offset
	{
		_ram->read((offset + (cnt * 20)),20, tmpbf); //Grab the rules from XRAM and store it in IRAM for faster handling
		if(tmpbf[0] == 0xFF && tmpbf[1] == 0xFF)//if we have reached the end of the rules
		{
			return false;
		}
		condByteMask=tmpbf[0];
		condType=tmpbf[1];

		switch (condType)
		{
			case 0://If entire frame matches
			{
				uint8_t ccnt=0;
				for(uint8_t a=0;a<leng;a++)
				{
					if(data[a] != tmpbf[2 + a] )
					{
						break;
					}
					ccnt++;
				}
				if (ccnt == leng) { return applyRule(busno, ID, data, leng, tmpbf); }
				break;
			}
			case 1://check for specific bytes if equal
			{
				uint8_t ccnt=0;
				for(uint8_t a=0;a<leng;a++)
				{
					if(converti.getBit(condByteMask,a))
					{
						if(data[a] != tmpbf[2 + a] )
						{
							break;
						}
					}
					ccnt++;
				}
				if (ccnt == leng) { return applyRule(busno, ID, data, leng, tmpbf); }
				break;
			}
			case 2://check for specific bytes if greater
			{
				uint8_t ccnt=0;
				for(uint8_t a=0;a<leng;a++)
				{
					if(converti.getBit(condByteMask,a))
					{
						if(data[a] <= tmpbf[2 + a] )
						{
							break;
						}
					}
					ccnt++;
				}
				if (ccnt == leng) { return applyRule(busno, ID, data, leng, tmpbf); }
				break;
			}
			case 3://check for specific bytes if less
			{
				uint8_t ccnt=0;
				for(uint8_t a=0;a<leng;a++)
				{
					if(converti.getBit(condByteMask,a))
					{
						if(data[a] >= tmpbf[2 + a] )
						{
							break;
						}
					}
					ccnt++;
				}
				if (ccnt == leng) { return applyRule(busno, ID, data, leng, tmpbf); }
				break;
			}
			default:
			{
				return false;//unknown type or the end of the rules, so just forward the frame as it is and dont check for more rules
			}
		}
		cnt++;
	}
    return false;
}


// if checkRule() found an applicable rule, this function will be called and the message will be transformed or discarded according to actionType and actionByteMask
bool CAN_MITM::applyRule(uint8_t busno, uint32_t ID, uint8_t *data, uint8_t leng, uint8_t *tmpbf) {
	uint8_t actionType = tmpbf[11];
	uint8_t actionByteMask = tmpbf[10];

	switch (actionType)
	{
		case 0://Swap an entire frame
		{
			uint8_t toSend[8] = {tmpbf[12], tmpbf[13],tmpbf[14],tmpbf[15],tmpbf[16],tmpbf[17],tmpbf[18],tmpbf[19]};
			if (busno == 1)
			{
				uint8_t timeout=0;
				while(!_canbus2->write(CANMessage(ID, reinterpret_cast<char*>(&toSend), leng, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			else
			{
				uint8_t timeout=0;
				while(!_canbus1->write(CANMessage(ID, reinterpret_cast<char*>(&toSend), leng, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			return true;//rule applied, so nothing more to see here
		}
		case 1://Swap specific bytes
		{
			uint8_t toSend[8]={0};
			for(uint8_t a=0;a<leng;a++)
			{
				if(converti.getBit(actionByteMask,a))
				{
					toSend[a]=tmpbf[12 + a];
				}
				else
				{
					toSend[a]=data[a];
				}
			}
			if (busno == 1)
			{
				uint8_t timeout=0;
				while(!_canbus2->write(CANMessage(ID, reinterpret_cast<char*>(&toSend), leng, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			else
			{
				uint8_t timeout=0;
				while(!_canbus1->write(CANMessage(ID, reinterpret_cast<char*>(&toSend), leng, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			return true;
		}
		case 2://add a fixed value to specific bytes
		{
			uint8_t toSend[8]={0};
			for(uint8_t a=0;a<leng;a++)
			{
				if(converti.getBit(actionByteMask,a))
				{
					toSend[a]=(data[a] + tmpbf[12 + a]);
				}
				else
				{
					toSend[a]=data[a];
				}
			}
			if (busno == 1)
			{
				uint8_t timeout=0;
				while(!_canbus2->write(CANMessage(ID, reinterpret_cast<char*>(&toSend), leng, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			else
			{
				uint8_t timeout=0;
				while(!_canbus1->write(CANMessage(ID, reinterpret_cast<char*>(&toSend), leng, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			return true;
		}
		case 3://substract a fixed value to specific bytes
		{
			uint8_t toSend[8]={0};
			for(uint8_t a=0;a<leng;a++)
			{
				if(converti.getBit(actionByteMask,a))
				{
					toSend[a]=(data[a] - tmpbf[12 + a]);
				}
				else
				{
					toSend[a]=data[a];
				}
			}
			if (busno == 1)
			{
				uint8_t timeout=0;
				while(!_canbus2->write(CANMessage(ID, reinterpret_cast<char*>(&toSend), leng, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			else
			{
				uint8_t timeout=0;
				while(!_canbus1->write(CANMessage(ID, reinterpret_cast<char*>(&toSend), leng, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			return true;
		}
		case 4://Multiply specific bytes
		{
			uint8_t toSend[8]={0};
			for(uint8_t a=0;a<leng;a++)
			{
				if(converti.getBit(actionByteMask,a))
				{
					toSend[a]=(data[a] * tmpbf[12 + a]);
				}
				else
				{
					toSend[a]=data[a];
				}
			}
			if (busno == 1)
			{
				uint8_t timeout=0;
				while(!_canbus2->write(CANMessage(ID, reinterpret_cast<char*>(&toSend), leng, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			else
			{
				uint8_t timeout=0;
				while(!_canbus1->write(CANMessage(ID, reinterpret_cast<char*>(&toSend), leng, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			return true;
		}
		case 5://Divide specific bytes
		{
			uint8_t toSend[8]={0};
			for(uint8_t a=0;a<leng;a++)
			{
				if(converti.getBit(actionByteMask,a))
				{
					toSend[a]=(data[a] / tmpbf[12 + a]);
				}
				else
				{
					toSend[a]=data[a];
				}
			}
			if (busno == 1)
			{
				uint8_t timeout=0;
				while(!_canbus2->write(CANMessage(ID, reinterpret_cast<char*>(&toSend), leng, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			else
			{
				uint8_t timeout=0;
				while(!_canbus1->write(CANMessage(ID, reinterpret_cast<char*>(&toSend), leng, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			return true;
		}
		case 6://Increase a percent to specific bytes
		{
			uint8_t toSend[8]={0};
			for(uint8_t a=0;a<leng;a++)
			{
				if(converti.getBit(actionByteMask,a))
				{
					toSend[a]=(data[a] + ((data[a] * tmpbf[12 + a]) / 100));
				}
				else
				{
					toSend[a]=data[a];
				}
			}
			if (busno == 1)
			{
				uint8_t timeout=0;
				while(!_canbus2->write(CANMessage(ID, reinterpret_cast<char*>(&toSend), leng, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			else
			{
				uint8_t timeout=0;
				while(!_canbus1->write(CANMessage(ID, reinterpret_cast<char*>(&toSend), leng, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			return true;
		}
		case 7://Decrease a percent to specific bytes
		{
			uint8_t toSend[8]={0};
			for(uint8_t a=0;a<leng;a++)
			{
				if(converti.getBit(actionByteMask,a))
				{
					toSend[a]=(data[a] - ((data[a] * tmpbf[12 + a]) / 100));
				}
				else
				{
					toSend[a]=data[a];
				}
			}
			if (busno == 1)
			{
				uint8_t timeout=0;
				while(!_canbus2->write(CANMessage(ID, reinterpret_cast<char*>(&toSend), leng, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			else
			{
				uint8_t timeout=0;
				while(!_canbus1->write(CANMessage(ID, reinterpret_cast<char*>(&toSend), leng, CANData, frameFormat)) && _backButton->read() != 0 && timeout < 100)
				{
					timeout++;
					wait(0.0001);
				}//make sure the msg goes out
			}
			return true;
		}
		case 8://drop the frame
		{
			return true;//do nothing so the frame will be dropped
		}
		default:
		{
			return false;//unknown rule, so just forward the frame as it is and dont check for more rules
		}
	}
}
