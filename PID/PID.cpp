/*
* PID LIBRARY
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

#include "PID.h"

PIDHandler::PIDHandler(CAN *canbus, uint32_t CANFreq)
{
	_canbus=canbus;
	_canbus->frequency(CANFreq);
	tp = new TPHandler(_canbus);
}

PIDHandler::~PIDHandler()
{
	delete tp;
}



uint32_t PIDHandler::PID7Request(uint8_t *reply)//request Emission related fault codes during current and last cycle. The third byte (starting from 1) is the number of DTCs. Each DTC is two bytes AS IS, not converted to DEC. Add letter P to each code
{
	uint8_t request[3]={0x7};
	return tp->requestResponseClient(request, 1, reply);
}


uint32_t PIDHandler::PID10Request(uint8_t *reply)//request all permanent fault codes. The third byte (starting from 1) is the number of DTCs. Each DTC is two bytes AS IS, not converted to DEC. Add letter P to each code
{
	uint8_t request[3]={0xA};
	return tp->requestResponseClient(request, 1, reply);
}

uint32_t PIDHandler::PID4Request(uint8_t *reply)//request clear all fault codes. The third byte (starting from 1) is the number of DTCs. Each DTC is two bytes AS IS, not converted to DEC. Add letter P to each code
{
	uint8_t request[3]={0x4};
	return tp->requestResponseClient(request, 1, reply);
}

uint32_t PIDHandler::PID3Request(uint8_t *reply)//request all Emission related fault codes. The third byte (starting from 1) is the number of DTCs. Each DTC is two bytes AS IS, not converted to DEC. Add letter P to each code
{
	uint8_t request[3]={0x3};
	return tp->requestResponseClient(request, 1, reply);
}
