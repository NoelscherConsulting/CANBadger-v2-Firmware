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

#ifndef __TP20_H__
#define __TP20_H__


#define TP20_DEFAULT_REQUEST_TIMEOUT 1000
#define TP20_DEFAULT_RESPONSE_TIMEOUT 2000
#define TP20_DEFAULT_SESSION_TIMEOUT 3000

//ACK/NAK/etc for TP2.0
#define TP20_ACK_FOLLOW 0x00
#define TP20_ACK_LAST 0x10
#define TP20_NOACK_FOLLOW 0x20
#define TP20_NOACK_LAST 0x30
#define TP20_ACK 0xB0
#define TP20_NACK 0x90
#define TP20_BROADCAST_REQUEST 0x23
#define TP20_BROADCAST_RESPONSE 0x24

//Channel setup
#define TP20_SETUP_CHANNEL 0xC0
#define TP20_SETUP_CHANNEL_OK 0xD0
#define TP20_CONNECTION_SETUP 0xA0
#define TP20_CONNECTION_ACK 0xA1
#define TP20_CONNECTION_TEST 0xA3
#define TP20_CONNECTION_BREAK 0xA4
#define TP20_CONNECTION_DISCONNECT 0xA8
#define TP20_APPLICATION_TYPE_NOT_SUPPORTED 0xD6
#define TP20_APPLICATION_TYPE_TEMPORARILY_NOT_SUPPORTED 0xD7
#define TP20_TEMPORARILY_NO_FREE_RESOURCES 0xD8
#define TP20_BROADCAST_ID 0xF0

//Timing parameters
#define TP20_NO_TIMEOUT 0xFF

//Application types
#define TP20_APP_SD_DIAG 0x01
#define TP20_APP_INFOTAIMENT 0x10
#define TP20_APP_APPLICATION_PROTOCOL 0x20
#define TP20_APP_WFS_WIV 0x21

#include "mbed.h"
#include "canbadger_CAN.h"
#include "conversions.h"

class TP20Handler
{
	public:

		TP20Handler(CAN *canbus, uint8_t interface = 1);//Constructor and Destructor
		~TP20Handler();

    /** sends an TP2.0 request and returns the response, making the CANBadger behave like a client (tester)
			 @param uint8_t *rqst is a pointer to the array where the request is stored
			 @param uint32_t len is the length of the request
			 @param uint8_t *response is a pointer to the array where the response is stored			
			 @return upper nibble contains error code, lower nibble contains the length of the response. If there was an error, length will be 0
		*/
		uint32_t requestResponseClient(uint8_t *rqst, uint16_t len, uint8_t *response);
	
		void setTransmissionParameters(bool useFilters = true);

		uint32_t read(uint8_t *response);
		
		bool write(uint8_t *request, uint8_t len, uint8_t doACK = true);
		
		void setTimeouts(uint32_t request, uint32_t response);
	
		//will return 1 if all was ok, 0 if no communication, or the error code otherwise
		uint8_t channelSetup(const uint32_t requestAddress, uint8_t target_ID, uint8_t appType = TP20_APP_SD_DIAG);
		
		bool stillInSession();
		
		bool closeChannel(bool silent = false);

		void disableFilters();

		void enableFilters();

		float decodeTimeout(uint8_t timeByte);//returns the decoded time in seconds ready to be used with wait

		void checkSessionStatus();

		uint8_t getCurrentCounter();

		void resumeSession(uint32_t localID, uint32_t remoteID, uint8_t currentCount);




  private:
			
	CAN* _canbus;
	CANbadger_CAN* _cb;
	uint32_t ownID;
	uint32_t rID;
	Ticker tick;//to check the session status
	uint8_t frameCount;
	uint32_t requestTimeout;
	uint32_t responseTimeout;
	bool TPSendACK(uint8_t ackCounter);
	void increaseFrameCounter();
	uint8_t TPGetACK();
	uint8_t inSession;
	void keepChannelAlive();
	void sendCA(uint8_t wat);
	bool areFiltersActive;//to know if filters are active
	bool needFilters;
	uint8_t blockSize;
	float waitTime;
	float sessionTimeout;
	uint8_t _interface;
};
#endif
