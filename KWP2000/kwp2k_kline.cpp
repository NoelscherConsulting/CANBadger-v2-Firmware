/*
* KWP2000 LIBRARY USING K-LINE 
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


#include "mbed.h"
#include "kwp2k_kline.h"

KWP2KKLINEHandler::KWP2KKLINEHandler(KLINEHandler *kline)
{
	_kline=kline;
	inSession=0;
	testerAddress = KWP2K_KLINE_DEFAULT_TESTER_ADDRESS;
	targetAddress = KWP2K_KLINE_DEFAULT_TARGET_ADDRESS;
	addressType = KWP2K_KLINE_DEFAULT_ADDRESS_TYPE;	
	AL0=0;
	AL1=0;
	HB0=0;
	HB1=0;
	TP0=0;
//	requestTimeout= (KWP2K_KLINE_DEFAULT_REQUEST_TIMEOUT * 10);//the timeouts are in ms, but the code uses us
//	responseTimeout=(KWP2K_KLINE_DEFAULT_RESPONSE_TIMEOUT * 10);
}

KWP2KKLINEHandler::~KWP2KKLINEHandler(){}
	


uint32_t KWP2KKLINEHandler::readECUID(uint8_t opt, uint8_t *buffer)
{
	/*The following options are available:         
											0x80 - All data
											0x86 - VIN and Hardware no.
											0x8A - Boot version
											0x90 - VIN
											0x91 - Manufacturer Drawing number / Main Firmware version 
											0x92 - ECU hardware no.
											0x93 - ECU hardware version no.
											0x94 - ECU software no.
											0x95 - ECU software version no.
											0x96 - Homologation code
											0x97 - ISO code
											0x98 - Tester code
											0x99 - Reprogramming/production date [Y-Y-M-D]
											0x9B - NA/Configuration software version
											*/
	uint8_t request[2]={KWP_READ_ECU_ID, opt};
	return requestResponseClient(request, 2, buffer);		
}

void KWP2KKLINEHandler::setSpeed(uint32_t baudRate)
{
	_kline->setBaudrate(baudRate);
}

void KWP2KKLINEHandler::parseKeyByte(uint8_t keyByte)
{
		Conversions convert;//we will get the config bits;
		AL0=convert.getBit(keyByte,0);
		AL1=convert.getBit(keyByte,1);
		HB0=convert.getBit(keyByte,2);
		HB1=convert.getBit(keyByte,3);
		TP0=convert.getBit(keyByte,4);
		if(addressType == 1)//we only wanna optimise with physical address
		{
			if((HB0 == 0 && HB1 == 0) || (HB0 == 1 && HB1 == 0))//oh yes, we have seen the first case nonsense
			{
				if(HB0 == 0 && HB1 == 0)
				{
					AL0=1;
				}
				addressType=1;//physical
			}
			else if((HB0 == 0 && HB1 == 1) || (HB0 == 1 && HB1 == 1))
			{
				addressType = 0;//no addressing required, faster
			}
		}	
	
	
}


bool KWP2KKLINEHandler::startComms(uint8_t initType)
{
  uint8_t pata[8]={KWP_START_COMMUNICATIONS,targetAddress,testerAddress,KWP_START_COMMUNICATIONS};
	uint8_t reply[8];
	uint16_t a = 0;
	if(inSession == 0)//if we are not in a session, then lets run startComms
	{
		if(initType == KWP_FAST)
		{
			while(a < 3)//we will try the init three times
			{
				wait(1);
				_kline->fastInit(pata, 4);//we send the fast init sequence
				read(reply);//now we get the reply
				if (reply[0]==(KWP_START_COMMUNICATIONS + 0x40))
				{
					break;
				}
				a++;
			}
			if(a == 3)
			{
				return false;
			}
			parseKeyByte(reply[1]);
		}
		else
		{
			while(a < 3)//we will try the init three times
			{
				wait(1);
				if(_kline->slowInit(targetAddress) != 0)
				{
					break;
				}
				a++;
			}
			if(a == 3)
			{
				return false;
			}
			a = _kline->getByte();//we now get the first key byte
			if(a == 0xFF00)
			{
				return false;
			}
			parseKeyByte(a & 0xFF);
			a = _kline->getByte();//now we get the second one
			if(a == 0xFF00)
			{
				return false;
			}			
			if(!_kline->sendByte(~a))//and reply with the inverse
			{
				return false;
			}
			a = _kline->getByte();//now we get the inverse of the address
			if(a == 0xFF00)
			{
				return false;
			}						
			//and now we are good
		}
		inSession=1;
		tick.attach(this,&KWP2KKLINEHandler::sendTesterPresent, 0.5);
	}	
	return true;
}

