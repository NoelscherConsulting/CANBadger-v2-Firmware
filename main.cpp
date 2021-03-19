/*
* CANBadger code entry
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


#include "mbed.h"
#include "canbadger.h"
#include "canbadger_settings.hpp"
#include "ethernet_manager.hpp"

char cbMac[6];//used to store the ethernet mac, which is derived from the eeprom UID

int main()
{
	//setting up the EthernetManager
	CANbadger canbadger;

	//canbadger.test(); //enable this to run test routine instead of main code

	canbadger.readEEPROMUID((uint8_t*) cbMac); // we get exactly 6 bytes
	cbMac[0]=0x00;
	cbMac[1]=0x02;
	CanbadgerSettings *cb_settings = CanbadgerSettings::restore(&canbadger);
	canbadger.setCanbadgerSettings(cb_settings);

	if(!canbadger.checkForDisplay() || canbadger.chooseStartupMode()) {
		// start in Ethernet mode
		Mail<EthernetMessage, 16> commandQueue = Mail<EthernetMessage, 16>();
		canbadger.setCommandQueue(&commandQueue);
		EthernetManager ethernet_manager = EthernetManager(1, cb_settings, &commandQueue);
		ethernet_manager.setup();
		canbadger.setEthernetManager(&ethernet_manager);
		canbadger.ethernetLoop();
	} else {
		//start Menu for Display mode
		canbadger.mainMenu();
	}
	while(1){}
}

extern "C" void mbed_mac_address(char *s)
{
    memcpy(s, cbMac, 6);
}
