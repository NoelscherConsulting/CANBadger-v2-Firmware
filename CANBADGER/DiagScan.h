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

#ifndef __DIAGSCAN_H__
#define __DIAGSCAN_H__

#include "mbed.h"
#include "UDSCAN.h"
#include "tp20.h"
#include "TP.h"
#include "kwp2k_can.h"
#include "kwp2k_tp20.h"
#include "buttons.h"
#include "fileHandler.h"
#include "canbadger_CAN.h"

class DiagSCAN
{
	public:

		DiagSCAN(CAN *canbus);//Constructor and Destructor
		~DiagSCAN();

		void setTransmissionParameters(uint32_t sourceID, uint32_t targetID, CANFormat doFrameFormat =CANStandard, bool doUseFullFrame = true, uint8_t doBsByte = 0);

		bool setCommsType(uint8_t commsType);//1 for UDS, 2 for TP2.0, 3 for KWP2K over CAN.

		bool setCANSpeed(uint32_t speed);

       /** Creates a list of detected UDS comm channels. The list will be arranged with the following format:
        *
        * 	Pairs of uint32_t in the array where if both are bigger than 0, it means that it is a channel, and if the second one is 0, it means it is potential UDS but not verified

			@param *IDList is the array where the list of found IDs will be stored

			@return 0 if no UDS IDs were found, the number of UDS comms found if any is found, and 0xFFFFFFFF if no CAN traffic was detected and 0xFFFFFFFE if there was an error with the SD

		*/

		uint32_t ScanActiveUDSIDsFromLog(uint32_t *IDList, const char *fileName, FileHandler *sd, Buttons *buttons);

		bool scanTP20ID(uint32_t channel, uint8_t ecuID);//provide a channel as input and the ECU id you want to check

		bool scanTP20Channel(uint32_t channel);

		uint32_t scanUDSID(uint32_t targetID, uint8_t sessionType = UDS_DIAGNOSTIC_DEFAULT_SESSION, uint32_t tTimeout= 30, uint8_t variantType = 0);//used to check if an ID supports UDS. to be used in a loop. returns 0 on no response, Id that replied + MSB set if positive, Id + second MSB set if reply but error

	private:

		CAN* _canbus; //can object to be used
		CANFormat _frameFormat; //Frame format, CANStandard or CANExtended
		uint8_t _commsType;//0 for Autodetect, 1 for UDS, 2 for TP2.0, 3 for KWP2K over CAN. Autodetect can be slow
		bool _useFullFrame;//wether to use a full frame
		uint8_t _paddingByte;//padding to be used in case of using a full frame
		uint32_t txID;//CAN id to be used for TX
		uint32_t rxID;//CAN ID to be used for RX

};
#endif