bool KWP2KKLINEHandler::stopDiagSession()
{
	uint8_t request[8]={KWP_STOP_DIAG_SESSION, 0};
	requestResponseClient(request, 1, request);		
	if(request[0] == (KWP_STOP_DIAG_SESSION + 0x40))
	{
		return true;
	}
	return false;
}

bool KWP2KKLINEHandler::stopComms()
{
	uint8_t request[8]={KWP_STOP_COMMUNICATIONS, 0};
	requestResponseClient(request, 1, request);		
	if(request[0] == (KWP_STOP_COMMUNICATIONS + 0x40))
	{
		return true;
	}
	return false;
}

bool KWP2KKLINEHandler::startDiagSession(uint8_t sub)
{
  /*sub must be 0x85 to write (download) and 0x86 to read (upload) the flash
	Possible Modes are:
								0x81 - Standard Diagnostics
								0x84 - End of Line supplier mode
								0x85 - Download mode
								0x86 - Upload mode
								0x89 - Yet another Standard Diag session
								*/
  //We try all possible baudrates to get the fastest link speed possible
  //first we try to init diag normally:
  uint8_t pata[8]={KWP_START_DIAG_SESSION,sub};
	uint8_t reply[8];
	if(inSession == 0)//if we are not in a session, then lets run startComms
	{
		if(!startComms())
		{
			return false;
		}
	}
	wait(0.01);
  //lets get started with speed...
  /*pata[2] = 0xA7;//This baudrate seems unstable on some ECUS, will discard it until a solution is found
  requestResponseClient(pata, 3, reply);
  if (reply[0] == 0x50)//If diag session is accepted...
  {
			_kline->setBaudrate(250000);//set the baudrate for the com port
      return true;
  }
	wait(0.01);*/
  pata[2] = 0x87;//125kbaud
	requestResponseClient(pata, 3, reply);	
  if (reply[0] == 0x50)
  {
			_kline->setBaudrate(125000);
			if(inSession == 0)
			{
				inSession = 1;
				tick.attach(this,&KWP2KKLINEHandler::sendTesterPresent, 0.5);
			}
      return true;
  }
  pata[2] = 0x74;//83500kbaud
	wait(0.01);
  requestResponseClient(pata, 3, reply);	
  if (reply[0] == 0x50)
  {
			_kline->setBaudrate(83500);
			if(inSession == 0)
			{
				inSession = 1;
				tick.attach(this,&KWP2KKLINEHandler::sendTesterPresent, 0.5);
			}
      return true;
  }
	
  pata[2] = 0x66;//63500kbaud
	wait(0.01);
	requestResponseClient(pata, 3, reply);	
  if (reply[0] == 0x50)
    {
			_kline->setBaudrate(63500);
			if(inSession == 0)
			{
				inSession = 1;
				tick.attach(this,&KWP2KKLINEHandler::sendTesterPresent, 0.5);
			}
      return true;
    }

  pata[2] = 0x50;//38400kbaud
	wait(0.01);
	requestResponseClient(pata, 3, reply);	
  if (reply[0] == 0x50)
  {
			_kline->setBaudrate(38400);
			if(inSession == 0)
			{
				inSession = 1;
				tick.attach(this,&KWP2KKLINEHandler::sendTesterPresent, 0.5);
			}
			return true;
  }		
	wait(0.01);
	requestResponseClient(pata, 2, reply);	//if nothing worked, lets try a normal one without the speed parameter
  if (reply[0] == 0x50)
  {
			if(inSession == 0)
			{
				inSession = 1;
				tick.attach(this,&KWP2KKLINEHandler::sendTesterPresent, 0.5);
			}
			return true;
  }			
  return false;//if nothing worked, well...
}  
	
