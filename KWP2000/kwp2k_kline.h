/*
* KWP2000 LIBRARY USING KLINE (ISO 9141-2)
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

#ifndef __KWP2K_KLINE_H__
#define __KWP2K_KLINE_H__


//#define KWP2K_KLINE_DEFAULT_REQUEST_TIMEOUT 500 //not used yet, seems to be no need as it is hardcoded as 1s in the kline lib
//#define KWP2K_KLINE_DEFAULT_RESPONSE_TIMEOUT 1000
#define KWP2K_KLINE_DEFAULT_TESTER_ADDRESS 0xF1
#define KWP2K_KLINE_DEFAULT_TARGET_ADDRESS 0x01 //This is typically the engine control unit
#define KWP2K_KLINE_NO_ADDRESS 0
#define KWP2K_KLINE_PHYSICAL_ADDRESS 0x80
#define KWP2K_KLINE_FUNCTIONAL_ADDRESS 0xC0
#define KWP2K_KLINE_DEFAULT_ADDRESS_TYPE 1 //we will use physical address as default


#define KWP_SLOW 0
#define KWP_FAST 1


#include "mbed.h"
#include "kwp2000_sids.h"
#include "kline.h"
#include "conversions.h"

class KWP2KKLINEHandler
{
	public:

		KWP2KKLINEHandler(KLINEHandler *kline);//Constructor and Destructor
		~KWP2KKLINEHandler();

    /** sends an KwP2K request and returns the response, making the CANBadger behave like a client (tester)
			 @param uint8_t *rqst is a pointer to the array where the request is stored
			 @param uint32_t len is the length of the request
			 @param uint8_t *response is a pointer to the array where the response is stored			
			 @return upper nibble contains error code, lower nibble contains the length of the response. If there was an error, length will be 0
		*/
		uint32_t requestResponseClient(uint8_t *rqst, uint16_t len, uint8_t *response);
	
		
	
    /** Sets the UDS transmission parameters
			 @param uint32_t sourceID is the tester address
			 @param uint32_t targetID is the ECU address
			 @param CANFormat doFrameFormat is the type of frame (CANStandard or CANExtended)
			 @param bool doUseFullFrame determines if we will add padding bytes to each transmission
			 @param uint8_t doBsByte is the value of the padding bytes
			 @param uint8_t doVariant determines if we use standard UDS addressing (0), or extended UDS addressing(1).	
		*/	

		void setTransmissionParameters( uint32_t targetID, uint32_t sourceID = KWP2K_KLINE_DEFAULT_TESTER_ADDRESS);
		
    /** Sets the KWP2000 addressing scheme.
			 0 = No addressing
			 1 = Physical Address
			 2 = Functional Address
			 @param uint8_t addressing is the value according to the above table
		*/	

		void setAddressingType(uint8_t addressing);//used to set the type of addressing to be used

		uint32_t read(uint8_t *response);
	
		bool write(uint8_t *request, uint8_t len);
	
		bool sessionStatus();
		
    /** Sets the KLINE transmission timeouts
			 @param uint32_t doByteDelay specifies the delay between transmitting each byte
			 @param uint32_t byteTimeout is how long to wait for an incoming transmission to end, or for RX to timeout meaning the transmission is complete
			 @param uint32_t readTimeaut is how long to wait for the beginning of an incoming transmission before it times out AKA no response
		*/			
		
		uint32_t readECUID(uint8_t opt, uint8_t *buffer);//Retrieves ECU info in fast mode, returns the size of the reply
		
		void setTimeouts(uint32_t doByteDelay, uint32_t byteTimeout = KLINE_DEFAULT_BYTE_READ_TIMEOUT, uint32_t readTimeaut = KLINE_DEFAULT_READ_TIMEOUT);
		
		uint32_t readDataByLocalID(uint8_t dataType, uint8_t *response);
		
		uint32_t writeDataByLocalID(uint8_t dataType, uint8_t *data, uint8_t payLength, uint8_t *response);
		
		uint32_t requestSeed(uint8_t *payload, uint8_t paylength, uint8_t *response); //Requests a seed. You must add the level and any additional data if required
		
		uint32_t requestSeed(uint8_t level, uint8_t *response); //Requests a seed. You must add the level
		
		uint32_t sendKey(uint8_t *payload, uint32_t paylength, uint8_t *response); //Sends a Key as response to a Seed.
		
		uint32_t sendKey(uint8_t level, uint32_t key, uint8_t *response); //Sends a Key as response to a Seed.
		
		bool readMemByAddress(uint32_t address, uint8_t howMuch, uint8_t *whereTo);
		
		bool writeMemByAddress(uint32_t address, uint8_t howMuch, uint8_t *whereFrom);
		
		bool startRoutineByLocalID(uint8_t routineID);
		
		bool startRoutineByLocalID(uint8_t routineID, uint8_t *params, uint8_t len);
		
		uint8_t requestRoutineResultsByLocalID(uint8_t routineID, uint8_t *whereTo);
		
		bool stopRoutineByLocalID(uint8_t routineID);
		
		bool startDiagSession(uint8_t sub);
		
		bool stopDiagSession();
		
		bool startComms(uint8_t initType = KWP_FAST);//init type is either KWP_SLOW or KWP_FAST
		
		bool stopComms();
		
		
		
		


		/**  Requests a data download from the ECU (read ECU flash), returns the block length accepted by ECU
		
			 @param uint8_t dataFormatID is the data format, being 0x00 no encryption or compression. The rest are manufacturer specific
			 @param uint32_t memAddress is the memory address you want to upload to. It is only 24 bit
			 @param uint32_t memSize is the size of the uncompressed file to be transferred
			 @return the max block size per transfer
			 
		*/		

		uint32_t requestDownload(uint32_t memAddress, uint32_t memSize, uint8_t dataFormatID);
		
		/**  Requests a data upload to the ECU (write ECU flash), returns the block length accepted by ECU
		
			 @param uint8_t dataFormatID is the data format, being 0x00 no encryption or compression. The rest are manufacturer specific
			 @param uint32_t memAddress is the memory address you want to upload to. It is only 24 bit
			 @param uint32_t memSize is the size of the uncompressed file to be transferred
			 @return the max block size per transfer
			 
		*/		
		
		uint8_t requestUpload(uint32_t memAddress, uint32_t memSize = 0, uint8_t dataFormatID = 0);
		
		/**  Transfer Data service, but used when you are Downloading (reading) from an ECU. 
		
			 @param uint8_t transferType determines and how many bytes of address need to be added. 0 for none.
			 @param uint32_t address is the address where you want to read from. used by some ECUs
			 @param uint8_t *whereTo is pretty obvious
			 @return the ammount of bytes that were read, DATA only (without header, ready be written to a file).
			 
		*/			
		
		uint32_t transferDataRead(uint8_t transferType, uint32_t address, uint8_t *whereTo ); 

		/**  Transfer Data service, but used when you are Uploading (writing) to an ECU. 
		
			 @param uint8_t *whereFrom is the buffer that contains the data to be written
			 @param uint8_t howMuch is the number of bytes. Have in mind that you need to account on addressing bytes and so on, so you wont always be able to transfer 0xFE bytes
			 @param uint8_t transferType determines and how many bytes of address need to be added. 0 for none.
			 @param uint32_t address is the address where you want to write to. used by some ECUs
			 @return the ammount of bytes that were read, DATA only (without header, ready be written to a file).
			 
		*/	

		bool transferDataWrite(uint8_t *whereFrom, uint8_t howMuch, uint8_t transferType = 0, uint32_t address = 0);
		
		void setSpeed(uint32_t baudRate);


		uint32_t ECUReset(uint8_t resetType, uint8_t *response);


		uint32_t communicationControl(uint8_t controlType, uint8_t communicationType, uint8_t *response, uint16_t nodeID = 0);
		
		void parseKeyByte(uint8_t keyByte);


		void endSession();

  private:
			
	KLINEHandler* _kline;
	Ticker tick; //used to schedule TesterPresent
	uint8_t inSession;//0 means no session, 1 means in session
	void sendTesterPresent();
	uint8_t testerAddress;
	uint8_t targetAddress;
	uint8_t addressType;
	bool AL0;//used to store the AL0 info on startDiag
	bool AL1;
	bool HB0;
	bool HB1;
	bool TP0;
	
//	uint32_t requestTimeout;
//	uint32_t responseTimeout;

};
#endif
