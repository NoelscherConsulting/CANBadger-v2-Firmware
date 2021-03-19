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

#ifndef __TP_H__
#define __TP_H__

//Timeouts in milliseconds
#define TP_DEFAULT_REQUEST_TIMEOUT 1000 //1 second for each seems enough
#define TP_DEFAULT_RESPONSE_TIMEOUT 1000




#include "mbed.h"
#include "canbadger_CAN.h"

//flow control
#define TP_CONTINUE_TO_SEND 0x0
#define TP_WAIT 0x01
#define TP_OVERFLOW 0x02
#define TP_BLOCK_SIZE_UNLIMITED 0x00
#define TP_NO_WAIT_TIME 0x00



class TPHandler
{
	public:

		TPHandler(CAN *canbus);//Constructor and Destructor
		~TPHandler();

    /** sends an ISO-TP request and returns the response, making the CANBadger behave like a client (tester)
       @param CAN *canbus is the CAN object to be used for the request and response
			 @param uint8_t *rqst is a pointer to the array where the request is stored
			 @param uint32_t len is the length of the request
			 @param frameFormat is the format of the frame (CANStandard or CANExtended)
			 @param uint8_t *response is a pointer to the array where the response is stored			
			 @param bool useFullFrame determines if we will add bs bytes (padding bytes) to make all frames have 8 bytes length
			 @param bsByte is the value that should be used for padding	
			 @param variant determines the UDS variation from the standard
							As of now, the following variants exist:
								-0 = Standard addressing (default)
								-1 = Extended addressing
			 @param extendedID is the extended ID, and is only used by variant 1

			 @return upper nibble contains error code, lower nibble contains the length of the response. If there was an error, length will be 0
		*/
	
		void setTransmissionParameters(uint32_t sourceID, uint32_t targetID, CANFormat doFrameFormat =CANStandard, bool doUseFullFrame = true, uint8_t doBsByte = 0, uint8_t doVariant = 0, bool useFilters = true);

		uint32_t read(uint8_t *response);
	
		bool write(uint8_t *request, uint16_t len);		
		
		float getSeparationTime(uint8_t value);//returns the separation time in seconds, used by flow control

		void setTimeouts(uint32_t request, uint32_t response);

		 /** sends an ISO-TP request and returns the response, making the CANBadger behave like a client (tester)
					 @param uint8_t *rqst is a pointer to the array where the request is stored
					 @param uint32_t len is the length of the request
					 @param uint8_t *response is a pointer to the array where the response is stored
					 @return upper nibble contains error code, lower nibble contains the length of the response. If there was an error, length will be 0
		*/
		uint32_t requestResponseClient(uint8_t *rqst, uint16_t len, uint8_t *response, bool ignoreACK = false);

		void disableFilters();

		void enableFilters();

		void resumeSession(uint32_t localID, uint32_t remoteID);

  private:
			
	CAN* _canbus;
	CANbadger_CAN* _cb;
	CANFormat frameFormat;
	bool useFullFrame; 
	uint8_t bsByte; 
	uint8_t variant;
	uint32_t ownID;
	uint32_t rID;
	uint32_t requestTimeout;
	uint32_t responseTimeout;
	bool areFiltersActive;//to know if filters are active


};
#endif