void KWP2KKLINEHandler::sendTesterPresent()
{
		if(inSession == 0)//if we are not in a session, why would we do dis
		{
			tick.detach();
			return;
		}
		uint8_t tmpbfr[10]={0x3E};
		write(tmpbfr,1);
		uint32_t a = read(tmpbfr);
		if(a == 0 || tmpbfr[0] != 0x7E)
    {
           inSession=0;
           tick.detach();
		}		
}


void KWP2KKLINEHandler::setTimeouts(uint32_t doByteDelay, uint32_t byteTimeout, uint32_t readTimeaut)
{
	_kline->setTransmissionParameters(doByteDelay, byteTimeout, readTimeaut);
}




uint32_t KWP2KKLINEHandler::ECUReset(uint8_t resetType, uint8_t *response)
{
	uint8_t request[2]={KWP_ECU_RESET, resetType};
	return requestResponseClient(request, 2, response);
}

uint32_t KWP2KKLINEHandler::readDataByLocalID(uint8_t dataType, uint8_t *response)
{
		uint8_t request[2]={KWP_READ_DATA_BY_LOCAL_ID,dataType};
		return requestResponseClient(request, 2, response);
}

uint32_t KWP2KKLINEHandler::writeDataByLocalID(uint8_t dataType, uint8_t *data, uint8_t payLength, uint8_t *response)
{
	if(payLength > 0xFA)//why would anyone ever want to do that???
	{
		return 0;
	}
	uint8_t request[260]={KWP_WRITE_DATA_BY_LOCAL_ID, dataType};
	for (uint32_t a = 0; a < payLength; a++)
	{
		request[(a + 2)] = data[a];
	}
	return requestResponseClient(request, (payLength + 3), response);	
}


bool KWP2KKLINEHandler::readMemByAddress(uint32_t address, uint8_t howMuch, uint8_t *whereTo)
{
	if(address > 0xFFFFFF)
	{
		return false;
	}
	uint8_t request[12] = {KWP_READ_MEMORY_BY_ADDRESS,(uint8_t)(address >> 16),(uint8_t)(address >> 8), (uint8_t)(address), howMuch};
	uint32_t a = requestResponseClient(request, 5, whereTo);
	if(a == 0 || (a & 0xFF000000) != 0)//if there was an error
	{
			return false;
	}	
	for(a = 0; a < howMuch; a++)//we will now get rid of the SID and return only the data
	{
		whereTo[a] = whereTo[(a + 1)];
	}
}

bool KWP2KKLINEHandler::writeMemByAddress(uint32_t address, uint8_t howMuch, uint8_t *whereFrom)
{
	if(address > 0xFFFFFF)
	{
		return false;
	}
	uint8_t request[260] = {KWP_WRITE_MEMORY_BY_ADDRESS,(uint8_t)(address >> 16),(uint8_t)(address >> 8), (uint8_t)(address), howMuch};
	uint32_t a=0;
	for(a = 0; a < howMuch; a++)//we will now get rid of the SID and return only the data
	{
		request[(a + 5)] = whereFrom[a];
	}	
	a = requestResponseClient(request, 5, request);
	if(a == 0 || (a & 0xFF000000) != 0)//if there was an error
	{
			return false;
	}	
}



uint32_t KWP2KKLINEHandler::requestDownload(uint32_t memAddress, uint32_t memSize, uint8_t dataFormatID)
{
		uint8_t request[12]={KWP_REQUEST_DOWNLOAD,(uint8_t)(memAddress >> 16),(uint8_t)(memAddress >> 8),(uint8_t)(memAddress), dataFormatID,(uint8_t)(memSize >> 16),(uint8_t)(memSize >> 8),(uint8_t)(memSize)};
		uint32_t a = requestResponseClient(request, 8, request);
		if(a != 0 && (a & 0xFF000000) == 0)//if there was no error
		{
			return request[1];//this is the max block size
		}
		return 0;
}




