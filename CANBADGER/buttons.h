/*
* CANBADGER BUTTONS library 
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

#ifndef __BUTTONS_H__
#define __BUTTONS_H__

#include "mbed.h"

/*Push Buttons stuff*/
//defines for New CB
#define PB1 P2_11
#define PB2 P2_10
#define PB3 P2_12
#define PB4 P2_13



class Buttons
{
    public:

        Buttons(); //Constructor
		
				~Buttons(); //Destructor
				
				uint8_t getButtonPressed(float waitTime = 0.3);//non-interrupt button retrieval

				bool isButtonPressed(uint8_t button);//will return true if the selected button is pressed.

		private:

				/***Buttons***/
				DigitalIn* StartButton;
				DigitalIn* DownButton;
				DigitalIn* UpButton;
				DigitalIn* BackButton;


};







#endif		
