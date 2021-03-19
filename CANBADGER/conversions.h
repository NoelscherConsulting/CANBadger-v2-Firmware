/*
* Conversions library
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

#ifndef __CONVERSIONS_H__
#define __CONVERSIONS_H__

#include "mbed.h"
#include "atoh.h"
#include "string.h"
#include "USBSerial.h"
#include <stdlib.h>
#include <ctype.h>

class Conversions
{
    public:

        Conversions(); //Constructor
		
				~Conversions(); //Destructor

		/**
            Returns an uint32_t given an offset by reading four consecutive bytes and concatenating them. Does not check for separators such as line termination.
            @param offset is the adress where to start reading.
            @param *data is the array to read from
            @return the uint32_t value read
        */
				uint32_t getUInt32FromData(uint32_t offset, char *data);//Provide a data array and an offset to start from that array. Returns the UINT32 value read from there.

		/**
            Converts an uint64_t to ASCII Decimal
            @param i is the uint64_t.
            @param *s is the array to write to
			@param digits is how many digits of that number to write
        */
				void itoa(uint64_t i, char *s, uint8_t digits = 8); //Interger to ASCII Decimal
		
		/**
            Converts an uint64_t to ASCII Hex
            @param i is the uint64_t.
            @param *s is the array to write to
			@param digits is how many digits of that number to write
        */
				void itox(uint64_t i, char *s, uint8_t digits = 8); //Interger to ASCII Hex

		/**
            a more lightweight version of strlen
            @param *input is the array to check the length for
			@return the uint32_t length
        */
				uint32_t getArrayLength(char *input);//Returns the length of a String array.

		/**
            Returns the float value from a given array
            @param *s is the array containing the hex representation of the float
						@param position is position of the data to be grabbed (first number, second number, etc...)
						@param separator is the char to consider as a separator
						@return the double
        */
				double grabFloatValue(char *s, uint8_t position, char separator);//Returns the float value from a given array

		/**
            Returns a hex value from an ASCII string based on its position in an array. Checks for separators
            @param *s is the array containing the data
						@param position is position of the data to be grabbed (first number, second number, etc...)
						@param separator is the char to consider as a separator
						@return the uint32_t value of the number
        */
				uint32_t grabHexValue(char *s, uint8_t position, char separator);//Returns the hex value from a given array.

		/**
            Converts a line from the CANBADGER log format into a RAW CAN frame
            @param *tim is the timestamp of the frame
						@param *tID is the CAN ID of the frame
						@param *tlen is the length of the frame in bytes
						@param *tdata is the data contained in the frame
						@return true if it was able to parse the line
        */
				bool lineToRawFrame(char *s, uint32_t *tim, uint32_t *tID, uint8_t *tlen, uint8_t *tdata);//

		/**
            Converts usb serial (user) ascii hex input to an uint32_t
            @param USBSerial *input is the usb serial object
						@return uint32_t converted number from hex user input 
        */				
				uint32_t inputToHex(USBSerial *input);

		/**
            Converts usb serial (user) ascii decimal input to an uint32_t
            @param USBSerial *input is the usb serial object
						@return uint32_t converted number from decimal user input 
        */				
				uint32_t inputToDec(USBSerial *input);

		/**
            Converts an array ascii hex input to an uint32_t
            @param *input is the array containing the characters
            @paran len is how many digits
						@return uint32_t converted number from ascii hex
        */	
				uint32_t arrayToHex(char *input, uint8_t len);

		/**
            gets the bit number of a value
            @param *b is the value to retrieve the bit from
						@param bitNumber is the number of the bit you want to retrieve (from 0 to 31)
						@return true if bit is 1, false if bit is 0
        */					
				bool getBit(uint32_t b, uint8_t bitNumber);

		/**
            sets the bit number of a value
						@param *whereTo is the variable whose bit you want to change. It needs to be passed as an address (using the & operand)
            @param value is the value to set the bit to
						@param bitNumber is the number of the bit you want to retrieve (from 0 to 31)
        */				
				void setBit(uint32_t *whereTo, uint8_t value, uint8_t bitNumber);
				
		/**
            reverses a string
						@param *str is the string to be reversed
        */								
				void strrev(char *str);

		/**
            tells you how many digits does a number have
            @param *b is the value to retrieve the bit from
						@param bitNumber is the number of the bit you want to retrieve (from 0 to 31)
						@return true if bit is 1, false if bit is 0
        */	

				uint8_t getHexNumberLength(uint64_t number);//returns how many digits a hex number has, useful for itox
				
				




};







#endif
