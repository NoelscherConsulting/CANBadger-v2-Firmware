/*
* SOCKETCAN LIBRARY
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



////it will NOT send can frames, need to figure out why


#include "socketcan.h"

Serial ftdi(FTDI_TX, FTDI_RX);//Serial object for FTDI

SocketCAN::SocketCAN(CAN *canbus, uint8_t interfaceNo, uint8_t *buffer)
{
	cBuffer = buffer;
	isSetup=false;
	isOpen=false;
	listenOnly=false;
	isPolling=false;
	CANStatus=0;
	_interfaceNo = interfaceNo;
	isTimeStampEnabled = false;
	_canbus = canbus;
	cBufferWritePointer = 0;
	cBufferCurrentPointer = 0;
	pendingRequests=0;
	perfCounter=0;//used only for test

}

SocketCAN::~SocketCAN()
{
	_canbus->reset();
}

void SocketCAN::start(Buttons* buttons)
{
	ftdi= new Serial(FTDI_TX,FTDI_RX);//create the serial object
	ftdi->baud(FTDI_BAUDRATE);//set the baudrate
	// Setup a serial interrupt function to receive data
	ftdi->attach(this,&SocketCAN::commandQueue, Serial::RxIrq);
	while(!buttons->isButtonPressed(4))//we will now wait for user input to stop socketcan mode
	{
		if(pendingRequests > 0)
		{
			processCommand();
		}
	}
}

void SocketCAN::resetTimerScheduler()
{
	timer.reset();
}

void SocketCAN::commandQueue()
{
	while(ftdi->readable())
	{
		cBuffer[cBufferWritePointer]=ftdi->getc();
		if(cBuffer[cBufferWritePointer] == 0xD)//if it is the end of a request
		{
			pendingRequests++;
		}
		cBufferWritePointer++;
		if(cBufferWritePointer == 4096)//to make it circular
		{
			cBufferWritePointer = 0;
		}
	}
}



void SocketCAN::processCommand()
{
	if(isPolling == true)//disable interrupt so we can reply properly
	{
		_canbus->attach(0);
	}
	char cmd=0;
	char params[64];
	uint8_t q=0;
	char reply[64];
	uint8_t replyLen=0;
	memset(reply,0,64);
	memset(params,0,64);
	while(cmd != 0xD)//we get a full request
	{
		if(cBufferCurrentPointer == 4096)//to make it circular
		{
			cBufferCurrentPointer = 0;
		}
		params[q]=cBuffer[cBufferCurrentPointer];
		cmd = cBuffer[cBufferCurrentPointer];
		q++;
		cBufferCurrentPointer++;
		if(q == 250)//this would mean we got just garbage
		{
			return;
		}
	}
	switch(params[0])
	{
		case 'S'://Set Speed
		{
			if(params[1] < 0x30 || params[1] > 0x38 || params[2] != 0xD)//if it is not a valid command
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			switch(params[1])//now we will set the appropiate speed
			{
				case 0x30://10kbit
				{
					_canbus->frequency(10000);
					break;
				}
				case 0x31://20kbit
				{
					_canbus->frequency(20000);
					break;
				}
				case 0x32://50kbit
				{
					_canbus->frequency(50000);
					break;
				}
				case 0x33://100kbit
				{
					_canbus->frequency(100000);
					break;
				}
				case 0x34://125kbit
				{
					_canbus->frequency(125000);
					break;
				}
				case 0x35://250kbit
				{
					_canbus->frequency(250000);
					break;
				}
				case 0x36://500kbit
				{
					_canbus->frequency(500000);
					break;
				}
				case 0x37://800kbit
				{
					_canbus->frequency(800000);
					break;
				}
				case 0x38://1Mbit
				{
					_canbus->frequency(1000000);
					break;
				}
			}
			isSetup=true;
			reply[replyLen] = 0xD;
			replyLen++;
			break;
		}
		case 's'://set can speed register, not supported at the moment. need to translate sja1000 values to lpc1769 values
		{
			reply[0] = 7;
			replyLen++;
			break;
		}
		case 'O'://open with TX and RX
		{
			if(params[1] != 0xD)
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(isOpen == true || isSetup == false)//if it was already open or not setup
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			isOpen=true;
			reply[replyLen] = 0xD;
			replyLen++;
			isPolling=true;
			_canbus->attach(this,&SocketCAN::getCANFrame, CAN::RxIrq);
			//led should be solid here
			break;
		}
		case 'L'://listen only AKA monitor mode
		{
			if(params[1] != 0xD)
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(isOpen == true || isSetup == false)//if it was already open or not setup
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			_canbus->monitor(true);//we set the monitor mode
			listenOnly=true;
			isOpen=true;
			reply[replyLen] = 0xD;
			replyLen++;
			isPolling=true;
			_canbus->attach(this,&SocketCAN::getCANFrame, CAN::RxIrq);
			//led should be blinking here
			break;
		}
		case 'C'://close connection
		{
			if(params[1] != 0xD)
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(isOpen == false)//if it was not open
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(listenOnly == true)//if it was in listen only mode
			{
				_canbus->monitor(false);//we unset the monitor mode
			}
			if(isPolling == true)
			{
				_canbus->attach(0);//remove the interrupt
			}
			isOpen=false;
			reply[replyLen] = 0xD;
			replyLen++;
			//led should be off here
			break;
		}
		case 't'://transmit standard ID frame
		{
			uint8_t len = convert.arrayToHex((params + 4),1);
			if(params[(5 + (len * 2))] != 0xD)//if termination is not CR
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(isOpen == false)//if it was not open
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(listenOnly == true)//if we are on listen mode only, no frame should be sent
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			uint32_t id=convert.arrayToHex((params + 1),3);
			uint8_t data[8];
			uint8_t b=0;
			for(uint8_t a=0; a < len; a++)
			{
				data[a]=convert.arrayToHex((params + 5 + b),2);
				b = b+2;
			}
			while(((LPC_CAN1->GSR & 4) == 0 || (LPC_CAN2->GSR & 4) == 0)){}//fix for silicon bug
			_canbus->write(CANMessage(id, reinterpret_cast<char*>(data), len, CANData, CANStandard));//send the CAN frame
			if(isPolling == true)
			{
				reply[replyLen] = 'z';
				replyLen++;
			}
			reply[replyLen] = 0xD;
			replyLen++;
			break;
		}
		case 'T'://transmit extended ID frame
		{
			uint8_t len = convert.arrayToHex((params + 9),1);
			if(params[(10 + (len * 2))] != 0xD)//if termination is not CR
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(isOpen == false)//if it was not open
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(listenOnly == true)//if we are on listen mode only, no frame should be sent
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			uint32_t id=convert.arrayToHex((params + 1),8);
			uint8_t data[8];
			uint8_t b=0;
			for(uint8_t a=0; a < len; a++)
			{
				data[a]=convert.arrayToHex((params + 10 + b),2);
				b = b+2;
			}
			while(((LPC_CAN1->GSR & 4) == 0 || (LPC_CAN2->GSR & 4) == 0)){}//fix for silicon bug
			_canbus->write(CANMessage(id, reinterpret_cast<char*>(data), len, CANData, CANExtended));//send the CAN frame
			if(isPolling == true)
			{
				reply[replyLen] = 'z';
				replyLen++;
			}
			reply[replyLen] = 0xD;
			replyLen++;
			break;
		}
		case 'r'://transmit standard ID remote frame
		{
			uint8_t len = convert.arrayToHex((params + 4),1);
			if(params[5] != 0xD)//if termination is not CR
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(isOpen == false)//if it was not open
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(listenOnly == true)//if we are on listen mode only, no frame should be sent
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			uint32_t id=convert.arrayToHex((params + 1),3);
			uint8_t data[8];
			while(((LPC_CAN1->GSR & 4) == 0 || (LPC_CAN2->GSR & 4) == 0)){}//fix for silicon bug
			_canbus->write(CANMessage(id, reinterpret_cast<char*>(data), len, CANRemote, CANStandard));//send the CAN frame
			if(isPolling == true)
			{
				reply[replyLen] = 'z';
				replyLen++;
			}
			reply[replyLen] = 0xD;
			replyLen++;
			break;
		}
		case 'R'://transmit extended ID Remote frame
		{
			uint8_t len = convert.arrayToHex((params + 9),1);
			if(params[10] != 0xD)//if termination is not CR
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(isOpen == false)//if it was not open
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(listenOnly == true)//if we are on listen mode only, no frame should be sent
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			uint32_t id=convert.arrayToHex((params + 1),8);
			uint8_t data[8];
			while(((LPC_CAN1->GSR & 4) == 0 || (LPC_CAN2->GSR & 4) == 0)){}//fix for silicon bug
			_canbus->write(CANMessage(id, reinterpret_cast<char*>(data), len, CANRemote, CANExtended));//send the CAN frame
			if(isPolling == true)
			{
				reply[replyLen] = 'z';
				replyLen++;
			}
			reply[replyLen] = 0xD;
			replyLen++;
			break;
		}
		case 'P'://Poll single can frame. Since we are emulating newer FW than V1220, this is disabled when autopoll is enabled.
		{
			if(params[1] != 0xD)//if termination is not CR
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(isOpen == false)//if it was not open
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(isPolling == true)//if we have autopoll enabled
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			CANMessage can_msg(0,CANAny);//create a message for any kind of ID
			if(_canbus->read(can_msg) == 0)//if there are no frames
			{
				reply[replyLen] = 0xD;
				replyLen++;
				break;
			}
			memset(params,0,64);
			if(can_msg.id < 0x800)//if standard ID
			{
				convert.itox(can_msg.id, params, 3);
				//for standard ID
				reply[replyLen] = 't';
				replyLen++;
				for(uint8_t a=0; a<3; a++)
				{
					reply[replyLen] = params[a];
					replyLen++;
				}
				memset(params,0,64);
				convert.itox(can_msg.len, params, 1);
				reply[replyLen] = params[0];
				replyLen++;
				for(uint8_t a=0; a < can_msg.len; a++)//now we will put out data bytes
				{
					memset(params,0,64);
					convert.itox(can_msg.data[a], params, 2);
					reply[replyLen] = params[0];
					replyLen++;
					reply[replyLen] = params[1];
					replyLen++;
				}
				if(isTimeStampEnabled == true)//if timestamp is enabled
				{
					uint16_t currentTime= timer.read_ms();
					memset(params,0,64);
					convert.itox(currentTime, params, 4);
					for(uint8_t a=0; a<4; a++)
					{
						reply[replyLen] = params[a];
						replyLen++;
					}
				}
				reply[replyLen] = 0xD;
				replyLen++;
				break;
			}
			else
			{
				convert.itox(can_msg.id, params, 8);
				reply[replyLen] = 'T';
				replyLen++;
				for(uint8_t a=0; a<8; a++)
				{
					reply[replyLen] = params[a];//put out the id
					replyLen++;
				}
				memset(params,0,64);
				convert.itox(can_msg.len, params, 1);
				reply[replyLen] = params[0];
				replyLen++;
				for(uint8_t a=0; a < can_msg.len; a++)//now we will put out data bytes
				{
					memset(params,0,64);
					convert.itox(can_msg.data[a], params, 2);
					reply[replyLen] = params[0];
					replyLen++;
					reply[replyLen] = params[1];
					replyLen++;
				}
				if(isTimeStampEnabled == true)//if timestamp is enabled
				{
					uint16_t currentTime= timer.read_ms();
					memset(params,0,64);
					convert.itox(currentTime, params, 4);
					for(uint8_t a=0; a<4; a++)
					{
						reply[replyLen] = params[a];
						replyLen++;
					}
				}
				reply[replyLen] = 0xD;
				replyLen++;
				break;
			}

		}
		case 'A'://Poll all FIFO can frames. Since we are emulating newer FW than V1220, this is disabled when autopoll is enabled.
		{
			if(params[1] != 0xD)//if termination is not CR
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(isOpen == false)//if it was not open
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(isPolling == true)//if we have autopoll enabled
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			CANMessage can_msg(0,CANAny);//create a message for any kind of ID
			while(_canbus->read(can_msg) != 0)//while there are frames in buffer
			{
				memset(params,0,64);
				if(can_msg.id < 0x800)//if standard ID
				{
					convert.itox(can_msg.id, params, 3);
					reply[replyLen] = 't';
					replyLen++;
					for(uint8_t a=0; a<3; a++)
					{
						reply[replyLen] = params[a];
						replyLen++;
					}
					memset(params,0,64);
					convert.itox(can_msg.len, params, 1);
					reply[replyLen] = params[0];
					replyLen++;
					for(uint8_t a=0; a < can_msg.len; a++)//now we will put out data bytes
					{
						memset(params,0,64);
						convert.itox(can_msg.data[a], params, 2);
						reply[replyLen] = params[0];
						replyLen++;
						reply[replyLen] = params[1];
						replyLen++;
					}
					if(isTimeStampEnabled == true)//if timestamp is enabled
					{
						uint16_t currentTime= timer.read_ms();
						memset(params,0,64);
						convert.itox(currentTime, params, 4);
						for(uint8_t a=0; a<4; a++)
						{
							reply[replyLen] = params[a];//put the timestamp
							replyLen++;
						}
					}
					reply[replyLen] = 0xD;
					replyLen++;
				}
				else
				{
					convert.itox(can_msg.id, params, 8);
					reply[replyLen] = 'T';
					replyLen++;
					for(uint8_t a=0; a<8; a++)
					{
						reply[replyLen] = params[a];
						replyLen++;
					}
					memset(params,0,64);
					convert.itox(can_msg.len, params, 1);
					reply[replyLen] = params[0];
					replyLen++;
					for(uint8_t a=0; a < can_msg.len; a++)//now we will put out data bytes
					{
						memset(params,0,64);
						convert.itox(can_msg.data[a], params, 2);
						reply[replyLen] = params[0];
						replyLen++;
						reply[replyLen] = params[1];
						replyLen++;
					}
					if(isTimeStampEnabled == true)//if timestamp is enabled
					{
						uint16_t currentTime= timer.read_ms();
						memset(params,0,64);
						convert.itox(currentTime, params, 4);
						for(uint8_t a=0; a<4; a++)
						{
							reply[replyLen] = params[a];
							replyLen++;
						}
					}
					reply[replyLen] = 0xD;
					replyLen++;
				}
			}
			reply[replyLen] = 'A';
			replyLen++;
			reply[replyLen] = 0xD;
			replyLen++;
			break;
		}
		case 'X'://Enable or disable autopoll
		{
			if(params[2] != 0xD)//if termination is not CR
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(isOpen == true)//if it was open
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			switch(params[1])
			{
				case '0':
				{
					if(isPolling == true)
					{
						_canbus->attach(0);
						isPolling=false;
					}
				}
				case '1':
				{
					if(isPolling == false)
					{
						_canbus->attach(this,&SocketCAN::getCANFrame, CAN::RxIrq);
						isPolling=true;
					}
				}
			}
			reply[replyLen] = 0xD;
			replyLen++;
			break;
		}
		case 'F'://get status register
		{
			if(params[1] != 0xD)//if termination is not CR
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(isOpen == false)//if it was not open
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			reply[replyLen] = 'F';
			replyLen++;
			updateStatusByte();//we get the status byte
			convert.itox(CANStatus, params, 2);
			reply[replyLen] = params[0];
			replyLen++;
			reply[replyLen] = params[1];
			replyLen++;
			reply[replyLen] = 0xD;
			replyLen++;
			break;
		}
		case 'U'://set the baudrate
		{
			if(params[2] != 0xD)//if termination is not CR
			{
				reply[replyLen] = 7;
				replyLen++;
				break;
			}
			if(isOpen == true)//if it was open
			{
				reply[replyLen] = 7;
				replyLen++;
				break;
			}
			reply[replyLen] = 0xD;
			replyLen++;
			switch(params[1])
			{
				case '0':
				{
					ftdi->baud(230400);//set the baudrate
					break;
				}
				case '1':
				{
					ftdi->baud(115200);//set the baudrate
					break;
				}
				case '2':
				{
					ftdi->baud(57600);//set the baudrate
					break;
				}
				case '3':
				{
					ftdi->baud(38400);//set the baudrate
					break;
				}
				case '4':
				{
					ftdi->baud(19200);//set the baudrate
					break;
				}
				case '5':
				{
					ftdi->baud(9600);//set the baudrate
					break;
				}
				case '6':
				{
					ftdi->baud(2400);//set the baudrate
					break;
				}
			}
			break;
		}
		case 'V'://get firmware version
		{
			if(params[1] != 0xD)//if termination is not CR
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			//we will reply to the base version that we used for the specs
			reply[replyLen] = 'V';
			replyLen++;
			reply[replyLen] = '1';
			replyLen++;
			reply[replyLen] = '2';
			replyLen++;
			reply[replyLen] = '2';
			replyLen++;
			reply[replyLen] = '0';
			replyLen++;
			reply[replyLen] = 0xD;
			replyLen++;
			break;
		}
		case 'N'://get Serial Number
		{
			if(params[1] != 0xD)//if termination is not CR
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			//we will reply with the serial number used in their docs
			reply[replyLen] = 'N';
			replyLen++;
			reply[replyLen] = 'A';
			replyLen++;
			reply[replyLen] = '1';
			replyLen++;
			reply[replyLen] = '2';
			replyLen++;
			reply[replyLen] = '3';
			replyLen++;
			reply[replyLen] = 0xD;
			replyLen++;
			break;
		}
		case 'Z'://enable/disable timestamp
		{
			if(params[2] != 0xD)//if termination is not CR
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			if(isOpen == true)//if it was open
			{
				reply[0] = 7;
				replyLen++;
				break;
			}
			switch(params[1])
			{
				case '0'://disable
				{
					if(isTimeStampEnabled == true)
					{
						timer.stop();
						timer.reset();
						isTimeStampEnabled =false;
						resetTimer.detach();
					}

				}
				case '1':
				{
					timer.reset();
					timer.start();
					isTimeStampEnabled =true;
					resetTimer.attach(this,&SocketCAN::resetTimerScheduler, 60.0);
				}
			}
			reply[replyLen] = 0xD;
			replyLen++;
			break;
		}

		default:
		{
			reply[0] = 7;
			replyLen++;
			break;
		}
	}
	for(uint8_t a = 0; a < replyLen; a++)//we send the reply
	{
		ftdi->putc(reply[a]);
	}
	if(isPolling == true)//reattach the interrupt if it was in place
	{
		while(((LPC_CAN1->GSR & 4) == 0 || (LPC_CAN2->GSR & 4) == 0)){}//fix for silicon bug. If we enable the interrupt while TX is ongoing, it gets dropped
		_canbus->attach(this,&SocketCAN::getCANFrame, CAN::RxIrq);
	}
	pendingRequests--;//remove the processed pending request

}


void SocketCAN::getCANFrame()
{
	char params[64];
	char reply[64];
	uint8_t replyLen=0;
	memset(reply,0,64);
	CANMessage can_msg(0,CANAny);//create a message for any kind of ID
	while(_canbus->read(can_msg) != 0)//while there are frames in buffer
	{
		memset(params,0,64);
		if(can_msg.id < 0x800)//if standard ID
		{
			convert.itox(can_msg.id, params, 3);
			reply[replyLen] = 't';
			replyLen++;
			for(uint8_t a=0; a<3; a++)
			{
				reply[replyLen] = params[a];
				replyLen++;
			}
			memset(params,0,64);
			convert.itox(can_msg.len, params, 1);
			reply[replyLen] = params[0];
			replyLen++;
			for(uint8_t a=0; a < can_msg.len; a++)//now we will put out data bytes
			{
				memset(params,0,64);
				convert.itox(can_msg.data[a], params, 2);
				reply[replyLen] = params[0];
				replyLen++;
				reply[replyLen] = params[1];
				replyLen++;
			}
			if(isTimeStampEnabled == true)//if timestamp is enabled
			{
				uint16_t currentTime= timer.read_ms();
				memset(params,0,64);
				convert.itox(currentTime, params, 4);
				for(uint8_t a=0; a<4; a++)
				{
					reply[replyLen] = params[a];
					replyLen++;
				}
			}
			reply[replyLen] = 0xD;
			replyLen++;
		}
		else
		{
			convert.itox(can_msg.id, params, 8);
			reply[replyLen] = 'T';
			replyLen++;
			for(uint8_t a=0; a<8; a++)
			{
				reply[replyLen] = params[a];
				replyLen++;
			}
			memset(params,0,64);
			convert.itox(can_msg.len, params, 1);
			reply[replyLen] = params[0];
			replyLen++;
			for(uint8_t a=0; a < can_msg.len; a++)//now we will put out data bytes
			{
				memset(params,0,64);
				convert.itox(can_msg.data[a], params, 2);
				reply[replyLen] = params[0];
				replyLen++;
				reply[replyLen] = params[1];
				replyLen++;
			}
			if(isTimeStampEnabled == true)//if timestamp is enabled
			{
				uint16_t currentTime= timer.read_ms();
				memset(params,0,64);
				convert.itox(currentTime, params, 4);
				for(uint8_t a=0; a<4; a++)
				{
					reply[replyLen] = params[a];
					replyLen++;
				}
			}
			reply[replyLen] = 0xD;
			replyLen++;
		}
		for(uint8_t a = 0; a < replyLen; a++)//we send the reply
		{
			ftdi->putc(reply[a]);
		}
//		perfCounter++;//used for debug
	}
}

void SocketCAN::updateStatusByte()
{

	if(_interfaceNo == 1)
	{
		if((LPC_CAN1->GSR & 1) == 0)//Check RBS bit of GSR (Receive Buffer status)
		{
			convert.setBit(&CANStatus,0,0);
		}
		else
		{
			convert.setBit(&CANStatus,1,0);
		}
		if((LPC_CAN1->GSR & 2) == 0)//Check DOS bit of GSR (data Overrun)
		{
			convert.setBit(&CANStatus,0,3);
		}
		else
		{
			convert.setBit(&CANStatus,1,3);
		}
		if((LPC_CAN1->GSR & 4) == 0)//Check TBS bit of GSR (transmit buffer status)
		{
			convert.setBit(&CANStatus,0,1);
		}
		else
		{
			convert.setBit(&CANStatus,1,1);
		}
		if((LPC_CAN1->GSR & 0x40) == 0)//Check ES bit of GSR (error status)
		{
			convert.setBit(&CANStatus,0,2);
		}
		else
		{
			_canbus->reset();//reset the errors to check next time if they dont exist anymore
			convert.setBit(&CANStatus,1,2);
		}
		if((LPC_CAN1->ICR & 0x40) == 0)//Check ALI bit of ICR (Arbitration Lost Interrupt)
		{
			convert.setBit(&CANStatus,0,6);
		}
		else
		{
			_canbus->reset();//reset the errors to check next time if they dont exist anymore
			convert.setBit(&CANStatus,1,6);
		}
		if((LPC_CAN1->GSR & 0x80) == 0)//Check BS bit of GSR (Bus Status)
		{
			convert.setBit(&CANStatus,0,7);
		}
		else
		{
			_canbus->reset();//reset the errors to check next time if they dont exist anymore
			convert.setBit(&CANStatus,1,7);
		}
		if((LPC_CAN1->ICR & 0x20) == 0)//Check EPI bit of ICR (Error Passive Interrupt)
		{
			convert.setBit(&CANStatus,0,5);
		}
		else
		{
			_canbus->reset();//reset the errors to check next time if they dont exist anymore
			convert.setBit(&CANStatus,1,5);
		}
	}
	else
	{
		if((LPC_CAN2->GSR & 1) == 0)//Check RBS bit of GSR (Receive Buffer status)
		{
			convert.setBit(&CANStatus,0,0);
		}
		else
		{
			convert.setBit(&CANStatus,1,0);
		}
		if((LPC_CAN2->GSR & 2) == 0)//Check DOS bit of GSR (data Overrun)
		{
			convert.setBit(&CANStatus,0,3);
		}
		else
		{
			convert.setBit(&CANStatus,1,3);
		}
		if((LPC_CAN2->GSR & 4) == 1)//Check TBS bit of GSR (transmit buffer status)
		{
			convert.setBit(&CANStatus,0,1);
		}
		else
		{
			convert.setBit(&CANStatus,1,1);
		}
		if((LPC_CAN2->GSR & 0x40) == 0)//Check ES bit of GSR (error status)
		{
			convert.setBit(&CANStatus,0,2);
		}
		else
		{
			_canbus->reset();//reset the errors to check next time if they dont exist anymore
			convert.setBit(&CANStatus,1,2);
		}
		if((LPC_CAN2->ICR & 0x40) == 0)//Check ALI bit of ICR (Arbitration Lost Interrupt)
		{
			convert.setBit(&CANStatus,0,6);
		}
		else
		{
			_canbus->reset();//reset the errors to check next time if they dont exist anymore
			convert.setBit(&CANStatus,1,6);
		}
		if((LPC_CAN2->GSR & 0x80) == 0)//Check BS bit of GSR (Bus Status)
		{
			convert.setBit(&CANStatus,0,7);
		}
		else
		{
			_canbus->reset();//reset the errors to check next time if they dont exist anymore
			convert.setBit(&CANStatus,1,7);
		}
		if((LPC_CAN2->ICR & 0x20) == 0)//Check EPI bit of ICR (Error Passive Interrupt)
		{
			convert.setBit(&CANStatus,0,5);
		}
		else
		{
			_canbus->reset();//reset the errors to check next time if they dont exist anymore
			convert.setBit(&CANStatus,1,5);
		}
	}
}


