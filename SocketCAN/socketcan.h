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

#ifndef __SOCKETCAN_H__
#define __SOCKETCAN_H__


/*FTDI Serial*/
#define FTDI_TX P0_2
#define FTDI_RX P0_3
#define FTDI_BAUDRATE 1000000


/***CAN status bits*///
#define CAN_RX_FIFO_FULL 0
#define CAN_TX_FIFO_FULL 1
#define CAN_ERROR_WARNING 2
#define CAN_DATA_OVERRUN 3
#define CAN_ERROR_PASSIVE 5
#define CAN_ARBITRATION_LOST 6
#define CAN_BUS_ERROR 7


#define MAX_TIMESTAMP 0xEA5F //max value for timestamp


/*Status register*/
#define CAN_RX_FULL 	  	 0
#define CAN_TX_FULL 	  	 1
#define CAN_ERROR   	  	 2
#define CAN_DATA_OVERRUN  	 3
#define CAN_ERROR_PASSIVE 	 5
#define CAN_ARBITRATION_LOST 6
#define CAN_BUS_ERROR 		 7

#include "mbed.h"
#include "conversions.h"
#include "buttons.h"

class SocketCAN
{
	public:

		SocketCAN(CAN *canbus, uint8_t interfaceNo, uint8_t *buffer);//Constructor and Destructor
		~SocketCAN();

		void processCommand();//function to be called by the UART interrupt to process a command

		void commandQueue();

		void start(Buttons* buttons);//we need to pass a buttons object so we can stop the socketcan mode

		void CANInterrupt();//function used by autopoll

		bool setBaudrate(uint32_t baudRate);

		bool openInterface(bool listen = false);//listen will set the flag if the open operation was performed using L

		bool closeInterface();

		bool sendCANFrame(uint32_t ID, uint8_t len, uint8_t *data, CANFormat frameFormat = CANStandard, CANType frameType = CANData);

		void getCANFrame();//Used by the CAN interrupt, it simulates autopoll feature

		void updateStatusByte();//used to update the status byte. Called with a tick

		void resetTimerScheduler();//resets the timer for timestamp




  private:

	CAN* _canbus;

	uint8_t _interfaceNo;
	bool isSetup;//used to know if the interface has been setup so that the open command works
	bool isOpen;
	uint8_t *cBuffer;//buffer for commands. should be 4096 bytes in size
	uint32_t cBufferCurrentPointer;//to know where the buffer is pointing to
	uint32_t cBufferWritePointer;//to know where to write the next incoming requests
	uint32_t pendingRequests;//to store the number of pending requests
	bool listenOnly;//used to know if the interface is supposed to be in listen only mode
	bool isPolling;
	Serial* ftdi;//Serial port used by ftdi
	uint32_t CANStatus;//used to store the status of the CAN peripheral
	bool isTimeStampEnabled;//used to know if the time stamp should be used
	Timer timer;//used in timestamp;
	Ticker resetTimer; //used to reset the timer every 60 seconds
	Conversions convert;
	uint32_t perfCounter;//used only for test

};
#endif
