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

#ifndef __PID_H__
#define __PID_H__




#include "mbed.h"
#include "TP.h"
#include "conversions.h"

class PIDHandler
{
	public:

		PIDHandler(CAN *canbus, uint32_t CANFreq = 500000);//Constructor and Destructor
		~PIDHandler();

		uint32_t PID7Request(uint8_t *reply);

		uint32_t PID3Request(uint8_t *reply);

		uint32_t PID10Request(uint8_t *reply);

		uint32_t PID4Request(uint8_t *reply);

	  private:

		CAN* _canbus;
		TPHandler* tp;//ISO-TP handler
		Ticker tick; //used to schedule timed requests
		void sendTesterPresent();
	};
	#endif