uint32_t KWP2KKLINEHandler::transferDataRead(uint8_t transferType, uint32_t address, uint8_t *whereTo )
{
	uint8_t request[8]={KWP_TRANSFER_DATA};
	uint32_t rsult=0;
	uint8_t tmpBfr[260];
	if(transferType == 0)
	{
		rsult = requestResponseClient(request, 1, tmpBfr);
	}
	else if(transferType == 1)
	{
		request[1]=(uint8_t)(address);
		rsult = requestResponseClient(request, 2, tmpBfr);
	}
	else if(transferType == 2)
	{
		request[1]=(uint8_t)(address >> 8);
		request[2]=(uint8_t)(address);
		rsult = requestResponseClient(request, 3, tmpBfr);
	}	
	else if(transferType == 3)
	{
		request[1]=(uint8_t)(address >> 16);
		request[2]=(uint8_t)(address >> 8);
		request[3]=(uint8_t)(address);
		rsult = requestResponseClient(request, 4, tmpBfr);
	}	
	else if(transferType == 4)
	{
		request[1]=(uint8_t)(address >> 24);
		request[2]=(uint8_t)(address >> 16);
		request[3]=(uint8_t)(address >> 8);
		request[4]=(uint8_t)(address);
		rsult = requestResponseClient(request, 4, tmpBfr);
	}	
	if((rsult & 0xFF000000) == 0 && rsult != 0 && tmpBfr[0] == 0x76)//if all went ok
	{
		for(uint32_t c = 0; c < (rsult - 1); c++)//we need to remove one byte which is the SID positive reply
		{
			whereTo[c]=tmpBfr[(c+1)];
		}
		return (rsult - 1);
	}
	return 0;	
}


bool KWP2KKLINEHandler::transferDataWrite(uint8_t *whereFrom, uint8_t howMuch, uint8_t transferType, uint32_t address)
{
	uint8_t request[260]={KWP_TRANSFER_DATA};
	uint32_t rsult=0;
	if(transferType == 1)
	{
		request[1]=(uint8_t)(address);
	}
	else if(transferType == 2)
	{
		request[1]=(uint8_t)(address >> 8);
		request[2]=(uint8_t)(address);
	}	
	else if(transferType == 3)
	{
		request[1]=(uint8_t)(address >> 16);
		request[2]=(uint8_t)(address >> 8);
		request[3]=(uint8_t)(address);
	}	
	else if(transferType == 4)
	{
		request[1]=(uint8_t)(address >> 24);
		request[2]=(uint8_t)(address >> 16);
		request[3]=(uint8_t)(address >> 8);
		request[4]=(uint8_t)(address);
	}	
	for(uint32_t c = 0; c < howMuch; c++)//we copy the payload to the request
	{
		request[(c + 1 + transferType)] = whereFrom[c];
	}	
	rsult = requestResponseClient(request, (1 + transferType), request);//we send the request

	if((rsult & 0xFF000000) == 0 && rsult != 0 && request[0] == 0x76)//if all went ok
	{
		return true;
	}
	return false;	
}




uint8_t KWP2KKLINEHandler::requestUpload(uint32_t memAddress, uint32_t memSize, uint8_t dataFormatID)
{
		uint8_t request[12]={KWP_REQUEST_UPLOAD,(uint8_t)(memAddress >> 16),(uint8_t)(memAddress >> 8),(uint8_t)(memAddress), dataFormatID,(uint8_t)(memSize >> 16),(uint8_t)(memSize >> 8),(uint8_t)(memSize)};
		uint32_t a = requestResponseClient(request, 8, request);
		if(a != 0 && (a & 0xFF000000) == 0)//if there was no error
		{
			return request[1];//this is the max block size
		}
		return 0;
}





uint32_t KWP2KKLINEHandler::requestSeed(uint8_t *payload, uint8_t paylength, uint8_t *response)
{
	if(paylength > 0xFA)//why would anyone ever want to do that???
	{
		return 0;
	}
	uint8_t request[260]={KWP_SECURITY_ACCESS};
	for (uint32_t a = 0; a < paylength; a++)
	{
		request[(a + 1)] = payload[a];
	}
	return requestResponseClient(request, (paylength + 1), response);
}


uint32_t KWP2KKLINEHandler::requestSeed(uint8_t level, uint8_t *response)
{

	uint8_t request[2]={KWP_SECURITY_ACCESS, level};
	return requestResponseClient(request, 2, response);
}


