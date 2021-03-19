/*
* UDS LIBRARY USING ISO-TP (ISO 15765)
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


/************************************
 * IMPORTANT!!: USE TMPBUFFER TO PASS YOUR ARGUMENTS AS THE BUFFER NEEDS TO BE 4096 BYTES LONG, NO MATTER HOW MANY BYTES YOU WANT TO WRITE
 ************************************/

#ifndef __UDSCAN_H__
#define __UDSCAN_H__



#define UDS_FUNCTIONAL_BROADCAST_ID 0x7DF
#define UDS_FUNCTIONAL_RESPONSE_START 0x7E8
#define UDS_FUNCTIONAL_RESPONSE_COUNT 8
#define UDS_ARBITRATION_ID_OFFSET 0x8
#define UDS_RESPONSE_OFFSET 0x40



#include "mbed.h"
#include "uds_sids.h"
#include "TP.h"
#include "conversions.h"

class UDSCANHandler
{
	public:

		UDSCANHandler(CAN *canbus, uint32_t CANFreq = 500000);//Constructor and Destructor
		~UDSCANHandler();

    /** sends an UDS request and returns the response, making the CANBadger behave like a client (tester)
			 @param uint8_t *rqst is a pointer to the array where the request is stored
			 @param uint32_t len is the length of the request
			 @param uint8_t *response is a pointer to the array where the response is stored			
			 @return upper nibble contains error code, lower nibble contains the length of the response. If there was an error, length will be 0
		*/
		uint32_t requestResponseClient(uint8_t *rqst, uint16_t len, uint8_t *response, bool ignoreACK = false);
	

		uint32_t DiagSessionControl(uint8_t SessionType = UDS_DIAGNOSTIC_DEFAULT_SESSION);

		uint32_t DiagSessionStartup(uint8_t SessionType, uint8_t *response_buffer);
	
    /** Sets the UDS transmission parameters
			 @param uint32_t sourceID is the tester address
			 @param uint32_t targetID is the ECU address
			 @param CANFormat doFrameFormat is the type of frame (CANStandard or CANExtended)
			 @param bool doUseFullFrame determines if we will add padding bytes to each transmission
			 @param uint8_t doBsByte is the value of the padding bytes
			 @param uint8_t doVariant determines if we use standard UDS addressing (0), or extended UDS addressing(1).	
		*/	

		void setTransmissionParameters(uint32_t sourceID, uint32_t targetID, CANFormat doFrameFormat =CANStandard, bool doUseFullFrame = true, uint8_t doBsByte = 0, uint8_t doVariant = 0, bool useFilters = true);

		uint32_t read(uint8_t *response, bool ignoreACK = false);//ignoreack is used when some bootloaders will just start spitting data right after sending the wait command
	
		bool write(uint8_t *request, uint16_t len);
	
		bool sessionStatus();
		
		void setTimeouts(uint32_t request, uint32_t response);
		
		uint32_t readDataByID(uint16_t dataType, uint8_t *response);

		uint32_t writeDataByID(uint16_t dataType, uint8_t *data, uint32_t payLength, uint8_t *response);
		
		uint32_t requestSeed(uint8_t *payload, uint32_t paylength, uint8_t *response); //Requests a seed. You must add the level and any additional data if required
		
		uint32_t requestSeed(uint8_t level, uint8_t *response); //Requests a seed. You must add the level and any additional data if required
		
		uint32_t sendKey(uint8_t level, uint8_t *keyBytes, uint32_t keyLength, uint8_t *response); //Sends a Key longer than 4 bytes as response to a Seed.
		
		uint32_t sendKey(uint8_t level, uint32_t key, uint8_t *response); //Sends a Key as response to a Seed.
		
		uint32_t routineControl(uint8_t requestType, uint16_t routineType, uint8_t *parameters, uint32_t paramLength, uint8_t *response);
		
		uint32_t requestDownload(uint8_t dataFormatID, uint64_t memAddress, uint32_t memSize, uint8_t *response);
		
		uint32_t requestUpload(uint8_t dataFormatID, uint64_t memAddress, uint32_t memSize, uint8_t *response);
		
		uint32_t transferData(uint8_t blockNumber, uint8_t *data, uint16_t length, uint8_t *response, bool ignoreACK = false);
		
		uint32_t requestTransferExit(uint8_t *params, uint16_t paramLen, uint8_t *response);

		uint32_t getBlockSize(uint8_t *response);//just pass the reply from request upload/download and it will return the block size

		uint32_t ECUReset(uint8_t resetType, uint8_t *response);
		
		uint32_t communicationControl(uint8_t controlType, uint8_t communicationType, uint8_t *response, uint16_t nodeID = 0);
		
		uint32_t readMemoryByAddress(uint64_t memAddress, uint32_t memSize, uint8_t *response);
		
		uint32_t writeMemoryByAddress(uint64_t memAddress, uint32_t memSize, uint8_t *request);//will return the response in the request array
		
		uint32_t clearDTCs(uint32_t groupOfDTC);//0xFFFFFF for all groups
		
		uint32_t readNumberOfDTCs(uint8_t *reply, uint8_t dtcMask =0xFF);//0xFF means all

		uint32_t readDTCs(uint8_t *reply, uint8_t dtcMask =0xFF);//0xFF means all

		void checkDTC(uint32_t DTCCode, char *destination);//DTCCode should contain the first three bytes of DTC and the status bytes, so basically as it is read

		void checkError(uint8_t errorCode, uint8_t *destination);
		
		void setSessionStatus(bool setSession);//forces the UDS stack to be in a session or to terminate it. Used by Security Hijack

		void endSession();

		void disableFilters();

		void enableFilters();


  private:
			
	CAN* _canbus;
	TPHandler* tp;//ISO-TP handler
	uint32_t ownID;//used only for filtering
	uint32_t rID;//used only for filtering
	Ticker tick; //used to schedule TesterPresent
	uint8_t inSession;//0 means no session, 1 means in session
	bool areFiltersActive;//to know if filters are active
	void sendTesterPresent();
};
#endif
