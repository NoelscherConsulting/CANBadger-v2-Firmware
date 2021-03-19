/*
* KWP2000 on CAN LIBRARY USING TP
* Copyright (c) 2020 Noelscher Consulting GmbH
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

#ifndef __KWP2K_TP20_H__
#define __KWP2K_TP20_H__


#define KWP2K_CAN_DEFAULT_REQUEST_TIMEOUT 500
#define KWP2K_CAN_DEFAULT_RESPONSE_TIMEOUT 1000


#define KWP2K_ARBITRATION_ID_OFFSET 0x8
#define KWP2K_RESPONSE_OFFSET 0x40



#include "mbed.h"
#include "kwp2000_sids.h"
#include "tp20.h"
#include "conversions.h"
#include "kwp2k_common.h"

class KWP2KTP20Handler
{
	public:

		KWP2KTP20Handler(CAN *canbus, uint8_t interface = 1, uint32_t CANFreq = 500000);//Constructor and Destructor
		~KWP2KTP20Handler();

    /** sends an UDS request and returns the response, making the CANBadger behave like a client (tester)
			 @param uint8_t *rqst is a pointer to the array where the request is stored
			 @param uint32_t len is the length of the request
			 @param uint8_t *response is a pointer to the array where the response is stored
			 @return upper nibble contains error code, lower nibble contains the length of the response. If there was an error, length will be 0
		*/
		uint32_t requestResponseClient(uint8_t *rqst, uint16_t len, uint8_t *response, bool ignoreACK = false);

		uint32_t startComms(uint8_t *response, uint8_t SessionType = KWP_DEFAULT_SESSION);

		bool stopComms();

    /** Sets the UDS transmission parameters
			 @param uint32_t sourceID is the tester address
			 @param uint32_t targetID is the ECU address
			 @param CANFormat doFrameFormat is the type of frame (CANStandard or CANExtended)
			 @param bool doUseFullFrame determines if we will add padding bytes to each transmission
			 @param uint8_t doBsByte is the value of the padding bytes
		*/


		uint8_t channelSetup(const uint32_t requestAddress, uint8_t target_ID, uint8_t appType = TP20_APP_SD_DIAG);

		bool closeChannel();

		void setTransmissionParameters(bool useFilters = true);

		uint32_t read(uint8_t *response, bool ignoreACK = false);//ignoreack is used when some bootloaders will just start spitting data right after sending the wait command

		bool write(uint8_t *request, uint16_t len);

		bool sessionStatus();

		void setTimeouts(uint32_t request, uint32_t response);

		uint32_t readDataByLocalID(uint8_t dataType, uint8_t *response);

		uint32_t writeDataByLocalID(uint8_t dataType, uint8_t *data, uint32_t payLength, uint8_t *response);

		uint32_t readDataByCommonID(uint16_t dataType, uint8_t *response);

		uint32_t writeDataByCommonID(uint16_t dataType, uint8_t *data, uint32_t payLength, uint8_t *response);

		uint32_t readMemoryByAddress(uint32_t memAddress, uint8_t memSize, uint8_t *response, uint8_t transmissionMode = KWP_TRANSMISSION_MODE_SINGLE);

		uint32_t writeMemoryByAddress(uint8_t *data, uint32_t memAddress, uint8_t memSize, uint8_t *response);

		uint32_t requestDownload(uint8_t *data, uint32_t payLength, uint8_t *response);

		uint32_t requestUpload(uint8_t *data, uint32_t payLength, uint8_t *response);

		uint32_t transferData(uint8_t *data, uint32_t payLength, uint8_t *response);

		uint32_t requestTransferExit(uint8_t *data, uint32_t payLength, uint8_t *response);

		uint32_t clearDTCs(uint8_t *data, uint32_t payLength, uint8_t *response);

		uint32_t readDTCs(uint8_t *data, uint32_t payLength, uint8_t *response);

		uint8_t getDTCList(uint8_t *source, uint8_t *destination);//provide the list returned by readDTCs and it will return the number of DTCs and a list in destination. each DTC is two bytes

		void checkDTC(uint16_t DTCCode, uint8_t statusByte, char *destination);//to have some verbosity on the DTC error codes

		uint32_t readECUID(uint8_t idType, uint8_t *response);

		uint32_t requestSeed(uint8_t *payload, uint32_t paylength, uint8_t *response); //Requests a seed. You must add the level and any additional data if required

		uint32_t requestSeed(uint8_t level, uint8_t *response); //Requests a seed. You must add the level and any additional data if required

		uint32_t sendKey(uint8_t level, uint8_t *keyBytes, uint32_t keyLength, uint8_t *response); //Sends a Key as response to a Seed.

		uint32_t sendKey(uint8_t level, uint32_t key, uint8_t *response); //Sends a Key as response to a Seed.

		uint32_t startRoutineByLocalID(uint8_t routineID, uint8_t *data, uint32_t payLength, uint8_t *response);

		uint32_t stopRoutineByLocalID(uint8_t routineID, uint8_t *data, uint32_t payLength, uint8_t *response);

		uint32_t requestRoutineResultsByLocalID(uint8_t routineID, uint8_t *response);

		uint32_t startRoutineByAddress(uint32_t address, uint8_t *params, uint32_t payLength, uint8_t *response);

		uint32_t stopRoutineByAddress(uint32_t address, uint8_t *params, uint32_t payLength, uint8_t *response);

		uint32_t requestRoutineResultsByAddress(uint32_t address, uint8_t *response);

		void checkError(uint8_t errorCode, uint8_t *destination);

		uint32_t ECUReset(uint8_t resetType, uint8_t *response);

		void endSession();

		void disableFilters();

		void enableFilters();

		uint8_t silentCloseChannel();//stops the session without channel disconnect. useful for loaders. returns the current counter value

		void attachToSession(uint32_t localID,uint32_t remoteID, uint8_t tpCounter);

  private:

	CAN* _canbus;
	TP20Handler* tp;//TP2.0 handler
	Ticker tick; //used to schedule TesterPresent
	uint8_t inSession;//0 means no session, 1 means in session
	void sendTesterPresent();
	uint32_t ownID;
	uint32_t rID;
	bool areFiltersActive;//to know if filters are active

};
#endif