uint32_t KWP2KKLINEHandler::sendKey(uint8_t level, uint32_t key, uint8_t *response)
{

	uint8_t request[6]={KWP_SECURITY_ACCESS, (level + 1), (uint8_t)(key >> 24), (uint8_t)(key >> 16), (uint8_t)(key >> 8), (key & 0xFF)};
	return requestResponseClient(request, 6, response);
}


uint32_t KWP2KKLINEHandler::sendKey(uint8_t *payload, uint32_t paylength, uint8_t *response)//this one is used for fuzzing.
{
	if(paylength > 0xFA)//why would anyone ever want to do that???
	{
		return 0;
	}
	uint8_t request[260]={KWP_SECURITY_ACCESS};
	for (uint32_t a = 0; a < paylength; a++)
	{
		request[(a + 1)] = payload[a];
	}
	return requestResponseClient(request, (paylength + 1), response);
}


bool KWP2KKLINEHandler::sessionStatus()
{
	return inSession;
}

void KWP2KKLINEHandler::endSession()
{
	if(inSession==1)
	{
		tick.detach();
		inSession=0;
	}
}

bool KWP2KKLINEHandler::startRoutineByLocalID(uint8_t routineID)
{
	uint8_t request[8]={KWP_START_ROUTINE_BY_LOCAL_ID, routineID};
	requestResponseClient(request, 2, request);	
	if(request[0] == (KWP_START_ROUTINE_BY_LOCAL_ID + 0x40) && request[1] == routineID)
	{
		return true;
	}
	return false;
	
}

bool KWP2KKLINEHandler::startRoutineByLocalID(uint8_t routineID, uint8_t *params, uint8_t len)
{
	uint8_t request[260]={KWP_START_ROUTINE_BY_LOCAL_ID, routineID};
	for(uint8_t a = 0; a < len; a++)
	{
		request[(a+2)] = params[a];
	}
	requestResponseClient(request, (len + 2), request);	
	if(request[0] == (KWP_START_ROUTINE_BY_LOCAL_ID + 0x40) && request[1] == routineID)
	{
		return true;
	}
	return false;
}


bool KWP2KKLINEHandler::stopRoutineByLocalID(uint8_t routineID)
{
	uint8_t request[8]={KWP_STOP_ROUTINE_BY_LOCAL_ID, routineID};
	requestResponseClient(request, 2, request);	
	if(request[0] == (KWP_STOP_ROUTINE_BY_LOCAL_ID + 0x40) && request[1] == routineID)
	{
		return true;
	}
	return false;
	
}

uint8_t KWP2KKLINEHandler::requestRoutineResultsByLocalID(uint8_t routineID, uint8_t *whereTo)
{
	uint8_t request[260]={KWP_REQUEST_ROUTINE_RESULTS_BY_LOCAL_ID, routineID};
	uint32_t r = requestResponseClient(request, 2, request);	
	if(request[0] == (KWP_REQUEST_ROUTINE_RESULTS_BY_LOCAL_ID + 0x40) && request[1] == routineID)
	{
		for(uint8_t a = 2; a < (uint8_t) r; a++)
		{
			whereTo[(a - 2)] = request[a];
		}
	}
	return 0;	
}



uint32_t KWP2KKLINEHandler::requestResponseClient(uint8_t *rqst, uint16_t len, uint8_t *response)
{
		if(inSession != 0)
		{
			tick.detach();
		}
		if(!write(rqst,len))
		{
			return 0;
		}
		uint32_t readOp = read(response);
		if(inSession != 0)
		{
			tick.attach(this,&KWP2KKLINEHandler::sendTesterPresent, 0.5);
		}
		return readOp;
}

void KWP2KKLINEHandler::setTransmissionParameters(uint32_t targetID, uint32_t sourceID)
{
	testerAddress = sourceID;
	targetAddress = targetID;
}

void KWP2KKLINEHandler::setAddressingType(uint8_t addressing)
{
	addressType = addressing;
}

