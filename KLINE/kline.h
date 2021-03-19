/*
* K-LINE (ISO 9141-2) LIBRARY
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

/*Bro-tips:
-Slow init will most likely have the ECU reply at 9600bps
-Fast init will most likely have the ECU reply at 10400bps
-KWP2000 uses CRC, so enable it
*/

#ifndef __KLINE_H__
#define __KLINE_H__

#include "mbed.h"
#include "buttons.h"

#define KLINE_DEFAULT_BYTE_DELAY 5 //delay between bytes being sent, in ms
#define KLINE_DEFAULT_READ_TIMEOUT 500 //since we are reading raw kline, we dont know how the packet length if specified (if it is), so we just wait a bit for no bytes being sent
#define KLINE_DEFAULT_BYTE_READ_TIMEOUT 15 //timeout for single byte when reading without knowing the length
#define KLINE_DEFAULT_SPEED 10400


class KLINEHandler
{
	public:

		KLINEHandler(Serial *kline, uint8_t interfaceNo);//Constructor and Destructor
		~KLINEHandler();

		bool slowInit(uint8_t address);
		bool write(uint8_t *request, uint32_t len, bool doCRC = true);
		uint32_t read(uint8_t *response, uint32_t len = 0, bool checkCRC = true);//when len is 0, we just wait until no more traffic
		void setTransmissionParameters(uint32_t doByteDelay = KLINE_DEFAULT_BYTE_DELAY, uint32_t readTimeaut = KLINE_DEFAULT_READ_TIMEOUT, uint32_t bTimeout = KLINE_DEFAULT_BYTE_READ_TIMEOUT );
	  void setBaudrate(uint32_t baudrate);
		void fastInit(uint8_t *initSequence, uint8_t len, bool doCRC = true);//initSequence contains the sequence that should be sent after the init
		uint16_t getByte();
		bool sendByte(uint8_t toSend);
		void ftdiPassthrough(Buttons* buttons);// Uses K-LINE 1 as an ftdi passthrough. Useful to emulate interfaces.
	
	
	private:
		Serial* _kline;
		uint32_t byteDelay;
		uint32_t byteTimeout;
		uint32_t readTimeout;
		uint32_t speed;
		uint8_t interface;
	
};

#endif
