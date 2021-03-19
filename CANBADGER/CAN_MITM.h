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
MITM loads rules from /MITM/rules.txt and allocates them in RAM.
An index is allocated in BSBuffer (IRAM) containing the offsets in XRAM for the rules.
The structure is as follows:
IRAM index:
	-Target ID (4 bytes)
	-Rule offset in XRAM (3 bytes)

XRAM Rules:
	-Condition bytes (2 bytes)
	-Target payload (8 bytes)
	-Action bytes (2 bytes)
	-Action payload (8 bytes)

Rules are consecutively stored in XRAM, using 0xFFFF in the Condition bytes to indicate that there are no more rules following.
*/

#ifndef __CAN_MITM_H__
#define __CAN_MITM_H__

#include "mbed.h"
#include "SER23LC1024.h"
#include "conversions.h"
#include "canbadger.h"


class CANbadger;


class CAN_MITM
{
	public:

				CAN_MITM(CANbadger *canbadger, CAN *canbus1, CAN *canbus2, CANFormat format, uint8_t *BSBuffr, Ser23LC1024 *ram, DigitalIn *backButton);

				~CAN_MITM();

				void doMITM();

				uint32_t tableLookUp(uint32_t canID);

				uint32_t allocRAM(uint32_t ttargetID, uint32_t tOffset);

				bool addRule(uint32_t offset, uint32_t cType, uint8_t *tPayload, uint32_t Action, uint8_t *aPayload);

				uint32_t getLastIDEntryOffset();

				bool checkRule(uint8_t busno, uint32_t offset, uint32_t ID, uint8_t *data, uint8_t leng);

				bool applyRule(uint8_t busno, uint32_t ID, uint8_t *data, uint8_t leng, uint8_t *tmpbf);

	private:

	CANbadger* canbadger;
	CAN* _canbus1;
	CAN* _canbus2;
	Ser23LC1024* _ram;
	CANFormat frameFormat;
	DigitalIn* _backButton;
	uint8_t* BSBuffer;
	Conversions converti;

};







#endif