uint32_t KWP2KKLINEHandler::read(uint8_t *response)//this function assumes no other traffic in the bus. need to improve it. also CARB mode is not contemplated
{
	bool itsDone = false;//we will use this to handle the wait error message
	uint32_t transmissionLength = 0;//holds the actual transmission length
	while(!itsDone)
	{
		uint32_t a = _kline->read(response);
		uint8_t startByte=0;//this is the byte where the data starts
		if(a == 0)
		{
			return 0;
		}
		//we will need to implement error handling at some point, but for now lets live in wonderland
		if(addressType == 0)//if there is no addressing
		{
			if(response[0] == 0)//if the first byte does not hold the length
			{
				transmissionLength = response[1];//length will be in the second byte
				startByte = 2; //and the data starts on the third one
			}
			else
			{
				transmissionLength = response[0];//length will be in the second byte
				startByte = 1; //and the data starts on the third one			
			}
		}
		else if (addressType == 1)//physical addressing
		{
			if((response[0] ^ 0x80) == 0)//if the length is not in the first byte
			{
				transmissionLength = response[3];//length will be in the third byte
				startByte = 4; //and the data starts on the fourth one		
			}
			else
			{
				transmissionLength = (response[0] ^ 0x80);
				startByte = 3;
			}
		}
		else if (addressType == 2)//Functional addressing
		{
			if((response[0] ^ 0xC0) == 0)//if the length is not in the first byte
			{
				transmissionLength = response[3];//length will be in the third byte
				startByte = 4; //and the data starts on the fourth one		
			}
			else
			{
				transmissionLength = (response[0] ^ 0xC0);
				startByte = 3;
			}
		}
		if(response[(startByte + 0)] != 0x7F || response[(startByte + 2)] != 0x78)//if we didnt get a wait message
		{
			itsDone = true;
			if(response[(startByte + 0)] == 0x7F)//if we got an error
			{
				transmissionLength = response[(startByte + 2)];//we will return the error code
				transmissionLength = (transmissionLength << 24);
			}
			else
			{
				for(uint32_t b = 0; b < transmissionLength; b++)//we need to remove the header bytes
				{
					response[b] = response[(startByte + b)];
				}
			}
		}
	}
	return transmissionLength;
}



bool KWP2KKLINEHandler::write(uint8_t *request, uint8_t len)
{
	uint8_t tmpbfr[260];
	memset(tmpbfr,0,260);
	uint16_t totalLen = len;//we will adjust this
	if(addressType == 0)//if no addressing
	{
		tmpbfr[0] = 0;//set the address type
		uint8_t toAdd=0;
		if( len < 64 && AL0 == true)//if we can fit the length in the first byte and its supported by the ECU
		{
			tmpbfr[0] = (uint8_t)len;//first byte should be the length
			totalLen++;
		}
		else
		{
			tmpbfr[1] = (uint8_t)len;//first byte should be the length
			toAdd = 1;//and we add the extra byte to be skipped below
			totalLen = (totalLen + 2);//we just added two bytes
		}
		for(uint16_t a = 0; a < len; a++)
		{
			tmpbfr[(a + 1 + toAdd)] = request[a];
		}
	}
	else if(addressType == 1 || addressType == 2)//if physical addressing
	{
		uint8_t toAdd=0;
		if( len < 64 && AL0 == true)//if we can fit the length in the first byte
		{
			if(addressType == 1)
			{
				tmpbfr[0] = 0x80 + (uint8_t)len;//first byte should be the length
			}
			else//works for now, but will need to rework it when we add CARB support
			{
				tmpbfr[0] = 0xC0 + (uint8_t)len;//first byte should be the length
			}
			tmpbfr[1] = targetAddress;
			tmpbfr[2] = testerAddress;
			totalLen = (totalLen + 3);
		}
		else
		{
			if(addressType == 1)
			{
				tmpbfr[0] = 0x80;//first byte should be the address identifier
			}
			else//works for now, but will need to rework it when we add CARB support
			{
				tmpbfr[0] = 0xC0;//first byte should be the address identifier
			}
			tmpbfr[1] = targetAddress;
			tmpbfr[2] = testerAddress;
			tmpbfr[3] = len;
			toAdd = 1;//and we add the extra byte to be skipped below
			totalLen = (totalLen + 4);
		}
		for(uint16_t a = 0; a < len; a++)
		{
			tmpbfr[(a + 3 + toAdd)] = request[a];
		}		
	}
	return _kline->write(tmpbfr,totalLen);
	
}
