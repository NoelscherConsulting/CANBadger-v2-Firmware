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

#include "buttons.h"
#include "mbed.h"


Buttons::Buttons() 
{
	/***Buttons***/
	//defines for New CB
	StartButton = new DigitalIn(PB4);
	DownButton = new DigitalIn(PB3);
	UpButton = new DigitalIn(PB2);
	BackButton = new DigitalIn(PB1);
	//defines for old CB
	/*DigitalIn StartButton(PB1);
	DigitalIn DownButton(PB2);
	DigitalIn UpButton(PB3);
	DigitalIn BackButton(PB4);*/		

	StartButton->mode(PullUp);
	DownButton->mode(PullUp);
	UpButton->mode(PullUp);
	BackButton->mode(PullUp);	
}

Buttons::~Buttons() 
{
	delete StartButton;
	delete DownButton;
	delete UpButton;
	delete BackButton;
}


uint8_t Buttons::getButtonPressed(float waitTime)
{
	while(1)//we just wait
	{
		if(StartButton->read() == 0)
		{
			wait(waitTime);
			return 1;
		}
		if(DownButton->read() == 0)
		{
			wait(waitTime);
			return 2;
		}
		if(UpButton->read() == 0)
		{
			wait(waitTime);
			return 3;
		}
		if(BackButton->read() == 0)
		{
			wait(waitTime);
			return 4;
		}		
	}
}

bool Buttons::isButtonPressed(uint8_t button)
{
	if(button == 1 && StartButton->read() == 0)
	{
		return true;
	}
	else if(button == 2 && DownButton->read() == 0)
	{
		return true;
	}
	else if(button == 3 && UpButton->read() == 0)
	{
		return true;
	}
	else if(button == 4 && BackButton->read() == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

