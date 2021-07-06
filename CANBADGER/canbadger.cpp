/*
* CANBadger library
* Copyright (c) 2019 Noelscher Consulting GmbH
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




/****LOG format***
The header has always the same length, and consists of the following:

-Bytes [0] is bus number and protocol encoded bitwise, 
	where the first two LSB represent bus 1 and 2 (bit 0 and bit 1), 
	the third and fourth LSB represent the protocol (bit2 for CAN and bit3 for KLINE), 
	and the fith and sixth represent the kind of CAN frame if applicable (bit4 for Standard and bit 5 for Extended)
-Byte [1-4] is the timestamp
-Bytes [5-8] are used for CAN ID or KLINE sender and target address. In the latter, the first two bytes are the sender, and the latter are the target.
-Bytes [9-12] are the frequency/speed the interface had at the time of the capture
-Byte [13] is the length of the data. We should be good as CAN has max 8 bytes of data per frame, and KLINE has a max of 255
-Following bytes are the data
*/
#include "crc32.h"
#include "canbadger.h"

//We first create all the objects, and later destroy them if not used


//SD Card File Handler
FileHandler sd;


/***LED stuff****/
DigitalOut GLED(GLED_PIN);//green led
DigitalOut RLED(RLED_PIN);//red led


/***CAN***/
CAN can1(CAN1_RX, CAN1_TX);
CAN can2(CAN2_RX, CAN2_TX);

/****SPI RAM***/
RTOS_SPI memory(RAM_MOSI, RAM_MISO, RAM_SCK); // mosi, miso, sclk
Ser23LC1024 ram(&memory, RAM_CS, RAM_HOLD);

/***EEPROM***/
Ser25AA02E48 eeprom(&memory, ROM_CS, ROM_HOLD);

/***PC Serial***/
USBSerial device(0x1f00, 0x2012, 0x0001, false);

/***Ethernet***/
EthernetInterface eth;

/***K-Line***/
Serial kline1(K_LINE1_TX,K_LINE1_RX);
Serial kline2(K_LINE2_TX,K_LINE2_RX);

/***Buttons***/
//defines for New CB
DigitalIn StartButton(PB4);
DigitalIn DownButton(PB3);
DigitalIn UpButton(PB2);
DigitalIn BackButton(PB1);
//defines for old CB
/*DigitalIn StartButton(PB1);
DigitalIn DownButton(PB2);
DigitalIn UpButton(PB3);
DigitalIn BackButton(PB4);*/

/***OLED***/

GPIOHandler relays;
static SSD1327 oled;



/***Conversions***/
Conversions convert;

/***Timer for logging***/
Timer timer;


/***Buttons object***/
Buttons buttons;

CANbadger::CANbadger()
{
	oled.initScreen();
	oled.displayMessage("  CANBADGER V2  ");
	oled.displayMessage(fwVersion,1);
	generalCounter1=0;
	generalCounter2=0;
	generalCounter3=0;
	generalCounter4=0;
	CAN1PaddingByte=0;
	CAN2PaddingByte=0;
	localID = 0;
	remoteID = 0;
	currentDiagSession = 0x01;
	isSDInserted=true;
	this->uds_handler = NULL;

	// should this be done here?. Yes, you dont initialize RAM on the destructor.
	memory.frequency(20000000);

	oled.displayMessage("Checking SD...",1);
    if(isSDInserted == true)//check if the SD is inserted and if we can communicate with it
    {
    	if(sd.checkSD() == false)
    	{
    		oled.displayMessage("SD not Detected!",1);
    		isSDInserted=false;
    	}
		else
		{
			oled.displayMessage("Checking folders",1);
			isSDInserted=1;//we have detected an inserted SD

			// create (or make sure they exist) the following directories in the SD
			const char *folders[36] = {"/Emulator", "/MemDumps", "/MemDumps/DID", "/MemDumps/DID/TP20", "/MemDumps/DID/TP20/LID", "/MemDumps/DID/TP20/CID", "/MemDumps/DID/TP20/ECUID",
					"/MemDumps/DID/UDS", "/MemDumps/DID/KWP2K", "/MemDumps/DID/KWP2K/LID", "/MemDumps/DID/KWP2K/CID", "/MemDumps/DID/KWP2K/ECUID", "/MemDumps/MBA", "/Logging", "/Logging/CAN", "/Logging/RAW", "/Logging/UDS", "/Logging/UDS/Hammer", "/Logging/UDS/Puppet",
					"/Logging/UDS/Scans", "/Logging/KWP2KCAN", "/Logging/KWP2KCAN/Hammer", "/Logging/KWP2KCAN/Puppet", "/Logging/KWP2KCAN/Scans", "/Logging/TP", "/Logging/TP20",
					"/Logging/TP20/Hammer", "/Logging/TP20/Puppet", "/Logging/TP20/Scans", "/Logging/KLINE", "/MITM", "/Replay", "/Transfers", "/Transfers/CAN", "/Transfers/CAN/Uploads",
					"/Transfers/CAN/Downloads"}; //creating an array this long on PC is nice, but this is an embedded system. If we experience crashes it will need to be rolled back.


			for (int folder_iterate = 0; folder_iterate<36; folder_iterate++) {
				if(!sd.doesDirExist(folders[folder_iterate])) {
					if(!sd.makeFolder(folders[folder_iterate])){
						oled.displayMessage("SD Error",1);
						isSDInserted=false;
						break;
					}
				}
			}
		}
    }
	wait(1);
}


CANbadger::~CANbadger()
{
	
}


bool CANbadger::chooseStartupMode()
{
	oled.clearScreen();
	const char* options[15]={"Standalone Mode", "Ethernet Mode"};
	uint8_t option = 1;
	while(1)
	{
		oled.clearScreen();
		option = oled.showOLEDMenu("Choose Mode:", options, 2, &buttons);
		if(option == 1)
		{
			return false;
		}
		else if(option == 2)
		{
			oled.clearScreen();
			oled.displayMessage("Starting up..");
			return true;
		}
	}
}

uint32_t CANbadger::grabASCIIValue()
{
	char tmp[8]={0};
	char chh[2]={0};
	uint8_t cnnt = 0;
	while(chh[0] != ',' && chh[0] != 0x0A && cnnt < 7)//read a value until separator or until EOL
	{
		uint8_t aa = sd.read(chh, 1);
		if(aa != 1)
		{
			return 0xFFFFFFFF;//end of file
		}
		if(chh[0] != 0x0D && chh[0] != 0x0A)
		{
			tmp[cnnt] = chh[0];
		}
		cnnt++;
	}
	return atoh<uint32_t>(tmp);
}

void CANbadger::ScanUDSIDs()
{
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	uint32_t txStart=0;
	uint32_t txEnd=0;
	uint8_t busno=1;
	uint32_t waitMS=50;//50 ms by default is very fast and gives ECUs time to reply if busy
	uint8_t addressType = 0;
	CANFormat formato=CANStandard;
	bool usePadding = false;
	uint8_t paddingByte=0;
	while(1)
	{
		oled.clearScreen();
		oled.displayMessage("    UDS Scan  ");//show the header
		oled.displayMessage(" St: 0x",1);
		char z[22];
		convert.itox(txStart,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" End: 0x",1);
		convert.itox(txEnd,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Wait(ms): ",1);
		convert.itoa(waitMS,z,4);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Bus:",1);
		if(busno == 1)
		{
				oled.displayMessage(" CAN1",0,1);
		}
		else
		{
				oled.displayMessage(" CAN2",0,1);
		}			
		oled.displayMessage(" ID Format: ",1);//show if standard or extended format is being used
		if(formato == CANStandard)
		{
			oled.displayMessage("STD",0,1);
		}
		else
		{
			oled.displayMessage("XTD",0,1);
		}
		oled.displayMessage(" Padding: ",1);
		if(usePadding == true)
		{
				convert.itox(CAN1PaddingByte,z,2);
		}
		else
		{
			z[0]='O';
			z[1]='F';
			z[2]='F';
			z[3]=0;
		}		
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Addressing: ",1);
		if(addressType == 0)
		{
			oled.displayMessage("STD",0,1);
		}
		else
		{
			oled.displayMessage("XTD",0,1);
		}	
		oled.displayMessage(" GO!",1);
		oled.set_text_rc(chosenOption,0);//we set the cursor on current highlight
		oled.putc(arrow);
		bool optChanged=false;
		while(!optChanged)
		{
			uint8_t a = buttons.getButtonPressed();//check which button was pressed
			if(a == 1)
			{
				if(chosenOption == 1)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=0;
					uint32_t tmplocalID=0;
					if(formato == CANStandard)
					{
						tmplen=2;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					else
					{
						tmplen=4;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x1F,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					oled.clearScreen();
					oled.displayMessage("Entered Start:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						txStart = tmplocalID;
					}
					optChanged=true;
				}
				else if(chosenOption == 2)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=0;
					uint32_t tmplocalID=0;
					if(formato == CANStandard)
					{
						tmplen=2;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					else
					{
						tmplen=4;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x1F,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					oled.clearScreen();
					oled.displayMessage("Entered End:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						txEnd = tmplocalID;
					}
					optChanged=true;
				}
				else if(chosenOption == 3)
				{
					if(getDecValue("Enter millis:", (uint64_t*)(&waitMS),1,9999,5))
					{
						optChanged=true;
					}
				}
				else if(chosenOption == 4)
				{
					if(busno == 1)
					{
						busno = 2;
					}
					else
					{
						busno = 1;					
					}
					optChanged=true;
				}				
				else if(chosenOption == 5)
				{
					if(formato == CANStandard)
					{
						formato = CANExtended;
					}	
					else
					{
						formato = CANStandard;
					}
					optChanged=true;
				}
				else if(chosenOption == 6)
				{
					if(usePadding)
					{
						usePadding = false;
					}
					else
					{
						usePadding = true;
						getHexValue("Enter padding",(uint64_t*)(&paddingByte),0,0xFF,1);				
					}
					optChanged=true;
				}
				else if(chosenOption == 7)
				{
					if(addressType == 0)
					{
						addressType=1;
					}
					else
					{
						addressType=0;
					}
					optChanged=true;
				}
				else if(chosenOption == 8)
				{
					oled.clearScreen();
					char filename2[90]="/Logging/UDS/Scans/";
					char tmpBuf[90];
					memset(tmpBuf,0,90);
					sprintf(tmpBuf,"%x_",txStart);
					strcat (filename2,tmpBuf);
					memset(tmpBuf,0,90);
					sprintf(tmpBuf,"%x_",txEnd);
					strcat (filename2,tmpBuf);
					memset(tmpBuf,0,90);
					if(addressType == 0)
					{
						sprintf(tmpBuf,"STD_");
					}
					else
					{
						sprintf(tmpBuf,"XTD_");
					}
					strcat (filename2,tmpBuf);
					char fExt[6]=".TXT";
					if(isSDInserted == true)
					{
						if(!sd.getSequencialFileName(filename2, fExt))
						{
							oled.clearScreen();
							oled.displayMessage(" Filename Error ");
							oled.displayMessage("****************",1);
							oled.displayMessage("  If limit of",1);
							oled.displayMessage(" 65535 files in",1);
							oled.displayMessage("  folder is",1);
							oled.displayMessage("  reached this",1);
							oled.displayMessage("  will happen",1);
							oled.displayMessage("****************",1);
							buttons.getButtonPressed();
							oled.clearScreen();
						}
						if (!sd.openFile(filename2, O_WRONLY | O_CREAT | O_TRUNC))//if we couldnt create the new file or open the log
						{
							oled.displayMessage("SD error...");
							buttons.getButtonPressed();
							isSDInserted = false;
						}
					}
					bool gotResults=false;
					oled.displayMessage("Scanning...");
					if(busno == 1)
					{
						DiagSCAN scan(&can1);
						scan.setTransmissionParameters(0, 0, formato, usePadding, paddingByte);
						for(uint32_t a= txStart; a < (txEnd + 1); a++)
						{
							uint32_t b= scan.scanUDSID(a, 0x01, waitMS, addressType);
							if((b & 0x1FFFFFFF) != 0 && (b & 0xF0000000) != 0xC0000000)//if we got a reply and it is not a broadcast
							{
								if(gotResults == false)
								{
									gotResults = true;
								}
								char z[22];
								if(formato == CANExtended)
								{
									convert.itox((a),z,8);
									oled.displayMessage(z,1);
									oled.displayMessage("->",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,8);
										sd.write("->",2);
									}
									convert.itox((b & 0x1FFFFFFF),z,8);
									oled.displayMessage(z,0,1);
									if(isSDInserted == true)
									{
										sd.write(z,8);
										sd.write("\n",1);
									}
								}
								else
								{
									convert.itox((a),z,3);
									oled.displayMessage(z,1);
									oled.displayMessage("->",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,3);
										sd.write("->",2);
									}
									convert.itox((b & 0x1FFFFFFF),z,4);
									oled.displayMessage(z,0,1);
									if(isSDInserted == true)
									{
										sd.write(z,3);
										sd.write("\n",1);
									}
								}
							}
							else if((b & 0xF0000000) == 0xC0000000)//broadcast
							{
								if(gotResults == false)
								{
									gotResults = true;
								}
								char z[22];
								if(formato == CANExtended)
								{
									convert.itox((a),z,8);
									oled.displayMessage(z,1);
									oled.displayMessage("->",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,8);
										sd.write("->",2);
									}
									convert.itox((b & 0xFFFF),z,4);
									oled.displayMessage(z,0,1);
									oled.displayMessage(" (BC)",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,4);
										sd.write(" (Broadcast replies)\n",1);
									}
								}
								else
								{
									convert.itox((a),z,3);
									oled.displayMessage(z,1);
									oled.displayMessage("->",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,3);
										sd.write("->",2);
									}
									convert.itox((b & 0xFFFF),z,4);
									oled.displayMessage(z,0,1);
									oled.displayMessage(" (BC)",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,4);
										sd.write(" (Broadcast replies)\n",1);
									}
								}
							}
						}
						scan.~DiagSCAN();
					}
					else
					{
						DiagSCAN scan(&can2);
						scan.setTransmissionParameters(0, 0, formato, usePadding, paddingByte);
						for(uint32_t a= txStart; a < (txEnd + 1); a++)
						{
							uint32_t b= scan.scanUDSID(a, 0x01, waitMS, addressType);
							if((b & 0x1FFFFFFF) != 0 && (b & 0xF0000000) != 0xC0000000)//if we got a reply and it is not a broadcast
							{
								if(gotResults == false)
								{
									gotResults = true;
								}
								char z[22];
								if(formato == CANExtended)
								{
									convert.itox((a),z,8);
									oled.displayMessage(z,1);
									oled.displayMessage("->",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,8);
										sd.write("->",2);
									}
									convert.itox((b & 0x1FFFFFFF),z,8);
									oled.displayMessage(z,0,1);
									if(isSDInserted == true)
									{
										sd.write(z,8);
										sd.write("\n",1);
									}
								}
								else
								{
									convert.itox((a),z,3);
									oled.displayMessage(z,1);
									oled.displayMessage("->",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,3);
										sd.write("->",2);
									}
									convert.itox((b & 0x1FFFFFFF),z,4);
									oled.displayMessage(z,0,1);
									if(isSDInserted == true)
									{
										sd.write(z,3);
										sd.write("\n",1);
									}
								}
							}
							else if((b & 0xF0000000) == 0xC0000000)//broadcast
							{
								if(gotResults == false)
								{
									gotResults = true;
								}
								char z[22];
								if(formato == CANExtended)
								{
									convert.itox((a),z,8);
									oled.displayMessage(z,1);
									oled.displayMessage("->",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,8);
										sd.write("->",2);
									}
									convert.itoa((b & 0xFFFF),z,4);
									oled.displayMessage(z,0,1);
									oled.displayMessage("(BCR)",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,4);
										sd.write(" (Broadcast replies)\n",1);
									}
								}
								else
								{
									convert.itox((a),z,3);
									oled.displayMessage(z,1);
									oled.displayMessage("->",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,3);
										sd.write("->",2);
									}
									convert.itoa((b & 0xFFFF),z,4);
									oled.displayMessage(z,0,1);
									oled.displayMessage("(BCR)",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,4);
										sd.write(" (Broadcast replies)\n",1);
									}
								}
							}
						}
						scan.~DiagSCAN();
					}
					if(isSDInserted == true)
					{
						sd.closeFile();
					}
					oled.displayMessage("Done",1);
					buttons.getButtonPressed();
					if(isSDInserted == true && gotResults == true)
					{
						oled.clearScreen();
						oled.displayMessage("Log saved in:");
						oled.displayMessage(filename2,1);
						buttons.getButtonPressed();
					}
					else
					{
						sd.deleteFile(filename2);
					}
					optChanged=true;
				}
			}			
			else if (a == 2)//One option down, the "+" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 8)//in order to not overwrite the header
				{
					b = 1;
				}
				else
				{
					b++;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 3)//One option up, the "-" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 1)//in order to not overwrite the header
				{
					b = 8;
				}
				else
				{
					b--;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 4)
			{
				return;
			}
		}
	}
}


void CANbadger::KWP2KCANReconMenu()
{
	const char* options[15]={"Scan all KWP2K", "Scan active", "Security Hijack", "Security Hammer"};
	uint8_t option = 1;
	while(1)
	{
		oled.clearScreen();
		option = oled.showOLEDMenu("KWP2K CAN Recon", options, 4, &buttons);
		if(option == 0)
		{
			return;
		}
		else if(option == 1)
		{
			ScanKWP2KIDs();
		}
		else if(option == 2)
		{
			ScanActiveKWP2KIDs();
		}
		else if(option == 3)
		{
			KWP2KSecurityHijackMenu();
		}
		else if(option == 4)
		{
			KWP2KSecurityHammerMenu();
		}
	}
}


void CANbadger::ScanKWP2KIDs()
{
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	uint32_t txStart=0;
	uint32_t txEnd=0;
	uint8_t busno=1;
	uint32_t waitMS=50;//50 ms by default is very fast and gives ECUs time to reply if busy
	uint8_t addressType = 0;
	CANFormat formato=CANStandard;
	bool usePadding = false;
	uint8_t paddingByte=0;
	while(1)
	{
		oled.clearScreen();
		oled.displayMessage("   KWP2K Scan   ");//show the header
		oled.displayMessage(" St: 0x",1);
		char z[22];
		convert.itox(txStart,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" End: 0x",1);
		convert.itox(txEnd,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Wait(ms): ",1);
		convert.itoa(waitMS,z,4);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Bus:",1);
		if(busno == 1)
		{
				oled.displayMessage(" CAN1",0,1);
		}
		else
		{
				oled.displayMessage(" CAN2",0,1);
		}
		oled.displayMessage(" ID Format: ",1);//show if standard or extended format is being used
		if(formato == CANStandard)
		{
			oled.displayMessage("STD",0,1);
		}
		else
		{
			oled.displayMessage("XTD",0,1);
		}
		oled.displayMessage(" Padding: ",1);
		if(usePadding == true)
		{
				convert.itox(CAN1PaddingByte,z,2);
		}
		else
		{
			z[0]='O';
			z[1]='F';
			z[2]='F';
			z[3]=0;
		}
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Addressing: ",1);
		if(addressType == 0)
		{
			oled.displayMessage("STD",0,1);
		}
		else
		{
			oled.displayMessage("XTD",0,1);
		}
		oled.displayMessage(" GO!",1);
		oled.set_text_rc(chosenOption,0);//we set the cursor on current highlight
		oled.putc(arrow);
		bool optChanged=false;
		while(!optChanged)
		{
			uint8_t a = buttons.getButtonPressed();//check which button was pressed
			if(a == 1)
			{
				if(chosenOption == 1)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=0;
					uint32_t tmplocalID=0;
					if(formato == CANStandard)
					{
						tmplen=2;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					else
					{
						tmplen=4;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x1F,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					oled.clearScreen();
					oled.displayMessage("Entered Start:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						txStart = tmplocalID;
					}
					optChanged=true;
				}
				else if(chosenOption == 2)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=0;
					uint32_t tmplocalID=0;
					if(formato == CANStandard)
					{
						tmplen=2;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					else
					{
						tmplen=4;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x1F,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					oled.clearScreen();
					oled.displayMessage("Entered End:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						txEnd = tmplocalID;
					}
					optChanged=true;
				}
				else if(chosenOption == 3)
				{
					if(getDecValue("Enter millis:", (uint64_t*)(&waitMS),1,9999,5))
					{
						optChanged=true;
					}
				}
				else if(chosenOption == 4)
				{
					if(busno == 1)
					{
						busno = 2;
					}
					else
					{
						busno = 1;
					}
					optChanged=true;
				}
				else if(chosenOption == 5)
				{
					if(formato == CANStandard)
					{
						formato = CANExtended;
					}
					else
					{
						formato = CANStandard;
					}
					optChanged=true;
				}
				else if(chosenOption == 6)
				{
					if(usePadding)
					{
						usePadding = false;
					}
					else
					{
						usePadding = true;
						getHexValue("Enter padding",(uint64_t*)(&paddingByte),0,0xFF,1);
					}
					optChanged=true;
				}
				else if(chosenOption == 7)
				{
					if(addressType == 0)
					{
						addressType=1;
					}
					else
					{
						addressType=0;
					}
					optChanged=true;
				}
				else if(chosenOption == 8)
				{
					oled.clearScreen();
					char filename2[90]="/Logging/KWP2K/Scans/";
					char tmpBuf[90];
					memset(tmpBuf,0,90);
					sprintf(tmpBuf,"%x_",txStart);
					strcat (filename2,tmpBuf);
					memset(tmpBuf,0,90);
					sprintf(tmpBuf,"%x_",txEnd);
					strcat (filename2,tmpBuf);
					memset(tmpBuf,0,90);
					if(addressType == 0)
					{
						sprintf(tmpBuf,"STD_");
					}
					else
					{
						sprintf(tmpBuf,"XTD_");
					}
					strcat (filename2,tmpBuf);
					char fExt[6]=".TXT";
					if(isSDInserted == true)
					{
						if(!sd.getSequencialFileName(filename2, fExt))
						{
							oled.clearScreen();
							oled.displayMessage(" Filename Error ");
							oled.displayMessage("****************",1);
							oled.displayMessage("  If limit of",1);
							oled.displayMessage(" 65535 files in",1);
							oled.displayMessage("  folder is",1);
							oled.displayMessage("  reached this",1);
							oled.displayMessage("  will happen",1);
							oled.displayMessage("****************",1);
							buttons.getButtonPressed();
							oled.clearScreen();
						}
						if (!sd.openFile(filename2, O_WRONLY | O_CREAT | O_TRUNC))//if we couldnt create the new file or open the log
						{
							oled.displayMessage("SD error...");
							buttons.getButtonPressed();
							isSDInserted = false;
						}
					}
					bool gotResults=false;
					oled.displayMessage("Scanning...");
					if(busno == 1)
					{
						DiagSCAN scan(&can1);
						scan.setTransmissionParameters(0, 0, formato, usePadding, paddingByte);
						for(uint32_t a= txStart; a < (txEnd + 1); a++)
						{
							uint32_t b= scan.scanUDSID(a, KWP_DEFAULT_SESSION, waitMS, addressType);
							if((b & 0x1FFFFFFF) != 0 && (b & 0xF0000000) != 0xC0000000)//if we got a reply and it is not a broadcast
							{
								if(gotResults == false)
								{
									gotResults = true;
								}
								char z[22];
								if(formato == CANExtended)
								{
									convert.itox((a),z,8);
									oled.displayMessage(z,1);
									oled.displayMessage("->",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,8);
										sd.write("->",2);
									}
									convert.itox((b & 0x1FFFFFFF),z,8);
									oled.displayMessage(z,0,1);
									if(isSDInserted == true)
									{
										sd.write(z,8);
										sd.write("\n",1);
									}
								}
								else
								{
									convert.itox((a),z,3);
									oled.displayMessage(z,1);
									oled.displayMessage("->",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,3);
										sd.write("->",2);
									}
									convert.itox((b & 0x1FFFFFFF),z,4);
									oled.displayMessage(z,0,1);
									if(isSDInserted == true)
									{
										sd.write(z,3);
										sd.write("\n",1);
									}
								}
							}
							else if((b & 0xF0000000) == 0xC0000000)//broadcast
							{
								if(gotResults == false)
								{
									gotResults = true;
								}
								char z[22];
								if(formato == CANExtended)
								{
									convert.itox((a),z,8);
									oled.displayMessage(z,1);
									oled.displayMessage("->",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,8);
										sd.write("->",2);
									}
									convert.itox((b & 0xFFFF),z,4);
									oled.displayMessage(z,0,1);
									oled.displayMessage(" (BC)",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,4);
										sd.write(" (Broadcast replies)\n",1);
									}
								}
								else
								{
									convert.itox((a),z,3);
									oled.displayMessage(z,1);
									oled.displayMessage("->",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,3);
										sd.write("->",2);
									}
									convert.itox((b & 0xFFFF),z,4);
									oled.displayMessage(z,0,1);
									oled.displayMessage(" (BC)",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,4);
										sd.write(" (Broadcast replies)\n",1);
									}
								}
							}
						}
						scan.~DiagSCAN();
					}
					else
					{
						DiagSCAN scan(&can2);
						scan.setTransmissionParameters(0, 0, formato, usePadding, paddingByte);
						for(uint32_t a= txStart; a < (txEnd + 1); a++)
						{
							uint32_t b= scan.scanUDSID(a, KWP_DEFAULT_SESSION, waitMS, addressType);
							if((b & 0x1FFFFFFF) != 0 && (b & 0xF0000000) != 0xC0000000)//if we got a reply and it is not a broadcast
							{
								if(gotResults == false)
								{
									gotResults = true;
								}
								char z[22];
								if(formato == CANExtended)
								{
									convert.itox((a),z,8);
									oled.displayMessage(z,1);
									oled.displayMessage("->",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,8);
										sd.write("->",2);
									}
									convert.itox((b & 0x1FFFFFFF),z,8);
									oled.displayMessage(z,0,1);
									if(isSDInserted == true)
									{
										sd.write(z,8);
										sd.write("\n",1);
									}
								}
								else
								{
									convert.itox((a),z,3);
									oled.displayMessage(z,1);
									oled.displayMessage("->",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,3);
										sd.write("->",2);
									}
									convert.itox((b & 0x1FFFFFFF),z,4);
									oled.displayMessage(z,0,1);
									if(isSDInserted == true)
									{
										sd.write(z,3);
										sd.write("\n",1);
									}
								}
							}
							else if((b & 0xF0000000) == 0xC0000000)//broadcast
							{
								if(gotResults == false)
								{
									gotResults = true;
								}
								char z[22];
								if(formato == CANExtended)
								{
									convert.itox((a),z,8);
									oled.displayMessage(z,1);
									oled.displayMessage("->",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,8);
										sd.write("->",2);
									}
									convert.itoa((b & 0xFFFF),z,4);
									oled.displayMessage(z,0,1);
									oled.displayMessage("(BCR)",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,4);
										sd.write(" (Broadcast replies)\n",1);
									}
								}
								else
								{
									convert.itox((a),z,3);
									oled.displayMessage(z,1);
									oled.displayMessage("->",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,3);
										sd.write("->",2);
									}
									convert.itoa((b & 0xFFFF),z,4);
									oled.displayMessage(z,0,1);
									oled.displayMessage("(BCR)",0,1);
									if(isSDInserted == true)
									{
										sd.write(z,4);
										sd.write(" (Broadcast replies)\n",1);
									}
								}
							}
						}
						scan.~DiagSCAN();
					}
					if(isSDInserted == true)
					{
						sd.closeFile();
					}
					oled.displayMessage("Done",1);
					buttons.getButtonPressed();
					if(isSDInserted == true && gotResults == true)
					{
						oled.clearScreen();
						oled.displayMessage("Log saved in:");
						oled.displayMessage(filename2,1);
						buttons.getButtonPressed();
					}
					else
					{
						sd.deleteFile(filename2);
					}
					optChanged=true;
				}
			}
			else if (a == 2)//One option down, the "+" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 8)//in order to not overwrite the header
				{
					b = 1;
				}
				else
				{
					b++;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 3)//One option up, the "-" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 1)//in order to not overwrite the header
				{
					b = 8;
				}
				else
				{
					b--;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 4)
			{
				return;
			}
		}
	}
}






void CANbadger::UDSCANReconMenu()
{
	const char* options[15]={"Scan all UDS", "Scan active UDS", "Security Hijack", "Security Hammer"};
	uint8_t option = 1;
	while(1)
	{
		oled.clearScreen();
		option = oled.showOLEDMenu("  UDS CAN Recon", options, 4, &buttons);
		if(option == 0)
		{
			return;
		}
		else if(option == 1)
		{
			ScanUDSIDs();
		}	
		else if(option == 2)
		{
			ScanActiveUDSIDs();
		}
		else if(option == 3)
		{
			UDSSecurityHijackMenu();
		}
		else if(option == 4)
		{
			UDSSecurityHammerMenu();
		}
	}
}

void CANbadger::UDSSecurityHammerMenu()
{
	if(isSDInserted == false)
	{
		oled.clearScreen();
		oled.displayMessage("SD not inserted");//show the header
		buttons.getButtonPressed();
		return;
	}
	UDSCANHandler uds(&can1);//Security hammer only works on CAN1
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	uint8_t addressType = 0;
	uint8_t secLvl=1;
	CANFormat formato;
	bool usePadding;
	uint8_t paddingByte;
	uint16_t timeDelay=0;
	if(getCANBadgerStatus(CAN1_STANDARD) == true)
	{
		formato = CANStandard;
	}
	else
	{
		formato = CANExtended;
	}
	if(getCANBadgerStatus(CAN1_USE_FULLFRAME) == true)
	{
		usePadding = true;
		paddingByte = CAN1PaddingByte;
	}
	else
	{
		usePadding = false;
	}
	while(1)
	{
		oled.clearScreen();
		oled.displayMessage("UDS SecHamr Menu");//show the header
		oled.displayMessage(" tID: 0x",1);
		char z[22];
		convert.itox(localID,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" rID: 0x",1);
		convert.itox(remoteID,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Diag type: 0x",1);
		convert.itox(currentDiagSession,z,2);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Sec lvl: 0x",1);
		convert.itox(secLvl,z,2);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Format: ",1);//show if standard or extended format is being used
		if(formato == CANStandard)
		{
			oled.displayMessage("STD",0,1);
		}
		else
		{
			oled.displayMessage("XTD",0,1);
		}
		oled.displayMessage(" Padding: ",1);
		if(usePadding == true)
		{
			convert.itox(paddingByte,z,2);
		}
		else
		{
			oled.displayMessage("OFF",0,1);
		}
		oled.displayMessage(" Addressing: ",1);
		if(addressType == 0)
		{
			oled.displayMessage("STD",0,1);
		}
		else
		{
			oled.displayMessage("XTD",0,1);
		}
		oled.displayMessage(" Delay: ",1);
		if(timeDelay == 0)
		{
			oled.displayMessage("NONE",0,1);
		}
		else
		{
			convert.itoa(timeDelay,z,8);
			oled.displayMessage(z,0,1);
		}
		oled.displayMessage(" GO!",1);
		oled.set_text_rc(chosenOption,0);//we set the cursor on current highlight
		oled.putc(arrow);
		bool optChanged=false;
		while(!optChanged)
		{
			uint8_t a = buttons.getButtonPressed();//check which button was pressed
			if(a == 1)
			{
				if(chosenOption == 1)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=0;
					uint32_t tmplocalID=0;
					if(formato == CANStandard)
					{
						tmplen=2;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					else
					{
						tmplen=4;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x1F,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					oled.clearScreen();
					oled.displayMessage("Entered ID:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						localID = tmplocalID;
						uds.setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					}
					optChanged=true;
				}
				else if(chosenOption == 2)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=0;
					uint32_t tmplocalID=0;
					if(formato == CANStandard)
					{
						tmplen=2;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					else
					{
						tmplen=4;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x1F,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					oled.clearScreen();
					oled.displayMessage("Entered RID:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						remoteID = tmplocalID;
						uds.setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					}
					optChanged=true;
				}
				else if(chosenOption == 3)
				{
					getHexValue("Enter Session:",(uint64_t*)(&currentDiagSession),0,0xFF,1);
					optChanged=true;
				}
				else if(chosenOption == 4)
				{
					getHexValue("Enter Level:",(uint64_t*)(&secLvl),0,0xFF,1);
					optChanged=true;
				}
				else if(chosenOption == 5)
				{

					if(getCANBadgerStatus(CAN1_STANDARD) == true)
					{
						setCANBadgerStatus(CAN1_STANDARD,0);
						setCANBadgerStatus(CAN1_EXTENDED,1);
						formato=CANExtended;
					}
					else
					{
						setCANBadgerStatus(CAN1_STANDARD,1);
						setCANBadgerStatus(CAN1_EXTENDED,0);
						formato=CANStandard;
					}
					uds.setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					optChanged=true;
				}
				else if(chosenOption == 6)
				{
					if(usePadding)
					{
						usePadding = false;
						setCANBadgerStatus(CAN1_USE_FULLFRAME,0);
					}
					else
					{
						usePadding = true;
						getHexValue("Enter padding:",(uint64_t*)(&paddingByte),0,0xFF,1);
						setCANBadgerStatus(CAN1_USE_FULLFRAME,1);
						CAN1PaddingByte	= paddingByte;
					}
					uds.setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					optChanged=true;
				}

				else if(chosenOption == 7)
				{
					if(addressType == 0)
					{
						addressType = 1;
						optChanged=true;
					}
					else
					{
						addressType = 0;
						optChanged=true;
					}
					uds.setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
				}
				else if(chosenOption == 8)
				{
					uint16_t tmpbyte = timeDelay;
					if(!getDecValue("Enter MS:",(uint64_t*)(&tmpbyte),0,500,1))
					{
						optChanged=true;
						break;
					}
					optChanged=true;
					timeDelay = tmpbyte;
				}
				else if(chosenOption == 9)
				{
					oled.clearScreen();
					oled.displayMessage("Connecting...");
					uds.setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					generalCounter1 = uds.DiagSessionControl(currentDiagSession);//we just use the counter to grab the code
					if(generalCounter1 == 0)
					{
						oled.displayMessage("Timeout",1);
						buttons.getButtonPressed();
						break;
					}
					else if((generalCounter1 & 0xFFFF0000) != 0)
					{
						uds.checkError((uint8_t)(generalCounter1 >> 16), tmpBuffer);
						displayError(tmpBuffer);
					}
					else
					{
						oled.displayMessage("Connected!",1);
						oled.displayMessage("Hammering DUT...",1);
						oled.displayMessage("Press Back to",1);
						oled.displayMessage("    stop",1);
						oled.displayMessage(" ",1);
						oled.displayMessage("Samples so far:",1);
						char filename[90]="/tmpfile.raw";
						uint32_t numberOfSamples=0;
						uint32_t reply=0;
						char z[22];
						bool doesChangingLvlWork=true;//used to know if changing the secaccess lvl remakes the seed
						bool doesChangingSessionWork=true;//used to know if changing the diag level works
						bool doesRequestingItAgainWork=true;//used to know if simply repeating the request works
						bool requestSeedInNewSession=true;//used to know if we shall request the seed when changing session or not
						uint8_t validSecLvl=0;
						uint8_t validSession=0;
						uint8_t tmpSeed[4]={0,0,0,0};
						if(sd.doesFileExist(filename))//delete the temp file if it already exists
						{
							sd.deleteFile(filename);
						}
						if (!sd.openFile(filename, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
						{
							oled.displayMessage("SD Write error");
							buttons.getButtonPressed();
							return;
						}
						bool failed=false;
						while(buttons.isButtonPressed(4) != 1)//run as long as the button is not pressed
						{
							oled.clearLine(8);//update the counter
							oled.set_rc(8,0);
							convert.itoa(numberOfSamples,z,16);
							oled.displayMessage(z,0,1);
							bool isDifferent=true;
							wait_ms(timeDelay);
							reply = uds.requestSeed(secLvl, tmpBuffer);
							if(reply == 0)//if we have no reply
							{
								oled.clearScreen();
								oled.displayMessage("Timeout");
								buttons.getButtonPressed();
								failed=true;
								break;
							}
							else if ((reply & 0xFFFF0000) != 0 && numberOfSamples == 0)//if there was an error
							{
								uds.checkError((uint8_t)(reply >> 16), tmpBuffer);
								displayError(tmpBuffer);
								failed=true;
								break;
							}
							else if ((reply & 0xFFFF0000) != 0 && numberOfSamples != 0)//if there was an error
							{
								isDifferent=false;
							}
							if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) == 0 || isDifferent == false)
							{
								isDifferent=false;
								while(!isDifferent)//let's try to force a new seed
								{
									if(doesRequestingItAgainWork == true)
									{
										uint8_t l;
										for(l=0; l< 5; l++)
										{
											wait_ms(timeDelay);
											reply = uds.requestSeed(secLvl, tmpBuffer);//we request a new seed
											if((reply & 0xFFFF) != 0)//if we got a valid reply
											{
												if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) != 0)
												{
													isDifferent=true;
													break;
												}
											}
											else if(reply == 0)//if we have no reply
											{
												l=5;
												break;
											}

										}
										if(l == 5)
										{
											doesRequestingItAgainWork=false;
										}
									}
									else if(doesChangingLvlWork == true)
									{
										if(validSecLvl == 0)//if its the first run
										{
											for(uint16_t w=1; w < 0x100; w = (w + 2))//we will look for a level that provides a seed
											{
												if(w != secLvl)
												{
													wait_ms(timeDelay);
													reply = uds.requestSeed(w, tmpBuffer);//we change to a different level to try reset secaccess
													if((reply & 0xFFFF) != 0)//but if we got a positive reply
													{
														validSecLvl=w;
														break;
													}
												}
											}
											wait_ms(timeDelay);
											reply = uds.requestSeed(secLvl, tmpBuffer);//then we request a new seed
											if(reply == 0)
											{
												doesChangingLvlWork=false;
												break;
											}
											else if((reply & 0xFFFF0000) != 0)//if we got an error
											{
												doesChangingLvlWork=false;
											}
											else if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) == 0)//if they are the same
											{
												doesChangingLvlWork=false;//we know it doesnt work
											}
											else
											{
												if(validSecLvl == 0)//if we did go through all levels but none provided a key, yet it did work
												{
													validSecLvl= secLvl + 2;//we just set a random one
												}
												isDifferent=true;
												break;
											}
										}
										else
										{
											wait_ms(timeDelay);
											reply = uds.requestSeed(validSecLvl, tmpBuffer);
											wait_ms(timeDelay);
											reply = uds.requestSeed(secLvl, tmpBuffer);
											if(reply == 0)
											{
												doesChangingLvlWork=false;
											}
											else if((reply & 0xFFFF0000) != 0)//if we got an error
											{
												doesChangingLvlWork=false;
											}
											else if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) == 0)//if they are the same
											{
												doesChangingLvlWork=false;//we know it doesnt work
											}
											else
											{
												isDifferent=true;
												break;
											}
										}
									}
									else if(doesChangingSessionWork == true)
									{
										if(validSession == 0 && requestSeedInNewSession == true)//if its the first run
										{
											for(uint16_t w=1; w < 0x100; w++)//we will look for an accepted session
											{
												if(w != currentDiagSession)
												{
													wait_ms(timeDelay);
													reply = uds.DiagSessionControl(w);//we change to a different session to try reset secaccess
													if((reply & 0xFFFF) != 0)//but if we got a positive reply
													{
														wait_ms(timeDelay);
														reply = uds.requestSeed(secLvl, tmpBuffer);//then we request a new seed
														if((reply & 0xFFFF) != 0)//if we got a positive reply
														{
															if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) != 0)//if they are different
															{
																validSession=w;
																requestSeedInNewSession=true;
																isDifferent=true;
																break;
															}
														}
														wait_ms(timeDelay);
														reply = uds.DiagSessionControl(currentDiagSession);//then we go back to the original one
														wait_ms(timeDelay);
														reply = uds.requestSeed(secLvl, tmpBuffer);//then we request a new seed
														if((reply & 0xFFFF) != 0)//if we got a positive reply
														{
															if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) != 0)//if they are different
															{
																validSession=w;
																isDifferent=true;
																break;
															}
														}
													}
												}
											}
											if(isDifferent == false)//if it didnt work
											{
												requestSeedInNewSession=false;//we know it doesnt work. right? or not? oh man, this is stressing...
											}
										}
										else
										{
											if(requestSeedInNewSession == true)
											{
												wait_ms(timeDelay);
												reply = uds.DiagSessionControl(validSession);//we change to a different session to try reset secaccess
												if(reply == 0)
												{
													doesChangingSessionWork=false;
													break;
												}
												wait_ms(timeDelay);
												reply = uds.requestSeed(secLvl, tmpBuffer);//then we request a new seed
												if((reply & 0xFFFF) != 0)//if we got a positive reply
												{
													if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) != 0)//if they are different
													{
														wait_ms(timeDelay);
														uds.DiagSessionControl(currentDiagSession);//we switch back to the normal one
														isDifferent=true;
														break;
													}
												}
												wait_ms(timeDelay);
												uds.DiagSessionControl(currentDiagSession);//we switch back to the normal one
												requestSeedInNewSession=false;//if we got here, it failed
											}
											else
											{
												wait_ms(timeDelay);
												reply = uds.DiagSessionControl(currentDiagSession + 2);//we change to a different session to try reset secaccess
												if(reply == 0)
												{
													doesChangingSessionWork=false;
													break;
												}
												wait_ms(timeDelay);
												uds.DiagSessionControl(currentDiagSession);//we switch back to the normal one
												wait_ms(timeDelay);
												reply = uds.requestSeed(secLvl, tmpBuffer);//then we request a new seed
												if((reply & 0xFFFF) != 0)//if we got a positive reply
												{
													if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) != 0)//if they are different
													{
														isDifferent=true;
														break;
													}
												}
												doesChangingSessionWork=false;//else, it didnt work
											}
										}
									}
									else//the DUT is not vulnerable
									{
										oled.clearScreen();
										oled.displayMessage("This DUT is not");
										oled.displayMessage(" vulnerable to",1);
										oled.displayMessage("  SecHammer or",1);
										oled.displayMessage("uses static seed",1);
										oled.displayMessage("  for requested",1);
										oled.displayMessage(" Security level",1);
										buttons.getButtonPressed();
										failed=true;
										break;
									}
								}
							}
							if(isDifferent==true)
							{
								for(uint8_t q=0; q<4; q++)//copy the seed
								{
									tmpSeed[q]=tmpBuffer[(q+2)];
								}
								sd.write((char*)tmpSeed,4);//we write it to the file
								numberOfSamples++;
							}
							if(failed == true)
							{
								break;
							}
						}
						sd.closeFile();
						if(failed == true)
						{
							sd.deleteFile(filename);//delete the temp file
						}
						else
						{
							oled.clearScreen();
							oled.displayMessage("Cleaning up..");
							char filename2[90]="/Logging/UDS/Hammer/";
							char tmpBuf[90];
							memset(tmpBuf,0,90);
							sprintf(tmpBuf,"%x_",remoteID);
							strcat (filename2,tmpBuf);
							memset(tmpBuf,0,90);
							sprintf(tmpBuf,"%x_",secLvl);
							strcat (filename2,tmpBuf);
							char fExt[6]=".CSV";
							if(!sd.getSequencialFileName(filename2, fExt))
							{
								oled.clearScreen();
								oled.displayMessage(" Filename Error ");
								oled.displayMessage("****************",1);
								oled.displayMessage("  If limit of",1);
								oled.displayMessage(" 65535 files in",1);
								oled.displayMessage("  folder is",1);
								oled.displayMessage("  reached this",1);
								oled.displayMessage("  will happen",1);
								oled.displayMessage("****************",1);
								buttons.getButtonPressed();
								break;
							}
							if (!sd.openFile(filename2, O_WRONLY | O_CREAT | O_TRUNC) || !sd.openFile(filename, O_RDONLY,2))//if we couldnt create the new file or open the log
							{
								oled.displayMessage("SD error...");
								buttons.getButtonPressed();
								break;
							}
							for(uint32_t i=0; i< numberOfSamples; i++)
							{
								uint8_t y=sd.read((char*)tmpBuffer,4,2);
								if(y != 4)//if we reached the end of the file for some reason before we got all the samples
								{
									break;
								}
								else
								{
									uint32_t u=tmpBuffer[0];
									u = (u << 8) + tmpBuffer[1];
									u = (u << 8) + tmpBuffer[2];
									u = (u << 8) + tmpBuffer[3];
									convert.itox(u,z,8);
									sd.write(z,8,1);
									char bs[6]=",\n";
									if(i != (numberOfSamples - 1))//we dont write the last one
									{
										sd.write(bs,2,1);
									}
								}
							}
							sd.closeFile();
							sd.closeFile(2);
							sd.deleteFile(filename);
							oled.clearScreen();
							oled.displayMessage("Log saved in:");
							oled.displayMessage(filename2,1);
							if(sd.doesFileExist(filename))//delete the temp file
							{
								sd.deleteFile(filename);
							}
							buttons.getButtonPressed();
						}
					}
					optChanged=true;
				}
			}
			else if (a == 2)//One option down, the "+" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 9)//in order to not overwrite the header
				{
					b = 1;
				}
				else
				{
					b++;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 3)//One option up, the "-" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 1)//in order to not overwrite the header
				{
					b = 9;
				}
				else
				{
					b--;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 4)
			{
				return;
			}
		}
	}
}

void CANbadger::KWP2KSecurityHammerMenu()
{
	if(isSDInserted == false)
	{
		oled.clearScreen();
		oled.displayMessage("SD not inserted");//show the header
		buttons.getButtonPressed();
		return;
	}
	KWP2KCANHandler kwp(&can1);//Security hammer only works on CAN1
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	uint8_t addressType = 0;
	uint8_t secLvl=1;
	CANFormat formato;
	bool usePadding;
	uint8_t paddingByte;
	if(getCANBadgerStatus(CAN1_STANDARD) == true)
	{
		formato = CANStandard;
	}
	else
	{
		formato = CANExtended;
	}
	if(getCANBadgerStatus(CAN1_USE_FULLFRAME) == true)
	{
		usePadding = true;
		paddingByte = CAN1PaddingByte;
	}
	else
	{
		usePadding = false;
	}
	while(1)
	{
		oled.clearScreen();
		oled.displayMessage("KWP SecHamr Menu");//show the header
		oled.displayMessage(" tID: 0x",1);
		char z[22];
		convert.itox(localID,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" rID: 0x",1);
		convert.itox(remoteID,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Diag type: 0x",1);
		convert.itox(currentDiagSession,z,2);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Sec lvl: 0x",1);
		convert.itox(secLvl,z,2);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Format: ",1);//show if standard or extended format is being used
		if(formato == CANStandard)
		{
			oled.displayMessage("STD",0,1);
		}
		else
		{
			oled.displayMessage("XTD",0,1);
		}
		oled.displayMessage(" Padding: ",1);
		if(usePadding == true)
		{
			convert.itox(paddingByte,z,2);
		}
		else
		{
			z[0]='O';
			z[1]='F';
			z[2]='F';
			z[3]=0;
		}
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Addressing: ",1);
		if(addressType == 0)
		{
			oled.displayMessage("STD",0,1);
		}
		else
		{
			oled.displayMessage("XTD",0,1);
		}
		oled.displayMessage(" GO!",1);
		oled.set_text_rc(chosenOption,0);//we set the cursor on current highlight
		oled.putc(arrow);
		bool optChanged=false;
		while(!optChanged)
		{
			uint8_t a = buttons.getButtonPressed();//check which button was pressed
			if(a == 1)
			{
				if(chosenOption == 1)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=0;
					uint32_t tmplocalID=0;
					if(formato == CANStandard)
					{
						tmplen=2;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					else
					{
						tmplen=4;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x1F,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					oled.clearScreen();
					oled.displayMessage("Entered ID:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						localID = tmplocalID;
						kwp.setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					}
					optChanged=true;
				}
				else if(chosenOption == 2)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=0;
					uint32_t tmplocalID=0;
					if(formato == CANStandard)
					{
						tmplen=2;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					else
					{
						tmplen=4;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x1F,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					oled.clearScreen();
					oled.displayMessage("Entered RID:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						remoteID = tmplocalID;
						kwp.setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					}
					optChanged=true;
				}
				else if(chosenOption == 3)
				{
					getHexValue("Enter Session:",(uint64_t*)(&currentDiagSession),0,0xFF,1);
					optChanged=true;
				}
				else if(chosenOption == 4)
				{
					getHexValue("Enter Level:",(uint64_t*)(&secLvl),0,0xFF,1);
					optChanged=true;
				}
				else if(chosenOption == 5)
				{

					if(getCANBadgerStatus(CAN1_STANDARD) == true)
					{
						setCANBadgerStatus(CAN1_STANDARD,0);
						setCANBadgerStatus(CAN1_EXTENDED,1);
						formato=CANExtended;
					}
					else
					{
						setCANBadgerStatus(CAN1_STANDARD,1);
						setCANBadgerStatus(CAN1_EXTENDED,0);
						formato=CANStandard;
					}
					kwp.setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					optChanged=true;
				}
				else if(chosenOption == 6)
				{
					if(usePadding)
					{
						usePadding = false;
						setCANBadgerStatus(CAN1_USE_FULLFRAME,0);
					}
					else
					{
						usePadding = true;
						getHexValue("Enter padding:",(uint64_t*)(&paddingByte),0,0xFF,1);
						setCANBadgerStatus(CAN1_USE_FULLFRAME,1);
						CAN1PaddingByte	= paddingByte;
					}
					kwp.setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					optChanged=true;
				}

				else if(chosenOption == 7)
				{
					if(addressType == 0)
					{
						addressType = 1;
						optChanged=true;
					}
					else
					{
						addressType = 0;
						optChanged=true;
					}
					kwp.setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
				}
				else if(chosenOption == 8)
				{
					oled.clearScreen();
					oled.displayMessage("Connecting...");
					kwp.setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					generalCounter1 = kwp.startComms(tmpBuffer, currentDiagSession);//we just use the counter to grab the code
					if(generalCounter1 == 0)
					{
						oled.displayMessage("Timeout",1);
						buttons.getButtonPressed();
						optChanged=true;
						break;
					}
					else if((generalCounter1 & 0xFFFF0000) != 0)
					{
						kwp.checkError((uint8_t)(generalCounter1 >> 16), tmpBuffer);
						displayError(tmpBuffer);
						optChanged=true;
						break;
					}
					else
					{
						oled.displayMessage("Connected!",1);
						oled.displayMessage("Hammering DUT...",1);
						oled.displayMessage("Press Back to",1);
						oled.displayMessage("    stop",1);
						oled.displayMessage(" ",1);
						oled.displayMessage("Samples so far:",1);
						char filename[90]="/tmpfile.raw";
						uint32_t numberOfSamples=0;
						uint32_t reply=0;
						char z[22];
						bool doesChangingLvlWork=true;//used to know if changing the secaccess lvl remakes the seed
						bool doesChangingSessionWork=true;//used to know if changing the diag level works
						bool doesRequestingItAgainWork=true;
						uint8_t tmpSeed[4]={0,0,0,0};
						if(sd.doesFileExist(filename))//delete the temp file if it already exists
						{
							sd.deleteFile(filename);
						}
						if (!sd.openFile(filename, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
						{
							oled.displayMessage("SD Write error");
							buttons.getButtonPressed();
							return;
						}
						bool failed=false;
						while(buttons.isButtonPressed(4) != 1)//run as long as the button is not pressed
						{
							oled.clearLine(8);//update the counter
							oled.set_rc(8,0);
							convert.itoa(numberOfSamples,z,16);
							oled.displayMessage(z,0,1);
							bool isDifferent=true;
							reply = kwp.requestSeed(secLvl, tmpBuffer);
							if(reply == 0)//if we have no reply
							{
								oled.clearScreen();
								oled.displayMessage("Timeout");
								buttons.getButtonPressed();
								break;
							}
							else if ((reply & 0xFFFF0000) != 0 && numberOfSamples == 0)//if there was an error but we didnt have a reply before, it is a legit error
							{
								kwp.checkError((uint8_t)(reply >> 16), tmpBuffer);
								displayError(tmpBuffer);
								failed=true;
								break;
							}
							else if ((reply & 0xFFFF0000) != 0 && numberOfSamples != 0)//if there was an error but we already had a first positive reply
							{
								isDifferent=false;
							}
							if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) == 0 || isDifferent == false)//if they are the same
							{
								isDifferent=false;
								while(!isDifferent)//let's try to force a new seed
								{
									if(doesRequestingItAgainWork == true)
									{
										uint8_t l=0;
										for(l=0; l< 5; l++)
										{
											reply = kwp.requestSeed(secLvl, tmpBuffer);//we request a new seed
											if((reply & 0xFFFF) != 0)//if we got a valid reply
											{
												if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) != 0)
												{
													isDifferent=true;
													break;
												}
											}
											else if(reply == 0)//if we have no reply
											{
												oled.displayMessage("Timeout",1);
												buttons.getButtonPressed();
												break;
											}

										}
										if(l == 5)
										{
											doesRequestingItAgainWork=false;
										}
									}
									else if(doesChangingLvlWork == true)//we try this one first as it is faster
									{
										if(secLvl != 1)
										{
											generalCounter1 = kwp.requestSeed(1, tmpBuffer);
										}
										else
										{
											generalCounter1 = kwp.requestSeed(3, tmpBuffer);;//we change to a different session to try reset secaccess
										}
										if(generalCounter1 == 0)
										{
											oled.displayMessage("Timeout",1);
											buttons.getButtonPressed();
											break;
										}
										reply = kwp.requestSeed(secLvl, tmpBuffer);//then we request a new seed
										if(reply == 0)
										{
											oled.displayMessage("Timeout",1);
											buttons.getButtonPressed();
											break;
										}
										else if((reply & 0xFFFF0000) != 0)//if we got an error
										{
											doesChangingLvlWork=false;
										}
										else if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) == 0)//if they are the same
										{
											doesChangingLvlWork=false;//we know it doesnt work
										}
										else
										{
											isDifferent=true;
											break;
										}
									}
									else if(doesChangingSessionWork == true)
									{
										if(currentDiagSession != 0x81)
										{
											generalCounter1 = kwp.startComms(tmpBuffer, 0x81);//we change to a different session to try reset secaccess
										}
										else
										{
											generalCounter1 = kwp.startComms(tmpBuffer, 0x83);//we change to a different session to try reset secaccess
										}
										if(generalCounter1 == 0)
										{
											oled.displayMessage("Timeout",1);
											buttons.getButtonPressed();
											break;
										}
										generalCounter1 = kwp.startComms(tmpBuffer, currentDiagSession);//then we restore the target one
										if(generalCounter1 == 0)
										{
											oled.displayMessage("Timeout",1);
											buttons.getButtonPressed();
											break;
										}
										reply = kwp.requestSeed(secLvl, tmpBuffer);//then we request a new seed
										if(reply == 0)
										{
											oled.displayMessage("Timeout",1);
											buttons.getButtonPressed();
											break;
										}
										else if((reply & 0xFFFF0000) != 0)//if we got an error
										{
											doesChangingSessionWork=false;
										}
										else if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) == 0)//if they are the same
										{
											doesChangingSessionWork=false;//we know it doesnt work
										}
										else
										{
											isDifferent=true;
											break;
										}
									}
									else//the DUT is not vulnerable
									{
										oled.clearScreen();
										oled.displayMessage("This DUT is not");
										oled.displayMessage("vulnerable to",1);
										oled.displayMessage("  SecHammer",1);
										buttons.getButtonPressed();
										failed=true;
										break;
									}
								}
							}
							if(isDifferent==true)
							{
								for(uint8_t q=0; q<4; q++)//copy the seed
								{
									tmpSeed[q]=tmpBuffer[(q+2)];
								}
								sd.write((char*)tmpSeed,4);//we write it to the file
								numberOfSamples++;
							}
							if(failed == true)
							{
								break;
							}
						}
						sd.closeFile();
						if(failed == true)
						{
							sd.deleteFile(filename);//delete the temp file
						}
						else
						{
							oled.clearScreen();
							oled.displayMessage("Cleaning up..");
							char filename2[90]="/Logging/KWP2KCAN/Hammer/";
							char tmpBuf[90];
							memset(tmpBuf,0,90);
							sprintf(tmpBuf,"%x_",remoteID);
							strcat (filename2,tmpBuf);
							memset(tmpBuf,0,90);
							sprintf(tmpBuf,"%x_",secLvl);
							strcat (filename2,tmpBuf);
							char fExt[6]=".CSV";
							if(!sd.getSequencialFileName(filename2, fExt))
							{
								oled.clearScreen();
								oled.displayMessage(" Filename Error ");
								oled.displayMessage("****************",1);
								oled.displayMessage("  If limit of",1);
								oled.displayMessage(" 65535 files in",1);
								oled.displayMessage("  folder is",1);
								oled.displayMessage("  reached this",1);
								oled.displayMessage("  will happen",1);
								oled.displayMessage("****************",1);
								buttons.getButtonPressed();
								break;
							}
							if (!sd.openFile(filename2, O_WRONLY | O_CREAT | O_TRUNC) || !sd.openFile(filename, O_RDONLY,2))//if we couldnt create the new file or open the log
							{
								oled.displayMessage("SD error...");
								buttons.getButtonPressed();
								break;
							}
							for(uint32_t i=0; i< numberOfSamples; i++)
							{
								uint8_t y=sd.read((char*)tmpBuffer,4,2);
								if(y != 4)//if we reached the end of the file for some reason before we got all the samples
								{
									break;
								}
								else
								{
									uint32_t u=tmpBuffer[0];
									u = (u << 8) + tmpBuffer[1];
									u = (u << 8) + tmpBuffer[2];
									u = (u << 8) + tmpBuffer[3];
									convert.itox(u,z,8);
									sd.write(z,8,1);
									char bs[6]=",\n";
									if(i != (numberOfSamples - 1))//we dont write the last one
									{
										sd.write(bs,2,1);
									}
								}
							}
							sd.closeFile();
							sd.closeFile(2);
							sd.deleteFile(filename);
							oled.clearScreen();
							oled.displayMessage("Log saved in:");
							oled.displayMessage(filename2,1);
							if(sd.doesFileExist(filename))//delete the temp file
							{
								sd.deleteFile(filename);
							}
							buttons.getButtonPressed();
						}
					}
					optChanged=true;
				}
			}
			else if (a == 2)//One option down, the "+" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 8)//in order to not overwrite the header
				{
					b = 1;
				}
				else
				{
					b++;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 3)//One option up, the "-" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 1)//in order to not overwrite the header
				{
					b = 8;
				}
				else
				{
					b--;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 4)
			{
				return;
			}
		}
	}
}



void CANbadger::TP20SecurityHammerMenu()
{
	if(isSDInserted == false)
	{
		oled.clearScreen();
		oled.displayMessage("SD not inserted");//show the header
		buttons.getButtonPressed();
		return;
	}
	KWP2KTP20Handler tp20(&can1);//Security hammer only works on CAN1
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	uint8_t secLvl=1;
	uint32_t cID=0x200;//TP20 channel negotiation ID
	uint8_t ecuID=0x01;//ID for the target ECU
	uint16_t timeDelay=0;
	currentDiagSession = 0x0;//set it to no diag session, which seems to be the most common case
	while(1)
	{
		oled.clearScreen();
		oled.displayMessage("TP2 SecHamr Menu");//show the header
		oled.displayMessage(" cID: 0x",1);
		char z[22];
		convert.itox(cID,z,3);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" tID: 0x",1);
		convert.itox(ecuID,z,2);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Diag type: ",1);
		if(currentDiagSession != 0)
		{
			oled.displayMessage("0x",0,1);
			convert.itox(currentDiagSession,z,2);
			oled.displayMessage(z,0,1);
		}
		else
		{
			oled.displayMessage("NONE",0,1);
		}
		oled.displayMessage(" Sec lvl: 0x",1);
		convert.itox(secLvl,z,2);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Delay: ",1);
		if(timeDelay == 0)
		{
			oled.displayMessage("NONE",0,1);
		}
		else
		{
			convert.itoa(timeDelay,z,8);
			oled.displayMessage(z,0,1);
		}
		oled.displayMessage(" GO!",1);
		oled.set_text_rc(chosenOption,0);//we set the cursor on current highlight
		oled.putc(arrow);
		bool optChanged=false;
		while(!optChanged)
		{
			uint8_t a = buttons.getButtonPressed();//check which button was pressed
			if(a == 1)
			{
				if(chosenOption == 1)
				{
					uint8_t tmpcID=0;
					uint8_t q = getHexValue("Enter LSB:",(uint64_t*)(&tmpcID),0,0xEF,1);
					if(q != 0)
					{
						cID = 0x200 + tmpcID;
					}
					optChanged=true;
				}
				else if(chosenOption == 2)
				{
					getHexValue("Enter Target ID:",(uint64_t*)(&ecuID),0,0xFF,1);
					optChanged=true;
				}
				else if(chosenOption == 3)
				{
					getHexValue("Enter Diag lvl:",(uint64_t*)(&currentDiagSession),0,0xFF,1);
					optChanged=true;
				}
				else if(chosenOption == 4)
				{
					getHexValue("Enter Sec Level:",(uint64_t*)(&secLvl),0,0xFF,1);
					optChanged=true;
				}
				else if(chosenOption == 5)
				{
					uint16_t tmpbyte = timeDelay;
					if(!getDecValue("Enter MS:",(uint64_t*)(&tmpbyte),0,500,1))
					{
						optChanged=true;
						break;
					}
					optChanged=true;
					timeDelay = tmpbyte;
				}
				else if(chosenOption == 6)
				{
					oled.clearScreen();
					oled.displayMessage("Connecting...");
					generalCounter1 = tp20.channelSetup(cID, ecuID);
					if(generalCounter1 == 0)
					{
						oled.displayMessage("Timeout",1);
						buttons.getButtonPressed();
						break;
					}
					else if(generalCounter1 != 1)
					{
						oled.displayMessage("Channel Error",1);
						buttons.getButtonPressed();
						break;
					}
					else
					{
						oled.displayMessage("Channel OK!",1);
						bool doesChangingLvlWork=true;//used to know if changing the secaccess lvl remakes the seed
						bool doesChangingSessionWork=true;//used to know if changing the diag level works
						bool doesRequestingItAgainWork=true;
						bool requestSeedInNewSession=true;//used to know if we shall request the seed when changing session or not
						uint8_t validSecLvl=0;
						uint8_t validSession=0;
						uint8_t tmpSeed[4]={0,0,0,0};
						if(currentDiagSession != 0)//if user wants to start a specific session
						{
							generalCounter1 = tp20.startComms(tmpBuffer, currentDiagSession);
							if(generalCounter1 == 0)
							{
								oled.displayMessage("Timeout",1);
								buttons.getButtonPressed();
								tp20.closeChannel();
								optChanged=true;
								break;
							}
							else if((generalCounter1 & 0xFFFF0000) != 0)
							{
								tp20.checkError((uint8_t)(generalCounter1 >> 16), tmpBuffer);
								displayError(tmpBuffer);
								optChanged=true;
								tp20.closeChannel();
								break;
							}
							oled.displayMessage("Connected!",1);
						}
						else
						{
							doesChangingSessionWork=false;//to avoid trying to change the diag session type
						}
						oled.displayMessage("Hammering DUT...",1);
						oled.displayMessage("Press Back to",1);
						oled.displayMessage("    stop",1);
						oled.displayMessage(" ",1);
						oled.displayMessage("Samples so far:",1);
						char filename[90]="/tmpfile.raw";
						uint32_t numberOfSamples=0;
						uint32_t reply=0;
						char z[22];
						if(sd.doesFileExist(filename))//delete the temp file if it already exists
						{
							sd.deleteFile(filename);
						}
						if (!sd.openFile(filename, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
						{
							oled.displayMessage("SD Write error");
							buttons.getButtonPressed();
							tp20.closeChannel();
							return;
						}
						bool failed=false;
						while(buttons.isButtonPressed(4) != 1)//run as long as the button is not pressed
						{
							oled.clearLine(8);//update the counter
							oled.set_rc(8,0);
							convert.itoa(numberOfSamples,z,16);
							oled.displayMessage(z,0,1);
							reply = tp20.requestSeed(secLvl, tmpBuffer);
							bool isDifferent=true;
							if(reply == 0)//if we have no reply
							{
								oled.clearScreen();
								oled.displayMessage("Timeout");
								buttons.getButtonPressed();
								tp20.closeChannel();
								break;
							}
							else if ((reply & 0xFFFF0000) != 0 && numberOfSamples == 0)//if there was an error
							{
								tp20.checkError((uint8_t)(reply >> 16), tmpBuffer);
								displayError(tmpBuffer);
								failed=true;
								tp20.closeChannel();
								break;
							}
							else if ((reply & 0xFFFF0000) != 0 && numberOfSamples != 0)//if there was an error
							{
								isDifferent=false;
							}
							if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) == 0 || isDifferent == false)//if they are the same or we got an error on the second request
							{
								isDifferent=false;
								while(!isDifferent)//let's try to force a new seed
								{
									if(doesRequestingItAgainWork == true)
									{
										uint8_t l=0;
										for(l=0; l< 5; l++)
										{
											wait_ms(timeDelay);
											reply = tp20.requestSeed(secLvl, tmpBuffer);//we request a new seed
											if((reply & 0xFFFF) != 0)//if we got a valid reply
											{
												if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) != 0)
												{
													isDifferent=true;
													break;
												}
											}
											else if(reply == 0)//if we have no reply
											{
												wait(0.005);//throttle down a bit
											}
										}
										if(l == 5)
										{
											doesRequestingItAgainWork=false;
										}
									}
									else if(doesChangingLvlWork == true)//we try this one first as it is faster
									{
										if(validSecLvl == 0)//if its the first run
										{
											for(uint16_t w=1; w < 0x100; w = (w + 2))//we will look for a level that provides a seed
											{
												if(w != secLvl)
												{
													wait_ms(timeDelay);
													reply = tp20.requestSeed(w, tmpBuffer);//we change to a different level to try reset secaccess
													if((reply & 0xFFFF) != 0)//but if we got a positive reply
													{
														validSecLvl=w;
														break;
													}
												}
											}
											wait_ms(timeDelay);
											reply = tp20.requestSeed(secLvl, tmpBuffer);//then we request a new seed
											if(reply == 0)
											{
												doesChangingLvlWork=false;
												break;
											}
											else if((reply & 0xFFFF0000) != 0)//if we got an error
											{
												doesChangingLvlWork=false;
											}
											else if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) == 0)//if they are the same
											{
												doesChangingLvlWork=false;//we know it doesnt work
											}
											else
											{
												if(validSecLvl == 0)//if we did go through all levels but none provided a key, yet it did work
												{
													validSecLvl= secLvl + 2;//we just set a random one
												}
												isDifferent=true;
												break;
											}
										}
										else
										{
											wait_ms(timeDelay);
											reply = tp20.requestSeed(validSecLvl, tmpBuffer);
											wait_ms(timeDelay);
											reply = tp20.requestSeed(secLvl, tmpBuffer);
											if(reply == 0)
											{
												doesChangingLvlWork=false;
											}
											else if((reply & 0xFFFF0000) != 0)//if we got an error
											{
												doesChangingLvlWork=false;
											}
											else if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) == 0)//if they are the same
											{
												doesChangingLvlWork=false;//we know it doesnt work
											}
											else
											{
												isDifferent=true;
												break;
											}
										}
									}
									else if(doesChangingSessionWork == true)
									{
										if(validSession == 0 && requestSeedInNewSession == true)//if its the first run
										{
											for(uint16_t w=1; w < 0x100; w++)//we will look for an accepted session
											{
												if(w != currentDiagSession)
												{
													wait_ms(timeDelay);
													reply = tp20.startComms(tmpBuffer,w);//we change to a different session to try reset secaccess
													if((reply & 0xFFFF) != 0)//but if we got a positive reply
													{
														wait_ms(timeDelay);
														reply = tp20.requestSeed(secLvl, tmpBuffer);//then we request a new seed
														if((reply & 0xFFFF) != 0)//if we got a positive reply
														{
															if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) != 0)//if they are different
															{
																validSession=w;
																requestSeedInNewSession=true;
																isDifferent=true;
																break;
															}
														}
														wait_ms(timeDelay);
														reply = tp20.startComms(tmpBuffer,currentDiagSession);//then we go back to the original one
														wait_ms(timeDelay);
														reply = tp20.requestSeed(secLvl, tmpBuffer);//then we request a new seed
														if((reply & 0xFFFF) != 0)//if we got a positive reply
														{
															if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) != 0)//if they are different
															{
																validSession=w;
																isDifferent=true;
																break;
															}
														}
													}
												}
											}
											if(isDifferent == false)//if it didnt work
											{
												requestSeedInNewSession=false;//we know it doesnt work. right? or not? oh man, this is stressing...
											}
										}
										else
										{
											if(requestSeedInNewSession == true)
											{
												wait_ms(timeDelay);
												reply = tp20.startComms(tmpBuffer,validSession);//we change to a different session to try reset secaccess
												if(reply == 0)
												{
													doesChangingSessionWork=false;
													break;
												}
												wait_ms(timeDelay);
												reply = tp20.requestSeed(secLvl, tmpBuffer);//then we request a new seed
												if((reply & 0xFFFF) != 0)//if we got a positive reply
												{
													if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) != 0)//if they are different
													{
														wait_ms(timeDelay);
														tp20.startComms(tmpBuffer,currentDiagSession);//we switch back to the normal one
														isDifferent=true;
														break;
													}
												}
												wait_ms(timeDelay);
												tp20.startComms(tmpBuffer,currentDiagSession);//we switch back to the normal one
												requestSeedInNewSession=false;//if we got here, it failed
											}
											else
											{
												wait_ms(timeDelay);
												tp20.startComms(tmpBuffer,currentDiagSession + 2);//we change to a different session to try reset secaccess
												if(reply == 0)
												{
													doesChangingSessionWork=false;
													break;
												}
												wait_ms(timeDelay);
												tp20.startComms(tmpBuffer,currentDiagSession);//we switch back to the normal one
												wait_ms(timeDelay);
												reply = tp20.requestSeed(secLvl, tmpBuffer);//then we request a new seed
												if((reply & 0xFFFF) != 0)//if we got a positive reply
												{
													if(memcmp(tmpSeed, (tmpBuffer  + 2), 4) != 0)//if they are different
													{
														isDifferent=true;
														break;
													}
												}
												doesChangingSessionWork=false;//else, it didnt work
											}
										}
									}
									else//the DUT is not vulnerable
									{
										oled.clearScreen();
										oled.displayMessage("This DUT is not");
										oled.displayMessage(" vulnerable to",1);
										oled.displayMessage("  SecHammer or",1);
										oled.displayMessage("uses static seed",1);
										oled.displayMessage("  for requested",1);
										oled.displayMessage(" Security level",1);
										buttons.getButtonPressed();
										failed=true;
										break;
									}
								}
							}
							if(isDifferent==true)
							{
								for(uint8_t q=0; q<4; q++)//copy the seed
								{
									tmpSeed[q]=tmpBuffer[(q+2)];
								}
								sd.write((char*)tmpSeed,4);//we write it to the file
								numberOfSamples++;
							}
							if(failed == true)
							{
								break;
							}
						}
						sd.closeFile();
						tp20.closeChannel();
						if(failed == true)
						{
							sd.deleteFile(filename);
						}
						else
						{
							oled.clearScreen();
							oled.displayMessage("Cleaning up..");
							char filename2[90]="/Logging/TP20/Hammer/";
							char tmpBuf[90];
							memset(tmpBuf,0,90);
							sprintf(tmpBuf,"ID_0x%x_",ecuID);
							strcat (filename2,tmpBuf);
							memset(tmpBuf,0,90);
							sprintf(tmpBuf,"LVL_%x_",secLvl);
							strcat (filename2,tmpBuf);
							char fExt[6]=".CSV";
							if(!sd.getSequencialFileName(filename2, fExt))
							{
								oled.clearScreen();
								oled.displayMessage(" Filename Error ");
								oled.displayMessage("****************",1);
								oled.displayMessage("  If limit of",1);
								oled.displayMessage(" 65535 files in",1);
								oled.displayMessage("  folder is",1);
								oled.displayMessage("  reached this",1);
								oled.displayMessage("  will happen",1);
								oled.displayMessage("****************",1);
								buttons.getButtonPressed();
								break;
							}
							if (!sd.openFile(filename2, O_WRONLY | O_CREAT | O_TRUNC) || !sd.openFile(filename, O_RDONLY,2))//if we couldnt create the new file or open the log
							{
								oled.displayMessage("SD error...");
								buttons.getButtonPressed();
								break;
							}
							for(uint32_t i=0; i< numberOfSamples; i++)
							{
								uint8_t y=sd.read((char*)tmpBuffer,4,2);
								if(y != 4)//if we reached the end of the file for some reason before we got all the samples
								{
									break;
								}
								else
								{
									uint32_t u=tmpBuffer[0];
									u = (u << 8) + tmpBuffer[1];
									u = (u << 8) + tmpBuffer[2];
									u = (u << 8) + tmpBuffer[3];
									convert.itox(u,z,8);
									sd.write(z,8,1);
									char bs[6]=",\n";
									if(i != (numberOfSamples - 1))//we dont write the last one
									{
										sd.write(bs,2,1);
									}
								}
							}
							sd.closeFile();
							sd.closeFile(2);
							sd.deleteFile(filename);
							oled.clearScreen();
							oled.displayMessage("Log saved in:");
							oled.displayMessage(filename2,1);
							if(sd.doesFileExist(filename))//delete the temp file
							{
								sd.deleteFile(filename);
							}
							buttons.getButtonPressed();
						}
					}
					optChanged=true;
				}
			}
			else if (a == 2)//One option down, the "+" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 6)//in order to not overwrite the header
				{
					b = 1;
				}
				else
				{
					b++;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 3)//One option up, the "-" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 1)//in order to not overwrite the header
				{
					b = 6;
				}
				else
				{
					b--;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 4)
			{
				return;
			}
		}
	}
}




void CANbadger::UDSSecurityHijackMenu()
{
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	uint32_t ownID=0;
	uint32_t rID=0;
	uint8_t level=0;
	uint8_t diagSession=0;
//	uint8_t addressType=0;//used to store the addressing type
	CANFormat formato=CANStandard;
	setCANBadgerStatus(CAN1_STANDARD,1);//SET EVERYTHING TO STANDARD
	setCANBadgerStatus(CAN1_EXTENDED,0);
	setCANBadgerStatus(CAN2_STANDARD,1);
	setCANBadgerStatus(CAN2_EXTENDED,0);
	setCANBadgerStatus(CAN1_USE_FULLFRAME,0);
	setCANBadgerStatus(CAN2_USE_FULLFRAME,0);
	bool usePadding = false;
	uint8_t paddingByte=0;
	while(1)
	{
		oled.clearScreen();
		oled.displayMessage(" UDS Sec Hijack ");//show the header
		oled.displayMessage(" tID: 0x",1);
		char z[22];
		convert.itox(ownID,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" rID: 0x",1);
		convert.itox(rID,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Level: ",1);
		if(level == 0)
		{
			z[0]='A';
			z[1]='N';
			z[2]='Y';
			z[3]=0;
		}
		else
		{
			oled.displayMessage("0x",0,1);
			convert.itoa(level,z,2);
		}
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Session: ",1);
		if(diagSession == 0)
		{
			z[0]='O';
			z[1]='F';
			z[2]='F';
			z[3]=0;
		}
		else
		{
			oled.displayMessage("0x",0,1);
			convert.itoa(diagSession,z,2);
		}
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Format: ",1);//show if standard or extended format is being used
		if(formato == CANStandard)
		{
			oled.displayMessage("STD",0,1);
		}
		else
		{
			oled.displayMessage("XTD",0,1);
		}
		oled.displayMessage(" Padding: ",1);
		if(usePadding == true)
		{
				convert.itox(CAN1PaddingByte,z,2);
		}
		else
		{
			z[0]='O';
			z[1]='F';
			z[2]='F';
			z[3]=0;
		}		
		oled.displayMessage(z,0,1);
/*		oled.displayMessage(" Addressing: ",1);
		if(addressType == 0)
		{
			z[0]='S';
			z[1]='T';
			z[2]='D';
			z[3]=0;
		}
		else
		{
			z[0]='X';
			z[1]='T';
			z[2]='D';
			z[3]=0;
		}	
		oled.displayMessage(z,0,1);	*/
		oled.displayMessage(" GO!",1);
		oled.set_text_rc(chosenOption,0);//we set the cursor on current highlight
		oled.putc(arrow);
		bool optChanged=false;
		while(!optChanged)
		{
			uint8_t a = buttons.getButtonPressed();//check which button was pressed
			if(a == 0 || a == 4)//back button
			{
				return;
			}
			if(a == 1)//start button
			{
				if(chosenOption == 1)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=0;
					uint32_t tmplocalID=0;
					if(formato == CANStandard)
					{
						tmplen=2;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					else
					{
						tmplen=4;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x1F,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					oled.clearScreen();
					oled.displayMessage("Entered ID:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						ownID = tmplocalID;
					}
					optChanged=true;
				}
				else if(chosenOption == 2)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=0;
					uint32_t tmplocalID=0;
					if(formato == CANStandard)
					{
						tmplen=2;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					else
					{
						tmplen=4;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x1F,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					oled.clearScreen();
					oled.displayMessage("Entered ID:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						rID = tmplocalID;
					}
					optChanged=true;
				}
				else if(chosenOption == 3)
				{
					if(getDecValue("Enter Sec Lvl:", (uint64_t*)(&level),0,0xFF,1))
					{
						optChanged=true;
					}
				}
				else if(chosenOption == 4)
				{
					if(getDecValue("Enter Session:", (uint64_t*)(&diagSession),0,0xFF,1))
					{
						optChanged=true;
					}
				}		
				else if(chosenOption == 5)
				{
					if(formato == CANStandard)
					{
						formato = CANExtended;
						setCANBadgerStatus(CAN1_STANDARD,0);
						setCANBadgerStatus(CAN1_EXTENDED,1);
						setCANBadgerStatus(CAN2_STANDARD,0);
						setCANBadgerStatus(CAN2_EXTENDED,1);
					}	
					else
					{
						formato = CANStandard;
						setCANBadgerStatus(CAN1_STANDARD,1);
						setCANBadgerStatus(CAN1_EXTENDED,0);
						setCANBadgerStatus(CAN2_STANDARD,1);
						setCANBadgerStatus(CAN2_EXTENDED,0);
					}
					optChanged=true;
				}
				else if(chosenOption == 6)
				{
					if(usePadding)
					{
						usePadding = false;
						setCANBadgerStatus(CAN1_USE_FULLFRAME,0);
						setCANBadgerStatus(CAN2_USE_FULLFRAME,0);
					}
					else
					{
						usePadding = true;
						setCANBadgerStatus(CAN1_USE_FULLFRAME,1);
						setCANBadgerStatus(CAN2_USE_FULLFRAME,1);						
						getHexValue("Enter padding:",(uint64_t*)(&paddingByte),0,0xFF,1);
						CAN1PaddingByte	= paddingByte;
						CAN2PaddingByte	= paddingByte;
						
					}
					optChanged=true;
				}

/*				else if(chosenOption == 7)
				{
					if(addressType == 0)
					{
						addressType = 1;
					}
					else
					{
						addressType = 0;					
					}
					optChanged=true;
				}*/
				else if(chosenOption == 7)
				{
					uint8_t r = UDSSecurityHijack(ownID, rID, level, diagSession);
					if(r != 0)//if we did hijack a session
					{
						localID = ownID;//set the UDS for the uds menu
						remoteID = rID;
						if(diagSession != 0)//if a type of diag session was chosen
						{
							currentDiagSession = diagSession;
						}
						doOLEDUDS(&can1, 1, true);
					}
					optChanged=true;
				}
			}			
			else if (a == 2)//One option down, the "+" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 7)//in order to not overwrite the header
				{
					b = 1;
				}
				else
				{
					b++;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 3)//One option up, the "-" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 1)//in order to not overwrite the header
				{
					b = 7;
				}
				else
				{
					b--;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}	
		}
	}
}

void CANbadger::TP20SecurityHijackMenu()
{
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	uint32_t ownID=0;
	uint32_t rID=0;
	uint8_t level=0;
	setCANBadgerStatus(CAN1_STANDARD,1);//SET EVERYTHING TO STANDARD
	setCANBadgerStatus(CAN1_EXTENDED,0);
	setCANBadgerStatus(CAN2_STANDARD,1);
	setCANBadgerStatus(CAN2_EXTENDED,0);
	setCANBadgerStatus(CAN1_USE_FULLFRAME,0);
	setCANBadgerStatus(CAN2_USE_FULLFRAME,0);
	bool usePadding = false;
	uint8_t paddingByte=0;
	while(1)
	{
		oled.clearScreen();
		oled.displayMessage("TP20 Sec Hijack ");//show the header
		oled.displayMessage(" tID: 0x",1);
		char z[22];
		convert.itox(ownID,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" rID: 0x",1);
		convert.itox(rID,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Level: ",1);
		if(level == 0)
		{
			z[0]='A';
			z[1]='N';
			z[2]='Y';
			z[3]=0;
		}
		else
		{
			oled.displayMessage("0x",0,1);
			convert.itoa(level,z,2);
		}
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Padding: ",1);
		if(usePadding == true)
		{
				convert.itox(CAN1PaddingByte,z,2);
		}
		else
		{
			z[0]='O';
			z[1]='F';
			z[2]='F';
			z[3]=0;
		}
		oled.displayMessage(z,0,1);
		oled.displayMessage(" GO!",1);
		oled.set_text_rc(chosenOption,0);//we set the cursor on current highlight
		oled.putc(arrow);
		bool optChanged=false;
		while(!optChanged)
		{
			uint8_t a = buttons.getButtonPressed();//check which button was pressed
			if(a == 0 || a == 4)//back button
			{
				return;
			}
			if(a == 1)//start button
			{
				if(chosenOption == 1)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=2;
					uint32_t tmplocalID=0;
					for(uint8_t a = 0; a < tmplen; a++)//now we get the length
					{
						char tmpstrng[32]="Enter MSB";
						char tmpstrng2[6];
						convert.itox((a + 1),tmpstrng2,1);
						strcat(tmpstrng,tmpstrng2);
						strcat(tmpstrng,":");
						if(a==0)
						{
							if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
							{
								break;
							}
						}
						else
						{
							if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
							{
								break;
							}
						}
						tmplocalID = (tmplocalID << 8) + tmpbyte;
					}
					oled.clearScreen();
					oled.displayMessage("Entered ID:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						ownID = tmplocalID;
					}
					optChanged=true;
				}
				else if(chosenOption == 2)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=2;
					uint32_t tmplocalID=0;
					tmplen=2;
					for(uint8_t a = 0; a < tmplen; a++)//now we get the length
					{
						char tmpstrng[32]="Enter MSB";
						char tmpstrng2[6];
						convert.itox((a + 1),tmpstrng2,1);
						strcat(tmpstrng,tmpstrng2);
						strcat(tmpstrng,":");
						if(a==0)
						{
							if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
							{
								break;
							}
						}
						else
						{
							if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
							{
								break;
							}
						}
						tmplocalID = (tmplocalID << 8) + tmpbyte;
					}
					oled.clearScreen();
					oled.displayMessage("Entered ID:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						rID = tmplocalID;
					}
					optChanged=true;
				}
				else if(chosenOption == 3)
				{
					if(getDecValue("Enter Sec Lvl:", (uint64_t*)(&level),0,0xFF,1))
					{
						optChanged=true;
					}
				}
				else if(chosenOption == 4)
				{
					if(usePadding == true)
					{
						usePadding = false;
						setCANBadgerStatus(CAN1_USE_FULLFRAME,0);
						setCANBadgerStatus(CAN2_USE_FULLFRAME,0);
					}
					else
					{
						usePadding = true;
						setCANBadgerStatus(CAN1_USE_FULLFRAME,1);
						setCANBadgerStatus(CAN2_USE_FULLFRAME,1);
						getHexValue("Enter padding:",(uint64_t*)(&paddingByte),0,0xFF,1);
						CAN1PaddingByte	= paddingByte;
						CAN2PaddingByte	= paddingByte;

					}
					optChanged=true;
				}
				else if(chosenOption == 5)
				{
					uint16_t r = TP20SecurityHijack(ownID, rID, level);
					if(r != 0)//if we did hijack a session
					{
						localID = ownID;//set the UDS for the uds menu
						remoteID = rID;
						r=(r>>8);
						doOLEDTP20(&can1, 1, true, (uint8_t)r);
					}
					optChanged=true;
				}
			}
			else if (a == 2)//One option down, the "+" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 7)//in order to not overwrite the header
				{
					b = 1;
				}
				else
				{
					b++;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 3)//One option up, the "-" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 1)//in order to not overwrite the header
				{
					b = 7;
				}
				else
				{
					b--;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
		}
	}
}

void CANbadger::KWP2KSecurityHijackMenu()
{
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	uint32_t ownID=0;
	uint32_t rID=0;
	uint8_t level=0;
	setCANBadgerStatus(CAN1_STANDARD,1);//SET EVERYTHING TO STANDARD
	setCANBadgerStatus(CAN1_EXTENDED,0);
	setCANBadgerStatus(CAN2_STANDARD,1);
	setCANBadgerStatus(CAN2_EXTENDED,0);
	setCANBadgerStatus(CAN1_USE_FULLFRAME,0);
	setCANBadgerStatus(CAN2_USE_FULLFRAME,0);
	bool usePadding = false;
	uint8_t paddingByte=0;
	while(1)
	{
		oled.clearScreen();
		oled.displayMessage("KWP2K Sec Hijack");//show the header
		oled.displayMessage(" tID: 0x",1);
		char z[22];
		convert.itox(ownID,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" rID: 0x",1);
		convert.itox(rID,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Level: ",1);
		if(level == 0)
		{
			z[0]='A';
			z[1]='N';
			z[2]='Y';
			z[3]=0;
		}
		else
		{
			oled.displayMessage("0x",0,1);
			convert.itoa(level,z,2);
		}
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Padding: ",1);
		if(usePadding == true)
		{
				convert.itox(CAN1PaddingByte,z,2);
		}
		else
		{
			z[0]='O';
			z[1]='F';
			z[2]='F';
			z[3]=0;
		}
		oled.displayMessage(z,0,1);
		oled.displayMessage(" GO!",1);
		oled.set_text_rc(chosenOption,0);//we set the cursor on current highlight
		oled.putc(arrow);
		bool optChanged=false;
		while(!optChanged)
		{
			uint8_t a = buttons.getButtonPressed();//check which button was pressed
			if(a == 0 || a == 4)//back button
			{
				return;
			}
			if(a == 1)//start button
			{
				if(chosenOption == 1)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=2;
					uint32_t tmplocalID=0;
					for(uint8_t a = 0; a < tmplen; a++)//now we get the length
					{
						char tmpstrng[32]="Enter MSB";
						char tmpstrng2[6];
						convert.itox((a + 1),tmpstrng2,1);
						strcat(tmpstrng,tmpstrng2);
						strcat(tmpstrng,":");
						if(a==0)
						{
							if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
							{
								break;
							}
						}
						else
						{
							if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
							{
								break;
							}
						}
						tmplocalID = (tmplocalID << 8) + tmpbyte;
					}
					oled.clearScreen();
					oled.displayMessage("Entered ID:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						ownID = tmplocalID;
					}
					optChanged=true;
				}
				else if(chosenOption == 2)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=2;
					uint32_t tmplocalID=0;
					tmplen=2;
					for(uint8_t a = 0; a < tmplen; a++)//now we get the length
					{
						char tmpstrng[32]="Enter MSB";
						char tmpstrng2[6];
						convert.itox((a + 1),tmpstrng2,1);
						strcat(tmpstrng,tmpstrng2);
						strcat(tmpstrng,":");
						if(a==0)
						{
							if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
							{
								break;
							}
						}
						else
						{
							if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
							{
								break;
							}
						}
						tmplocalID = (tmplocalID << 8) + tmpbyte;
					}
					oled.clearScreen();
					oled.displayMessage("Entered ID:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						rID = tmplocalID;
					}
					optChanged=true;
				}
				else if(chosenOption == 3)
				{
					if(getDecValue("Enter Sec Lvl:", (uint64_t*)(&level),0,0xFF,1))
					{
						optChanged=true;
					}
				}
				else if(chosenOption == 4)
				{
					if(usePadding == true)
					{
						usePadding = false;
						setCANBadgerStatus(CAN1_USE_FULLFRAME,0);
						setCANBadgerStatus(CAN2_USE_FULLFRAME,0);
					}
					else
					{
						usePadding = true;
						setCANBadgerStatus(CAN1_USE_FULLFRAME,1);
						setCANBadgerStatus(CAN2_USE_FULLFRAME,1);
						getHexValue("Enter padding:",(uint64_t*)(&paddingByte),0,0xFF,1);
						CAN1PaddingByte	= paddingByte;
						CAN2PaddingByte	= paddingByte;

					}
					optChanged=true;
				}
				else if(chosenOption == 5)
				{
					uint16_t r = KWP2KSecurityHijack(ownID, rID, level);
					if(r != 0)//if we did hijack a session
					{
						localID = ownID;//set the UDS for the uds menu
						remoteID = rID;
						r=(r>>8);
						doOLEDKWP2K(&can1, 1, true);
					}
					optChanged=true;
				}
			}
			else if (a == 2)//One option down, the "+" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 7)//in order to not overwrite the header
				{
					b = 1;
				}
				else
				{
					b++;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 3)//One option up, the "-" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 1)//in order to not overwrite the header
				{
					b = 7;
				}
				else
				{
					b--;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
		}
	}
}

uint16_t CANbadger::TP20SecurityHijack(uint32_t ownID, uint32_t rID, uint8_t level)
{
	oled.clearScreen();
	oled.displayMessage("Waiting...");
	CANFormat format = CANStandard;
	CANMessage can1_msg(0,format);
	CANMessage can2_msg(0,format);
	uint8_t pwn=0;//used as a counter for actions
	uint8_t lvl = 0;//to grab the security level
	uint8_t cnnt=0;//to count frames and discard a false channel negociation
	uint16_t tpCounter=0;//used to grab the current counter
	while(buttons.isButtonPressed(4) == false)//we will wait until we find a SA or the back button is pressed
	{
		if(can1.read(can1_msg))
		{
			if(can1_msg.id == rID)//target side
			{
				if(pwn == 1)//grab the reply to request
				{
					if(can1_msg.data[3] == 0x67 && can1_msg.data[4] == lvl)//seed
					{
						pwn++;
					}
				}
				else if(pwn == 3)//grab the reply to key
				{
					if(can1_msg.data[1] == 0x0 && can1_msg.data[3] == 0x67 && can1_msg.data[4] == (lvl + 1))
					{
						uint8_t tmpCounter=((can1_msg.data[0] + 1) & 0x0F) + 0xB0;//grab the counter to send the ack
						char tSend[8]={tmpCounter};
						wait(0.01);
						uint8_t timeout=0;
						while (!can1.write(CANMessage(ownID, tSend, 1)) && timeout < 100)//send the ACK
						{
							wait(0.0001);
							timeout++;
						}//make sure the msg goes out
						return lvl + tpCounter;//we return the level we got access to and the counter
					}
					else if(can1_msg.data[2] == 0x3 && can1_msg.data[3] == 0x7F && can1_msg.data[4] == 0x27)//authentication failed
					{
						pwn = 0;
						cnnt = 0;
					}
				}
				if(pwn > 0)
				{
					cnnt++;
					if (cnnt > 50)//timeout
					{
						pwn = 0;
						cnnt = 0;
					}
				}
			}
			uint8_t timeout=0;
			while (!can2.write(CANMessage(can1_msg.id, reinterpret_cast<char*>(&can1_msg.data), can1_msg.len)) && timeout < 100)
			{
				wait(0.0001);
				timeout++;
			}//make sure the msg goes out
		}
		if(can2.read(can2_msg))//tool side
		{
			if(can2_msg.id == ownID)
			{
				if(can2_msg.data[1] == 0x0 && can2_msg.data[2] == 0x02 && can2_msg.data[3] == 0x10 && can2_msg.len >= 5)//if a start diag session was requested
				{
					currentDiagSession = can2_msg.data[4];//we grab the session type for later use
				}
				if(pwn  == 0)//first we need to grab the request
				{
					if(can2_msg.data[1] == 0x0 && can2_msg.data[2] == 0x02 && can2_msg.data[3] == 0x27 && level == 0)
					{
						pwn++;
						lvl = can2_msg.data[4];
					}
					else if(can2_msg.data[1] == 0x0 && can2_msg.data[2] == 0x02 && can2_msg.data[3] == 0x27 && level == can2_msg.data[4])
					{
						pwn++;
						lvl = can2_msg.data[4];
					}
				}
				else if(pwn == 2)//then we grab the reply from tool
				{
					if(can2_msg.data[2] == 0x6 && can2_msg.data[3] == 0x27 && can2_msg.data[4] == (lvl + 1))
					{
						pwn++;
						tpCounter = (can2_msg.data[0] & 0x0F);//grab the counter
						tpCounter = (tpCounter + 2);//add 1 because there will be an additional frame
						tpCounter = tpCounter & 0x0F;//get only the lower part in case it did overflow
						tpCounter = (tpCounter << 8); //shift it so we return it later
					}
				}
				if(pwn > 0)
				{
					cnnt++;
					if (cnnt > 100)//timeout
					{
						pwn = 0;
						cnnt = 0;
					}
				}
			}
			uint8_t timeout=0;
			while (!can1.write(CANMessage(can2_msg.id, reinterpret_cast<char*>(&can2_msg.data), can2_msg.len)) && timeout < 100)
			{
				wait(0.0001);
				timeout++;
			}//make sure the msg goes out
		}
	}
	oled.clearScreen();
	oled.displayMessage("Aborted");
	buttons.getButtonPressed();
	return 0;
}

uint16_t CANbadger::KWP2KSecurityHijack(uint32_t ownID, uint32_t rID, uint8_t level)
{
	oled.clearScreen();
	oled.displayMessage("Waiting...");
	CANFormat format = CANStandard;
	CANMessage can1_msg(0,format);
	CANMessage can2_msg(0,format);
	uint8_t pwn=0;//used as a counter for actions
	uint8_t lvl = 0;//to grab the security level
	uint8_t cnnt=0;//to count frames and discard a false channel negotiation
	uint16_t tpCounter=0;//used to grab the current counter
	while(buttons.isButtonPressed(4) == false)//we will wait until we find a SA or the back button is pressed
	{
		if(can1.read(can1_msg))
		{
			if(can1_msg.id == rID)//target side
			{
				if(pwn == 1)//grab the reply to request
				{
					if(can1_msg.data[1] == 0x67 && can1_msg.data[2] == lvl)//seed
					{
						pwn++;
					}
				}
				else if(pwn == 3)//grab the reply to key
				{
					if(can1_msg.data[1] == 0x67 && can1_msg.data[2] == (lvl + 1))
					{
						return lvl;//we return the level we got access to. maybe return session type too?
					}
					else if(can1_msg.data[0] == 0x3 && can1_msg.data[1] == 0x7F && can1_msg.data[2] == 0x27)//authentication failed
					{
						pwn = 0;
						cnnt = 0;
					}
				}
				if(pwn > 0)
				{
					cnnt++;
					if (cnnt > 50)//timeout
					{
						pwn = 0;
						cnnt = 0;
					}
				}
			}
			uint8_t timeout=0;
			while (!can2.write(CANMessage(can1_msg.id, reinterpret_cast<char*>(&can1_msg.data), can1_msg.len)) && timeout < 100)
			{
				wait(0.0001);
				timeout++;
			}//make sure the msg goes out
		}
		if(can2.read(can2_msg))//tool side
		{
			if(can2_msg.id == ownID)
			{
				if(can2_msg.data[1] == 0x10 && (can2_msg.data[0] & 0xF0) == 0x0  && can2_msg.len >= 2)//if a start diag session was requested
				{
					currentDiagSession = can2_msg.data[2];//we grab the session type for later use
				}
				if(pwn  == 0)//first we need to grab the request
				{
					if(can2_msg.data[0] >= 0x02 &&  can2_msg.data[1] == 0x27 && level == 0)
					{
						pwn++;
						lvl = can2_msg.data[2];
					}
					else if(can2_msg.data[0] >= 0x02 && can2_msg.data[1] == 0x27 && level == can2_msg.data[2])
					{
						pwn++;
						lvl = can2_msg.data[2];
					}
				}
				else if(pwn == 2)//then we grab the reply from tool
				{
					if(can2_msg.data[0] == 0x6 && can2_msg.data[1] == 0x27 && can2_msg.data[2] == (lvl + 1))
					{
						pwn++;
					}
				}
				if(pwn > 0)
				{
					cnnt++;
					if (cnnt > 100)//timeout
					{
						pwn = 0;
						cnnt = 0;
					}
				}
			}
			uint8_t timeout=0;
			while (!can1.write(CANMessage(can2_msg.id, reinterpret_cast<char*>(&can2_msg.data), can2_msg.len)) && timeout < 100)
			{
				wait(0.0001);
				timeout++;
			}//make sure the msg goes out
		}
	}
	oled.clearScreen();
	oled.displayMessage("Aborted");
	buttons.getButtonPressed();
	return 0;
}


uint8_t CANbadger::UDSSecurityHijack(uint32_t ownID, uint32_t rID, uint8_t level, uint8_t sessionType)
{
	oled.clearScreen();
	oled.displayMessage("Waiting...");
	CANFormat format;
	if(getCANBadgerStatus(CAN1_STANDARD) == true)
	{
		format = CANStandard;
	}
	else
	{
		format = CANExtended;
	}
	CANMessage can1_msg(0,format);
	if(getCANBadgerStatus(CAN2_STANDARD) == true)
	{
		format = CANStandard;
	}
	else
	{
		format = CANExtended;
	}
	CANMessage can2_msg(0,format);
	uint8_t pwn=0;//used as a counter for actions
	uint8_t lvl = 0;//to grab the security level
	uint8_t cnnt=0;//to count frames and discard a false channel negociation
	uint32_t seed=0;
	uint32_t key=0;
	while(buttons.isButtonPressed(4) == false)//we will wait until we find a SA or the back button is pressed
	{
		if(can1.read(can1_msg))
		{
			if(can1_msg.id == rID)//target side
			{
				if(pwn == 1)//grab the reply to request
				{
					if((can1_msg.data[1] == 0x67 && can1_msg.data[2] == lvl) || (can1_msg.data[2] == 0x67 && can1_msg.data[3] == lvl))//single and multiframe
					{
						seed = can1_msg.data[3];
						seed = ((seed << 8) + can1_msg.data[4]);
						seed = ((seed << 8) + can1_msg.data[5]);
						seed = ((seed << 8) + can1_msg.data[6]);
						pwn++;
					}
				}
				else if(pwn == 3)//grab the reply to key
				{
					if((can1_msg.data[1] == 0x67 && can1_msg.data[2] == (lvl + 1)) || (can1_msg.data[2] == 0x67 && can1_msg.data[3] == (lvl + 1)))
					{
						return lvl;//we return the level we got access to
					}
					else if(can1_msg.data[0] == 0x3 && can1_msg.data[1] == 0x7F && can1_msg.data[2] == 0x27 && can1_msg.data[3] == 0x35)//authentication failed
					{
						pwn = 0;
						seed = 0;
						key = 0;
						cnnt = 0;
					}
				}
				if(pwn > 0)
				{
					cnnt++;
					if (cnnt > 100)//timeout
					{
						pwn = 0;
						cnnt = 0;
					}
				}
			}
			uint8_t timeout=0;
			while (!can2.write(CANMessage(can1_msg.id, reinterpret_cast<char*>(&can1_msg.data), can1_msg.len)) && timeout < 100)
			{
				timeout++;
			}//make sure the msg goes out
		}
		if(can2.read(can2_msg))
		{
			if(can2_msg.id == rID)//target side
			{
				if(can2_msg.data[0] == 0x2 && can2_msg.data[1] == 0x10 && can2_msg.len >= 3)//if a start diag session was requested
				{
					currentDiagSession = can2_msg.data[2];//we grab the session type for later use
				}
				if(pwn  == 0)//first we need to grab the request
				{
					if(can2_msg.data[0] == 0x2 && can2_msg.data[1] == 0x27 && level == 0)
					{
						if(sessionType !=0)
						{
							uint8_t data[8]={0x02,0x10,sessionType,CAN2PaddingByte,CAN2PaddingByte,CAN2PaddingByte,CAN2PaddingByte,CAN2PaddingByte};
							uint32_t timeout=0;
							if(getCANBadgerStatus(CAN2_USE_FULLFRAME) == true)
							{
								while (!can2.write(CANMessage(ownID, reinterpret_cast<char*>(data), 8, CANData, format)) && timeout < 10000)
								{
									wait(0.0001);
									timeout++;
								}//make sure the msg goes out
							}
							else
							{
								while (!can2.write(CANMessage(ownID, reinterpret_cast<char*>(data), 3, CANData, format)) && timeout < 10000)
								{
									wait(0.0001);
									timeout++;
								}//make sure the msg goes out
							}
						}
						pwn++;
						lvl = can2_msg.data[2];
					}
					else if(can2_msg.data[0] == 0x2 && can2_msg.data[1] == 0x27 && level == can2_msg.data[2])
					{
						pwn++;
						lvl = can2_msg.data[2];
					}
				}
				else if(pwn == 2)//then we grab the reply from tool
				{
					if(can2_msg.data[0] == 0x6 && can2_msg.data[1] == 0x27 && can2_msg.data[2] == (lvl + 1))
					{
						key = can2_msg.data[3];
						key = ((key << 8) + can2_msg.data[4]);
						key = ((key << 8) + can2_msg.data[5]);
						key = ((key << 8) + can2_msg.data[6]);
						pwn++;
					}
				}
				if(pwn > 0)
				{
					cnnt++;
					if (cnnt > 100)//timeout
					{
						pwn = 0;
						cnnt = 0;
					}
				}
			}
			uint8_t timeout=0;
			while (!can1.write(CANMessage(can2_msg.id, reinterpret_cast<char*>(&can2_msg.data), can2_msg.len)) && timeout < 100)
			{
			 timeout++;
			}//make sure the msg goes out
		}
	}
	oled.clearScreen();
	oled.displayMessage("Aborted");
	buttons.getButtonPressed();
	return 0;
}

void CANbadger::ScanActiveUDSIDs()//need to add support for extended addressing (NOT extended ID)
{

	/**********adjust this to new file method by using can bridge and all that stuff*/////
	oled.clearScreen();
	if(isSDInserted == false)//if there is no SD
	{
		oled.displayMessage("SD not Inserted");
		buttons.getButtonPressed();
		return;
	}
	char filename[90]="/tmpfile.raw";
	if(sd.doesFileExist(filename))//delete the temp file if it already exists
	{
		sd.deleteFile(filename);
	}
	if (!sd.openFile(filename, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
	{
		oled.displayMessage("SD Write error");
		buttons.getButtonPressed();
		return;
	}
	bool wasCANBridgeEnabled=false;
	if(getCANBadgerStatus(CAN1_LOGGING) == true || getCANBadgerStatus(CAN2_LOGGING) == true)//we need the user to not be already logging
	{
		oled.displayMessage("Disable logging");
		oled.displayMessage("  on both CAN",1);
		oled.displayMessage("   and retry",1);
		buttons.getButtonPressed();
		return;
	}
	setCANBadgerStatus(CAN1_LOGGING,1);
	setCANBadgerStatus(CAN2_LOGGING,1);
	if(getCANBadgerStatus(CAN_BRIDGE_ENABLED) == true)//check if CAN BRIDGE was already enabled
	{
		wasCANBridgeEnabled=true;
	}
	else
	{
		if(!CANBridge(1))
		{
			oled.displayMessage("Bridge error");
			buttons.getButtonPressed();
			return;//we are looking for a lot of conditions, but if somehow they are not met, then go back
		}
	}
	generalCounter1=0;//we initialize the buffer pointer for the interrupts and the pending packets
	generalCounter2=0;
	generalCounter3=0;//used as our buffer Pointer
	uint8_t data[269];//we will store the retrieved data from buffer here. 269 is the maximum length we will ever get
	oled.displayMessage("Logging traffic");
	oled.displayMessage("   Press back ",1);
	oled.displayMessage("    to stop   ",1);
	timer.stop();
	timer.reset();
	timer.start();//start it!
	while(buttons.isButtonPressed(4) == false)//log while the back button is not pressed
	{
		if(generalCounter2 > 0)//if we have pending data to retrieve from RAM
		{
			if(generalCounter3 == 4096)//if we have reached the end of the buffer space
			{
				generalCounter3 = 0;//reset it to make it circular!
			}
			readTmpBuffer(generalCounter3, 1, data);
			while(data[0] == 0)//to skip bytes intentionally left blank or if there was a read attempt that was interrupted
			{
				generalCounter3++;//we position the counter on the next byte
				if(generalCounter3 == 4096)//if we have reached the end of the buffer space
				{
					generalCounter3 = 0;//reset it to make it circular!
				}
				readTmpBuffer(generalCounter3, 1, data);
			}
			readTmpBuffer((generalCounter3 + 13),1,data);//retrieve the length of the payload
			uint32_t pLen=(data[0] + 14);//calculate the total length of the packet including header
			readTmpBuffer(generalCounter3,pLen,data);
			generalCounter3 = (generalCounter3 + pLen);//add the total length, and substract one so we are pointing at the last byte of the previous packet
			sd.write((char*)data,pLen);//write the data to the file
			generalCounter2--;//remove the packet we just wrote from the queue
		}
	}
	setCANBadgerStatus(CAN1_LOGGING,0);//now we will disable logging so we can process the pending frames
	setCANBadgerStatus(CAN2_LOGGING,0);
	while(generalCounter2 > 0)//if we have pending data to retrieve from buffer after stopping the logger
	{
		if(generalCounter3 == 4096)//if we have reached the end of the buffer space
		{
			generalCounter3 = 0;//reset it to make it circular!
		}
		readTmpBuffer(generalCounter3, 1, data);
		while(data[0] == 0)//to skip bytes intentionally left blank
		{
			generalCounter3++;//we position the counter on the next byte
			if(generalCounter3 == 4096)//if we have reached the end of the buffer space
			{
				generalCounter3 = 0;//reset it to make it circular!
			}
			readTmpBuffer(generalCounter3, 1, data);
		}
		readTmpBuffer((generalCounter3 + 13),1,data);//retrieve the length of the payload
		uint32_t pLen=(data[0] + 14);//calculate the total length of the packet including header
		readTmpBuffer(generalCounter3,pLen,data);
		generalCounter3 = (generalCounter3 + pLen);//add the total length, and substract one so we are pointing at the last byte of the previous packet
		sd.write((char*)data,pLen);//write the data to the file
		generalCounter2--;//remove the packet we just wrote from the queue
	}
	if(wasCANBridgeEnabled == false)//if bridge was not enabled before logging, we disable it
	{
		CANBridge(0);
	}
	sd.closeFile();//close the log to save the file
	oled.clearScreen();
	oled.displayMessage("Parsing data...");
	oled.displayMessage("   Press back ",1);
	oled.displayMessage("    to stop   ",1);
	DiagSCAN scan(&can1);
	uint32_t IDs[256];
	memset(IDs,0,256);
	uint32_t result=scan.ScanActiveUDSIDsFromLog(IDs, filename, &sd, &buttons);
	oled.clearScreen();
    if(result ==0)//if there are no UDS IDs
    {
    	oled.displayMessage("No UDS found");
    }
    else if(result == 0xFFFFFFFF)//if there was no traffic
    {
    	oled.displayMessage("No traffic found");
    }
    else//create code to parse the list and create a file with it, then display the file on the screen
    {
    	oled.displayMessage("Creating log...");
    	memset(filename,0,90);
    	strcat(filename, "/Logging/UDS/UDSActiveScan_");
    	char fileExtension[6]=".txt";
    	if(!sd.getSequencialFileName(filename, fileExtension))
    	{
    		oled.displayMessage(" Filename Error ",1);
    		oled.displayMessage("****************",1);
    		oled.displayMessage("  If limit of",1);
    		oled.displayMessage(" 65535 files in",1);
    		oled.displayMessage(" logs folder is",1);
    		oled.displayMessage("  reached this",1);
    		oled.displayMessage("  will happen",1);
    		oled.displayMessage("****************",1);
    		buttons.getButtonPressed();
    		return;
    	}
    	if (!sd.openFile(filename, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
    	{
    		oled.displayMessage("SD Write error",1);
    		buttons.getButtonPressed();
    		return;
    	}
    	sd.write("**CANBadger Active UDS Scan Log**\n",34);
    	sd.write("**Format is Tester->ECU**\n\n",27);
    	for(uint32_t a=0; a < result; a++)
    	{
    		uint32_t id1=IDs[(2*a)];
    		uint32_t id2=IDs[((2*a)+1)];
    		char z[22];
    		uint8_t writeSize=0;//used to know the length of the ID
    		memset(z,0,22);
    		if(id2 == 0)//if it is a possible but we didnt see who it was talking to
    		{
    			sd.write("Possible UDS ID->0x",19);
    			if(id1 > 0x7FF)//if extended ID
    			{
    				convert.itox(id1,z,8);
    				writeSize=8;
    			}
    			else
    			{
    				convert.itox(id1,z,3);
    				writeSize=3;
    			}
    			sd.write(z,writeSize);
    			sd.write("\n",1);
    		}
    		else
    		{
    			sd.write("0x",2);
    			if(id1 > 0x7FF)//if extended ID
    			{
    				convert.itox(id1,z,8);
    				writeSize=8;
    			}
    			else
    			{
    				convert.itox(id1,z,3);
    				writeSize=3;
    			}
    			sd.write(z,writeSize);
    			sd.write("->",2);
    			memset(z,0,22);
    			sd.write("0x",2);
    			if(id2 > 0x7FF)//if extended ID
    			{
    				convert.itox(id2,z,8);
    				writeSize=8;
    			}
    			else
    			{
    				convert.itox(id2,z,3);
    				writeSize=3;
    			}
    			sd.write(z,writeSize);
    			sd.write("\n",1);
    		}
    	}
    	sd.closeFile();
    	if(!displayFile(filename))
    	{
    		oled.displayMessage("Log open error",1);
    		buttons.getButtonPressed();
    		return;
    	}
    	oled.clearScreen();
    	oled.displayMessage("Log saved in:");
    	oled.displayMessage(filename,1);
    }
	buttons.getButtonPressed();
}

void CANbadger::ScanActiveKWP2KIDs()//need to add support for extended addressing (NOT extended ID)
{

	/**********adjust this to new file method by using can bridge and all that stuff*/////
	oled.clearScreen();
	if(isSDInserted == false)//if there is no SD
	{
		oled.displayMessage("SD not Inserted");
		buttons.getButtonPressed();
		return;
	}
	char filename[90]="/tmpfile.raw";
	if(sd.doesFileExist(filename))//delete the temp file if it already exists
	{
		sd.deleteFile(filename);
	}
	if (!sd.openFile(filename, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
	{
		oled.displayMessage("SD Write error");
		buttons.getButtonPressed();
		return;
	}
	bool wasCANBridgeEnabled=false;
	if(getCANBadgerStatus(CAN1_LOGGING) == true || getCANBadgerStatus(CAN2_LOGGING) == true)//we need the user to not be already logging
	{
		oled.displayMessage("Disable logging");
		oled.displayMessage("  on both CAN",1);
		oled.displayMessage("   and retry",1);
		buttons.getButtonPressed();
		return;
	}
	setCANBadgerStatus(CAN1_LOGGING,1);
	setCANBadgerStatus(CAN2_LOGGING,1);
	if(getCANBadgerStatus(CAN_BRIDGE_ENABLED) == true)//check if CAN BRIDGE was already enabled
	{
		wasCANBridgeEnabled=true;
	}
	else
	{
		if(!CANBridge(1))
		{
			oled.displayMessage("Bridge error");
			buttons.getButtonPressed();
			return;//we are looking for a lot of conditions, but if somehow they are not met, then go back
		}
	}
	generalCounter1=0;//we initialize the buffer pointer for the interrupts and the pending packets
	generalCounter2=0;
	generalCounter3=0;//used as our buffer Pointer
	uint8_t data[269];//we will store the retrieved data from buffer here. 269 is the maximum length we will ever get
	oled.displayMessage("Logging traffic");
	oled.displayMessage("   Press back ",1);
	oled.displayMessage("    to stop   ",1);
	timer.stop();
	timer.reset();
	timer.start();//start it!
	while(buttons.isButtonPressed(4) == false)//log while the back button is not pressed
	{
		if(generalCounter2 > 0)//if we have pending data to retrieve from RAM
		{
			if(generalCounter3 == 4096)//if we have reached the end of the buffer space
			{
				generalCounter3 = 0;//reset it to make it circular!
			}
			readTmpBuffer(generalCounter3, 1, data);
			while(data[0] == 0)//to skip bytes intentionally left blank or if there was a read attempt that was interrupted
			{
				generalCounter3++;//we position the counter on the next byte
				if(generalCounter3 == 4096)//if we have reached the end of the buffer space
				{
					generalCounter3 = 0;//reset it to make it circular!
				}
				readTmpBuffer(generalCounter3, 1, data);
			}
			readTmpBuffer((generalCounter3 + 13),1,data);//retrieve the length of the payload
			uint32_t pLen=(data[0] + 14);//calculate the total length of the packet including header
			readTmpBuffer(generalCounter3,pLen,data);
			generalCounter3 = (generalCounter3 + pLen);//add the total length, and substract one so we are pointing at the last byte of the previous packet
			sd.write((char*)data,pLen);//write the data to the file
			generalCounter2--;//remove the packet we just wrote from the queue
		}
	}
	setCANBadgerStatus(CAN1_LOGGING,0);//now we will disable logging so we can process the pending frames
	setCANBadgerStatus(CAN2_LOGGING,0);
	while(generalCounter2 > 0)//if we have pending data to retrieve from buffer after stopping the logger
	{
		if(generalCounter3 == 4096)//if we have reached the end of the buffer space
		{
			generalCounter3 = 0;//reset it to make it circular!
		}
		readTmpBuffer(generalCounter3, 1, data);
		while(data[0] == 0)//to skip bytes intentionally left blank
		{
			generalCounter3++;//we position the counter on the next byte
			if(generalCounter3 == 4096)//if we have reached the end of the buffer space
			{
				generalCounter3 = 0;//reset it to make it circular!
			}
			readTmpBuffer(generalCounter3, 1, data);
		}
		readTmpBuffer((generalCounter3 + 13),1,data);//retrieve the length of the payload
		uint32_t pLen=(data[0] + 14);//calculate the total length of the packet including header
		readTmpBuffer(generalCounter3,pLen,data);
		generalCounter3 = (generalCounter3 + pLen);//add the total length, and substract one so we are pointing at the last byte of the previous packet
		sd.write((char*)data,pLen);//write the data to the file
		generalCounter2--;//remove the packet we just wrote from the queue
	}
	if(wasCANBridgeEnabled == false)//if bridge was not enabled before logging, we disable it
	{
		CANBridge(0);
	}
	sd.closeFile();//close the log to save the file
	oled.clearScreen();
	oled.displayMessage("Parsing data...");
	oled.displayMessage("   Press back ",1);
	oled.displayMessage("    to stop   ",1);
	DiagSCAN scan(&can1);
	uint32_t IDs[256];
	memset(IDs,0,256);
	uint32_t result=scan.ScanActiveUDSIDsFromLog(IDs, filename, &sd, &buttons);
	oled.clearScreen();
    if(result ==0)//if there are no KWP2K IDs
    {
    	oled.displayMessage("No KWP2K found");
    }
    else if(result == 0xFFFFFFFF)//if there was no traffic
    {
    	oled.displayMessage("No traffic found");
    }
    else//create code to parse the list and create a file with it, then display the file on the screen
    {
    	oled.displayMessage("Creating log...");
    	memset(filename,0,90);
    	strcat(filename, "/Logging/KWP2K/KWP2KActiveScan_");
    	char fileExtension[6]=".txt";
    	if(!sd.getSequencialFileName(filename, fileExtension))
    	{
    		oled.displayMessage(" Filename Error ",1);
    		oled.displayMessage("****************",1);
    		oled.displayMessage("  If limit of",1);
    		oled.displayMessage(" 65535 files in",1);
    		oled.displayMessage(" logs folder is",1);
    		oled.displayMessage("  reached this",1);
    		oled.displayMessage("  will happen",1);
    		oled.displayMessage("****************",1);
    		buttons.getButtonPressed();
    		return;
    	}
    	if (!sd.openFile(filename, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
    	{
    		oled.displayMessage("SD Write error",1);
    		buttons.getButtonPressed();
    		return;
    	}
    	sd.write("**CANBadger Active KWP2K Scan Log**\n",34);
    	sd.write("**Format is Tester->ECU**\n\n",27);
    	for(uint32_t a=0; a < result; a++)
    	{
    		uint32_t id1=IDs[(2*a)];
    		uint32_t id2=IDs[((2*a)+1)];
    		char z[22];
    		uint8_t writeSize=0;//used to know the length of the ID
    		memset(z,0,22);
    		if(id2 == 0)//if it is a possible but we didnt see who it was talking to
    		{
    			sd.write("Possible KWP2K ID->0x",19);
    			if(id1 > 0x7FF)//if extended ID
    			{
    				convert.itox(id1,z,8);
    				writeSize=8;
    			}
    			else
    			{
    				convert.itox(id1,z,3);
    				writeSize=3;
    			}
    			sd.write(z,writeSize);
    			sd.write("\n",1);
    		}
    		else
    		{
    			sd.write("0x",2);
    			if(id1 > 0x7FF)//if extended ID
    			{
    				convert.itox(id1,z,8);
    				writeSize=8;
    			}
    			else
    			{
    				convert.itox(id1,z,3);
    				writeSize=3;
    			}
    			sd.write(z,writeSize);
    			sd.write("->",2);
    			memset(z,0,22);
    			sd.write("0x",2);
    			if(id2 > 0x7FF)//if extended ID
    			{
    				convert.itox(id2,z,8);
    				writeSize=8;
    			}
    			else
    			{
    				convert.itox(id2,z,3);
    				writeSize=3;
    			}
    			sd.write(z,writeSize);
    			sd.write("\n",1);
    		}
    	}
    	sd.closeFile();
    	if(!displayFile(filename))
    	{
    		oled.displayMessage("Log open error",1);
    		buttons.getButtonPressed();
    		return;
    	}
    	oled.clearScreen();
    	oled.displayMessage("Log saved in:");
    	oled.displayMessage(filename,1);
    }
	buttons.getButtonPressed();
}


void CANbadger::mainMenu()
{
	const char* options[15]={"Analysis", "Diagnostics", "Logging","Passthrough", "Configuration"};
	uint8_t option = 1;
	while(1)
	{
		oled.clearScreen();
		option = oled.showOLEDMenu("CANBADGER V2", options, 5, &buttons);
		if(option == 1)
		{
			analysisMenu();
		}
		else if(option == 2)
		{
			diagMenu();
		}
		else if(option == 3)
		{
			loggingMenu();
		}
		else if(option == 4)
		{
			passthroughMenu();
		}
		else if(option == 5)
		{
			ConfigMenu();
		}
	}
}

void CANbadger::passthroughMenu()
{
	const char* options[15]={"SocketCAN", "USB-KKL (KLINE)"};
	uint8_t option = 1;
	while(1)
	{
		oled.clearScreen();
		option = oled.showOLEDMenu("Passthrough Menu", options, 2, &buttons);
		if(option == 0)
		{
			return;
		}
		else if(option == 1)
		{
			socketCANMenu();
		}
		else if(option == 2)
		{
			oled.clearScreen();
			oled.displayMessage("   KKL Ready!   ");
			oled.displayMessage(" Use bottom USB",1);
			oled.displayMessage(" port and KLINE1",1);
			oled.displayMessage(" ",1);
			oled.displayMessage("  Make sure to ",1);
			oled.displayMessage("  install FTDI",1);
			oled.displayMessage("    drivers",1);
			oled.displayMessage(" ",1);
			oled.displayMessage("   Press back",1);
			oled.displayMessage("    to stop",1);
			KLINEHandler kline(&kline1,1);
			kline.ftdiPassthrough(&buttons);
		}
	}	
}



void CANbadger::test()//test routine
{

}



void CANbadger::socketCANMenu()
{
	const char* options[15]={"SocketCAN->CAN1", "SocketCAN->CAN2"};
	uint8_t option = 1;
	while(1)
	{
		oled.clearScreen();
		option = oled.showOLEDMenu("   SocketCAN    ", options, 2, &buttons);
		oled.clearScreen();
		if(option == 0)
		{
			return;
		}
		else if(option == 1)
		{
			oled.displayMessage(" SocketCAN Mode");
			oled.displayMessage("Interface: CAN1",1);
			oled.displayMessage("Press any button",1);
			oled.displayMessage("    to stop",1);
			SocketCAN socket(&can1,1,tmpBuffer);
			socket.start(&buttons);
		}
		if(option == 2)
		{
			oled.displayMessage(" SocketCAN Mode");
			oled.displayMessage("Interface: CAN2",1);
			oled.displayMessage("Press any button",1);
			oled.displayMessage("    to stop",1);
			SocketCAN socket(&can2,2,tmpBuffer);
			socket.start(&buttons);
		}
	}

}

void CANbadger::analysisMenu()
{
	const char* options[15]={"UDS CAN Recon", "KWP2K CAN Recon", "TP2.0 Recon", "RAW CAN Recon"};
	uint8_t option = 1;
	while(1)
	{
		oled.clearScreen();
		option = oled.showOLEDMenu("    Analysis    ", options, 4, &buttons);
		if(option == 0)
		{
			return;
		}
		else if(option == 1)
		{
			UDSCANReconMenu();
		}	
		else if(option == 2)
		{
			KWP2KCANReconMenu();
		}
		else if(option == 3)
		{
			TP20ReconMenu();
		}
		else if (option == 4)
		{
			CANReconMenu();
		}
	}	
}

void CANbadger::TP20ReconMenu()
{
	const char* options[15]={"Find TP2 Chans", "Scan TP2 Chan", "Security Hijack", "Security Hammer"};
	uint8_t option = 1;
	while(1)
	{
		oled.clearScreen();
		option = oled.showOLEDMenu("  TP2.0 Recon  ", options, 5, &buttons);
		if(option == 0)
		{
			return;
		}
		else if(option == 1)
		{
			findTP20Chans();
		}
		else if(option == 2)
		{
			scanTP20Chan();
		}
		else if(option == 3)
		{
			TP20SecurityHijackMenu();
		}
		else if(option == 5)
		{
			TP20SecurityHammerMenu();
		}
	}
}


void CANbadger::scanTP20Chan()
{
	oled.clearScreen();
	uint8_t tmpcID=0;
	getHexValue("Enter Chan LSB:",(uint64_t*)(&tmpcID),0,0xEF,1);
	uint16_t currentChan=0x200 + tmpcID;
	char filename2[90]="/Logging/TP20/Scans/";
	if(isSDInserted == true)
	{
		char tmpstrg[30];
		memset(tmpstrg,0,30);
		sprintf(tmpstrg,"%x_IDScan_",currentChan);
		strcat(filename2,tmpstrg);
		char fExt[6]=".TXT";
		if(!sd.getSequencialFileName(filename2, fExt))
		{
			oled.clearScreen();
			oled.displayMessage(" Filename Error ");
			oled.displayMessage("****************",1);
			oled.displayMessage("  If limit of",1);
			oled.displayMessage(" 65535 files in",1);
			oled.displayMessage("  folder is",1);
			oled.displayMessage("  reached this",1);
			oled.displayMessage("  will happen",1);
			oled.displayMessage("****************",1);
			buttons.getButtonPressed();
			oled.clearScreen();
		}
		else
		{
			if (!sd.openFile(filename2, O_WRONLY | O_CREAT | O_TRUNC))
			{
				oled.displayMessage("SD error...");
				buttons.getButtonPressed();
				oled.clearScreen();
			}
		}
	}
	oled.displayMessage("Scanning 0x");
	char z[32];
	convert.itox(currentChan, z, 3);
	oled.displayMessage(z,0,1);
	DiagSCAN scan(&can1);
	uint16_t currentID=0;
	bool isFirst=true;
	while(buttons.isButtonPressed(4) != 1)
	{
		if(scan.scanTP20ID(currentChan, currentID))
		{
			memset(z,0,32);
			convert.itox(currentID, z, 2);
			if(isFirst == true)
			{
				oled.displayMessage("0x",1);
				isFirst=false;
			}
			else
			{
				oled.displayMessage(",0x",0,1);
			}
			oled.displayMessage(z,0,1);
			memset(z,0,32);
			sprintf(z,"0x%02x is Active\n",currentID);
			sd.write(z,15);
		}
		currentID++;
		if(currentID == 0x100)
		{
			break;
		}
	}
	oled.displayMessage("Done",1);
	buttons.getButtonPressed();
	if(isSDInserted==true)
	{
		sd.closeFile();
		oled.clearScreen();
		oled.displayMessage("Log saved in:");
		oled.displayMessage(filename2,1);
		buttons.getButtonPressed();
	}
}



void CANbadger::findTP20Chans()
{
	oled.clearScreen();
	char filename2[90]="/Logging/TP20/Scans/ChannelScan_";
	if(isSDInserted == true)
	{
		char fExt[6]=".TXT";
		if(!sd.getSequencialFileName(filename2, fExt))
		{
			oled.clearScreen();
			oled.displayMessage(" Filename Error ");
			oled.displayMessage("****************",1);
			oled.displayMessage("  If limit of",1);
			oled.displayMessage(" 65535 files in",1);
			oled.displayMessage("  folder is",1);
			oled.displayMessage("  reached this",1);
			oled.displayMessage("  will happen",1);
			oled.displayMessage("****************",1);
			buttons.getButtonPressed();
			oled.clearScreen();
		}
		else
		{
			if (!sd.openFile(filename2, O_WRONLY | O_CREAT | O_TRUNC))
			{
				oled.displayMessage("SD error...");
				buttons.getButtonPressed();
				oled.clearScreen();
			}
		}
	}
	oled.displayMessage("Scanning...");
	DiagSCAN scan(&can1);
	uint16_t currentChan = 0x200;
	while(buttons.isButtonPressed(4) != 1)
	{
		if(scan.scanTP20Channel(currentChan))
		{
			char z[32];
			convert.itox(currentChan, z, 3);
			oled.displayMessage("0x",1);
			oled.displayMessage(z,0,1);
			oled.displayMessage(" is Active",0,1);
			memset(z,0,32);
			sprintf(z,"0x%03x is Active\n",currentChan);
			sd.write(z,16);
		}
		currentChan++;
		if(currentChan == 0x2F0)//end of non-broadcast channels
		{
			break;
		}
	}
	oled.displayMessage("Done",1);
	buttons.getButtonPressed();
	if(isSDInserted==true)
	{
		sd.closeFile();
		oled.clearScreen();
		oled.displayMessage("Log saved in:");
		oled.displayMessage(filename2,1);
		buttons.getButtonPressed();
	}
}





void CANbadger::CANReconMenu()
{
	const char* options[15]={"MITM"};
	uint8_t option = 1;
	while(1)
	{
		oled.clearScreen();
		option = oled.showOLEDMenu("    Analysis    ", options, 1, &buttons);
		if(option == 0)
		{
			return;
		}
		else if(option == 1)
		{
			CANMITMMenu();
		}
	}
}



void CANbadger::CANMITMMenu()
{
	oled.clearScreen();
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	CANFormat formato = CANStandard;
	char filename[64]={0};//where we will store the loaded rule
	while(1)
	{
		oled.clearScreen();
		oled.displayMessage("    CAN MITM    ");
		oled.displayMessage(" Rule: ",1);
		char z[22];
		if(filename[0] == 0)//if no rule file loaded
		{
			z[0]='N';
			z[1]='O';
			z[2]='N';
			z[3]='E';
			z[4]=0;
			oled.displayMessage(z,0,1);
		}
		else
		{
			char tmmp[10]={0};
			for(uint8_t u=0; u < 9; u++)
			{
				tmmp[u] = filename[u + 6];//to get just the filename without path
			}
			oled.displayMessage(tmmp,0,1);
		}
		oled.displayMessage(" ID Format: ",1);//show if standard or extended format is being used
		if(formato == CANStandard)
		{
			oled.displayMessage("STD",0,1);
		}
		else
		{
			oled.displayMessage("XTD",0,1);
		}
		oled.displayMessage(" Start MITM",1);
		oled.displayMessage(" Back",1);
		oled.set_text_rc(chosenOption,0);//we set the cursor on current highlight
		oled.putc(arrow);
		bool optChanged=false;
		while(!optChanged)
		{
			uint8_t a = buttons.getButtonPressed();//check which button was pressed
			if(a == 1)
			{
				if(chosenOption == 1)
				{
					char tmpFileName[64]="/MITM";
					if(getFileName(tmpFileName))
					{
						memset(filename,0,64);//clean the previous one
						strcat(filename,"/MITM/");
						strcat(filename, tmpFileName);//place the new one
					}
					optChanged = true;
				}
				else if(chosenOption == 2)
				{
					if(formato == CANStandard)
					{
						formato = CANExtended;
					}
					else
					{
						formato = CANStandard;
					}
					optChanged = true;
				}
				else if(chosenOption ==3)
				{
					if(filename[6] != 0)//if a rule was loaded
					{
						MITMMode(filename);
					}
					else
					{
						oled.clearScreen();
						oled.displayMessage("Choose rule!");
						wait(2);
					}
					optChanged = true;
				}
				else
				{
					return;
				}

			}
			else if (a == 4)//return button pressed
			{
				return;
			}
			else if (a == 2)//One option down, the "+" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 4)//in order to not overwrite the header
				{
					b = 1;
				}
				else
				{
					b++;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 3)//One option up, the "-" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 1)//in order to not overwrite the header
				{
					b = 4;
				}
				else
				{
					b--;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
		}
	}

}


void CANbadger::diagMenu()
{
	const char* options[15]={"UDS on CAN1", "UDS on CAN2", "TP2.0 on CAN1", "TP2.0 on CAN2"};
	uint8_t option = 1;
	while(1)
	{
		oled.clearScreen();
		option = oled.showOLEDMenu("  Diag Menu  ", options, 4, &buttons);
		if(option == 0)
		{
			return;
		}
		else if(option == 1)
		{
			oled.clearScreen();
			doOLEDUDS(&can1,1);
		}
		else if(option == 2)
		{
			oled.clearScreen();
			doOLEDUDS(&can2,2);
		}
		else if(option == 3)
		{
			doOLEDTP20(&can1,1);
		}
		else if(option == 4)
		{
			doOLEDTP20(&can2,2);
		}		
	}	
}

void CANbadger::loggingMenu()
{
	const char* options[15]={"Start logging"};
	uint8_t option = 1;
	while(1)
	{
		oled.clearScreen();
		option = oled.showOLEDMenu((const char*)"  Logging Menu  ", options, 1, &buttons);
		if(option == 0)
		{
			return;
		}
		else if(option == 1)
		{
			oled.clearScreen();
			startLog();
		}
	}	
}

void CANbadger::ConfigMenu()
{
	const char* options[15]={"CAN1", "CAN2", "CAN Bridge", "Format SD", "Reboot"};
	uint8_t option = 1;
	while(option != 6)
	{
		oled.clearScreen();
		option = oled.showOLEDMenu((const char*)"CANBadger Config", options, 5, &buttons);
		if(option == 0)
		{
			return;
		}
		else if(option == 1)
		{
			CANConfigMenu(1);
		}
		else if(option == 2)
		{
			CANConfigMenu(2);
		}
		else if(option == 3)
		{
			CANBridgeMenu();
		}
		else if(option == 4)
		{
			oled.clearScreen();
			oled.displayMessage("  Max size 32GB");
			oled.displayMessage("  This can take ",1);
			oled.displayMessage("   really long",1);
			oled.displayMessage("  (1H for 32GB)",1);
			oled.displayMessage(" ",1);
			oled.displayMessage("   Continue?",1);
			uint8_t a = buttons.getButtonPressed();//check which button was pressed
			if(a == 1)
			{
				oled.clearScreen();
				oled.displayMessage("Formatting...");
				timer.stop();
				timer.reset();
				timer.start();
				if(!sd.formatSD())
				{
					oled.displayMessage("Error!",1);
				}
				else
				{
					float timeBefore= timer.read();
					char z[32];
					convert.itoa(timeBefore,z,8);
					oled.displayMessage("Done in:",1);
					oled.displayMessage(z,1);
					oled.displayMessage(" seconds",0,1);
					oled.displayMessage(" ",1);
				}
				timer.stop();
				timer.reset();
				oled.displayMessage(" Press any key",1);
				a = buttons.getButtonPressed();//just to wait
			}
			
		}
		else if (option == 5)
		{
			NVIC_SystemReset();
		}
	}
		
}

void CANbadger::CANBridgeMenu()
{
	oled.clearScreen();
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	while(1)
	{
		oled.clearScreen();
		oled.displayMessage("CAN Bridge: ");
		char z[22];
		if(getCANBadgerStatus(CAN_BRIDGE_ENABLED) == true)
		{
			z[0]='O';
			z[1]='N';
			z[2]=0;
		}
		else
		{
			z[0]='O';
			z[1]='F';
			z[2]='F';
			z[3]=0;
		}
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Enable",1);
		oled.displayMessage(" Disable",1);
		oled.set_text_rc(chosenOption,0);//we set the cursor on current highlight
		oled.putc(arrow);
		bool optChanged=false;
		while(!optChanged)
		{
			uint8_t a = buttons.getButtonPressed();//check which button was pressed
			if(a == 1)
			{
				if(chosenOption == 1)
				{
					if(getCANBadgerStatus(CAN_BRIDGE_ENABLED) == false)
					{
						CANBridge(1);
					}
					optChanged=true;
				}
				else if(chosenOption == 2)
				{
					if(getCANBadgerStatus(CAN_BRIDGE_ENABLED) == true)
					{
						CANBridge(0);
					}
					optChanged=true;
				}
			}
			else if (a == 4)//return button pressed
			{
				return;
			}
			else if (a == 2)//One option down, the "+" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 2)//in order to not overwrite the header
				{
					b = 1;
				}
				else
				{
					b++;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 3)//One option up, the "-" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 1)//in order to not overwrite the header
				{
					b = 2;
				}
				else
				{
					b--;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}	
		}
	}
	
}

void CANbadger::CANConfigMenu(uint8_t busno)
{
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	while(1)
	{
		oled.clearScreen();
		if(busno==1)
		{
			oled.displayMessage("  CAN1 Options  ");//show the header
		}
		else
		{
			oled.displayMessage("  CAN2 Options  ");//show the header
		}
		oled.displayMessage(" Speed: ",1);
		char z[22];
		if(busno == 1)
		{
			convert.itoa(canbadger_settings->getSpeed(1),z,16);
		}
		else
		{
			convert.itoa(canbadger_settings->getSpeed(2),z,16);
		}		
		oled.displayMessage(z,0,1);

		oled.displayMessage(" Format: ",1);//show if standard or extended format is being used
		if(busno == 1)
		{
			if(getCANBadgerStatus(CAN1_STANDARD) == true)
			{
				oled.displayMessage("STD",0,1);
			}
			else
			{
				oled.displayMessage("XTD",0,1);
			}
		}
		else
		{
			if(getCANBadgerStatus(CAN2_STANDARD) == true)
			{
				oled.displayMessage("STD",0,1);
			}
			else
			{
				oled.displayMessage("XTD",0,1);
			}
		}		

		oled.displayMessage(" Padding: ",1);
		if(busno == 1)
		{
			if(getCANBadgerStatus(CAN1_USE_FULLFRAME) == true)
			{
				convert.itox(CAN1PaddingByte,z,2);
			}
			else
			{
				z[0]='O';
				z[1]='F';
				z[2]='F';
				z[3]=0;
			}
		}
		else
		{
			if(getCANBadgerStatus(CAN2_USE_FULLFRAME) == true)
			{
				convert.itox(CAN2PaddingByte,z,2);
			}
			else
			{
				z[0]='O';
				z[1]='F';
				z[2]='F';
				z[3]=0;
			}
		}			
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Bridge: ",1);
		if(busno == 1)
		{
			if(getCANBadgerStatus(CAN1_TO_CAN2_BRIDGE) == true)
			{
				z[0]='O';
				z[1]='N';
				z[2]=0;
			}
			else
			{
				z[0]='O';
				z[1]='F';
				z[2]='F';
				z[3]=0;
			}
		}
		else
		{
			if(getCANBadgerStatus(CAN2_TO_CAN1_BRIDGE) == true)
			{
				z[0]='O';
				z[1]='N';
				z[2]=0;
			}
			else
			{
				z[0]='O';
				z[1]='F';
				z[2]='F';
				z[3]=0;
			}
		}			
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Logging: ",1);
		if(busno == 1)
		{
			if(getCANBadgerStatus(CAN1_LOGGING) == true)
			{
				z[0]='O';
				z[1]='N';
				z[2]=0;
			}
			else
			{
				z[0]='O';
				z[1]='F';
				z[2]='F';
				z[3]=0;
			}
		}
		else
		{
			if(getCANBadgerStatus(CAN2_LOGGING) == true)
			{
				z[0]='O';
				z[1]='N';
				z[2]=0;
			}
			else
			{
				z[0]='O';
				z[1]='F';
				z[2]='F';
				z[3]=0;
			}
		}					
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Monitor: ",1);
		if(busno == 1)
		{
			if(getCANBadgerStatus(CAN1_MONITOR) == true)
			{
				z[0]='O';
				z[1]='N';
				z[2]=0;
			}
			else
			{
				z[0]='O';
				z[1]='F';
				z[2]='F';
				z[3]=0;
			}
		}
		else
		{
			if(getCANBadgerStatus(CAN2_MONITOR) == true)
			{
				z[0]='O';
				z[1]='N';
				z[2]=0;
			}
			else
			{
				z[0]='O';
				z[1]='F';
				z[2]='F';
				z[3]=0;
			}
		}
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Detect speed",1);
		oled.set_text_rc(chosenOption,0);//we set the cursor on current highlight
		oled.putc(arrow);
		bool optChanged=false;
		while(!optChanged)
		{
			uint8_t a = buttons.getButtonPressed();//check which button was pressed
			if(a == 1)
			{
				if(chosenOption == 1)
				{
					if(busno == 1)
					{
						// OPTIMIZATION HERE
						uint32_t read_speed;
						if(getDecValue("Enter Speed:",(uint64_t*)(&read_speed),5000,1000000,5000))
						{
							if(!setCANSpeed(1,read_speed))
							{
								setCANSpeed(1,500000);
							}
						}
					}
					else
					{
						// OPTIMIZATION HERE
						uint32_t read_speed;
						if(getDecValue("Enter Speed:",(uint64_t*)(&read_speed),5000,1000000,5000))
						{
							if(!setCANSpeed(2,read_speed))
							{
								setCANSpeed(2,500000);
							}
						}
					}						
					optChanged=true;
				}
				else if(chosenOption == 2)
				{
					if(busno == 1)
					{
						if(getCANBadgerStatus(CAN1_STANDARD) == true)
						{
							setCANBadgerStatus(CAN1_STANDARD, 0);
							setCANBadgerStatus(CAN1_EXTENDED, 1);
						}
						else
						{
							setCANBadgerStatus(CAN1_STANDARD, 1);
							setCANBadgerStatus(CAN1_EXTENDED, 0);
						}
					}	
					else
					{
						if(getCANBadgerStatus(CAN2_STANDARD) == true)
						{
							setCANBadgerStatus(CAN2_STANDARD, 0);
							setCANBadgerStatus(CAN2_EXTENDED, 1);
						}
						else
						{
							setCANBadgerStatus(CAN2_STANDARD, 1);
							setCANBadgerStatus(CAN2_EXTENDED, 0);
						}
					}
					optChanged=true;
				}
				else if(chosenOption == 3)
				{
					if(busno == 1)
					{
						if(getCANBadgerStatus(CAN1_USE_FULLFRAME) == true)
						{
							setCANBadgerStatus(CAN1_USE_FULLFRAME, 0);
						}
						else
						{
							if(getHexValue("Enter padding:",(uint64_t*)(&CAN1PaddingByte),0,0xFF,1))
							{
								setCANBadgerStatus(CAN1_USE_FULLFRAME, 1);
							}
						}
					}
					else
					{
						if(getCANBadgerStatus(CAN2_USE_FULLFRAME) == true)
						{
							setCANBadgerStatus(CAN2_USE_FULLFRAME, 0);
						}			
						else
						{
							if(getHexValue("Enter padding:",(uint64_t*)(&CAN2PaddingByte),0,0xFF,1))
							{
								setCANBadgerStatus(CAN2_USE_FULLFRAME, 1);
							}
						}
					}
					optChanged=true;
				}
				else if(chosenOption == 4)
				{
					if(busno == 1)
					{
						if(getCANBadgerStatus(CAN1_TO_CAN2_BRIDGE) == true)
						{
							setCANBadgerStatus(CAN1_TO_CAN2_BRIDGE,0);
						}
						else
						{
							setCANBadgerStatus(CAN1_TO_CAN2_BRIDGE,1);
						}
					}
					else
					{
						if(getCANBadgerStatus(CAN2_TO_CAN1_BRIDGE) == true)
						{
							setCANBadgerStatus(CAN2_TO_CAN1_BRIDGE,0);
						}
						else
						{
							setCANBadgerStatus(CAN2_TO_CAN1_BRIDGE,1);
						}						
					}
					optChanged=true;
				}

				else if(chosenOption == 5)
				{
					if(busno == 1)
					{
						if(getCANBadgerStatus(CAN1_LOGGING) == true)
						{
							setCANBadgerStatus(CAN1_LOGGING,0);
						}
						else
						{
							setCANBadgerStatus(CAN1_LOGGING,1);
						}
					}
					else
					{
						if(getCANBadgerStatus(CAN2_LOGGING) == true)
						{
							setCANBadgerStatus(CAN2_LOGGING,0);
						}
						else
						{
							setCANBadgerStatus(CAN2_LOGGING,1);
						}						
					}
					optChanged=true;
				}
				else if(chosenOption == 6)
				{
					if(busno == 1)
					{
						if(getCANBadgerStatus(CAN1_MONITOR) == true)
						{
							CANbadger_CAN can(&can1);
							can.CANMonitorMode(0);
							setCANBadgerStatus(CAN1_MONITOR,0);
						}
						else
						{
							CANbadger_CAN can(&can1);
							can.CANMonitorMode(1);
							setCANBadgerStatus(CAN1_MONITOR,1);
						}	
					}
					else
					{
						if(getCANBadgerStatus(CAN2_MONITOR) == true)
						{
							CANbadger_CAN can(&can2);
							can.CANMonitorMode(0);
							setCANBadgerStatus(CAN2_MONITOR,0);
						}
						else
						{
							CANbadger_CAN can(&can2);
							can.CANMonitorMode(1);
							setCANBadgerStatus(CAN2_MONITOR,1);
						}	
					}						
					optChanged=true;
				}							
				else if(chosenOption == 7)
				{
					oled.clearScreen();
					oled.displayMessage("Please wait..");
					uint32_t r;
					if(busno == 1)
					{
						CANbadger_CAN canbus(&can1);
						r=canbus.detectSpeed();
						if(r == 0)
						{
							canbadger_settings->setSpeed(1,500000);
						}
						else
						{
							canbadger_settings->setSpeed(1,r);
						}
					}
					else
					{
						CANbadger_CAN canbus(&can2);
						r=canbus.detectSpeed();
						if(r == 0)
						{
							canbadger_settings->setSpeed(2,500000);
						}
						else
						{
							canbadger_settings->setSpeed(2,r);
						}
					}
					if(r == 0)//if we couldnt detect the speed...
					{
						oled.displayMessage("No traffic found",1);
					}
					else
					{
						oled.displayMessage("Speed was set",1);
					}
					buttons.getButtonPressed();
					optChanged=true;
				}
			}
			else if (a == 4)//return button pressed
			{
				return;
			}
			else if (a == 2)//One option down, the "+" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 7)//in order to not overwrite the header
				{
					b = 1;
				}
				else
				{
					b++;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 3)//One option up, the "-" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 1)//in order to not overwrite the header
				{
					b = 7;
				}
				else
				{
					b--;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}	
		}
	}
}	


bool CANbadger::getDecValue(const char *Message, uint64_t *currentValue, uint64_t minValue, uint64_t maxValue, uint32_t step)
{
	oled.clearScreen();
	oled.displayMessage(Message);
	uint64_t newValue;
	if(maxValue <=0xFF)
	{
		newValue=(uint8_t&)(*currentValue);
	}
	else if(maxValue > 0xFF && maxValue <= 0xFFFF)
	{
		newValue=(uint16_t&)(*currentValue);
	}
	else if(maxValue > 0xFFFF && maxValue <= 0xFFFFFFFF)
	{
		newValue=(uint32_t&)(*currentValue);
	}
	else if(maxValue > 0xFFFFFFFF)
	{
		newValue=(uint64_t&)(*currentValue);
	}
	uint32_t timesHeld=0;
	bool fast = false;
	uint8_t toSkip = 0;
	char z[32]={0};
	timer.stop();
	timer.reset();
	timer.start();
	uint8_t extra=1;
	float waitTime=0;//how long to wait due to long key press
	if(newValue == 0)
	{
		convert.itox(newValue,z,1);
	}
	else
	{
		convert.itoa(newValue,z,16);
	}
	oled.set_text_rc(1,0);
	oled.displayMessage(z,0,1);
	while(1)
	{
		float timeBefore= timer.read();
		uint8_t a = buttons.getButtonPressed(0.01);//check which button was pressed
		float timeAfter = timer.read();
		if((timeAfter - timeBefore) >= 0.3)
		{
			timesHeld=1;
		}
		else if((timeAfter - timeBefore) < 0.3 && timesHeld < 201)
		{
			timesHeld++;
		}
		if(a == 1)//if the OK button is pressed
		{
			if(maxValue <=0xFF)//make sure we return the corrent int type not to overflow
			{
				(uint8_t&)(*currentValue)=newValue;
			}
			else if(maxValue > 0xFF && maxValue <= 0xFFFF)
			{
				(uint16_t&)(*currentValue)=newValue;
			}
			else if(maxValue > 0xFFFF && maxValue <= 0xFFFFFFFF)
			{
				(uint32_t&)(*currentValue)=newValue;
			}	
			else if(maxValue > 0xFFFFFFFF)
			{
				(uint64_t&)(*currentValue)=newValue;
			}				
			timer.stop();
			return true;//return the value
		}
		else if (a == 4)//return button pressed
		{
			timer.stop();
			timer.reset();
			return false;//thats how we know that the user cancelled
		}
		else if (a == 2)//One option down, the "+" button
		{
				if (timesHeld >= 15 && timesHeld < 100)
				{
					waitTime=0.1;
					newValue = (newValue + (step * 2));
				}
				else if (timesHeld >= 100 && timesHeld < 200)
				{
					waitTime=0;
					newValue = (newValue + (step * 4));			
				}		
				else if(timesHeld >= 200)
				{
					waitTime=0;
					newValue = (newValue + (step * 10));			
				}
				else
				{
					waitTime=0.15;
					newValue = (newValue + step);
				}			
				if(newValue > maxValue)
				{
					newValue = minValue;
				}			
				if((fast == false) || toSkip == 10)
				{
					oled.set_text_rc(1,0);
					for (uint8_t r=0; r< 16; r++)
					{
						oled.putc(' ');
					}				
					convert.itoa(newValue,z,16);
					oled.set_text_rc(1,0);
					oled.displayMessage(z,0,1);
					if(toSkip == 10)
					{
						toSkip = 0;
					}
				}
				wait(waitTime);
		}
		else if (a == 3)//One option up, the "-" button
		{
				if (timesHeld >= 15 && timesHeld < 100)
				{
					waitTime=0.1;
					extra=2;
				}
				else if (timesHeld >= 100 && timesHeld < 200)
				{
					waitTime=0;
					extra=4;
				}			
				else if (timesHeld >= 200)
				{
					waitTime=0;
					extra=10;
				}						
				else
				{
					waitTime=0.15;
					extra=1;
				}												
				if(((newValue - (step * extra)) >= minValue) && (newValue >= (step * extra)))
				{
					newValue = (newValue - (step * extra));
				}		
				else
				{
					newValue = maxValue;
				}
				if(newValue == 0)
				{
					convert.itox(newValue,z,1);
				}
				else
				{
					convert.itoa(newValue,z,16);
				}
				oled.set_text_rc(1,0);
				for (uint8_t r=0; r< 16; r++)
				{
					oled.putc(' ');
				}				
				convert.itoa(newValue,z,16);
				oled.set_text_rc(1,0);
				oled.displayMessage(z,0,1);
				wait(waitTime);
			}
	}		
}


bool CANbadger::getHexValue(const char *Message, uint64_t *currentValue, uint64_t minValue, uint64_t maxValue, uint32_t step)
{
	oled.clearScreen();
	oled.displayMessage(Message);
	uint64_t newValue;
	if(maxValue <=0xFF)
	{
		newValue=(uint8_t&)(*currentValue);
	}
	else if(maxValue > 0xFF && maxValue <= 0xFFFF)
	{
		newValue=(uint16_t&)(*currentValue);
	}
	else if(maxValue > 0xFFFF && maxValue <= 0xFFFFFFFF)
	{
		newValue=(uint32_t&)(*currentValue);
	}
	else if(maxValue > 0xFFFFFFFF)
	{
		newValue=(uint64_t&)(*currentValue);
	}
	uint32_t timesHeld=0;
	char z[32]={0};
	float waitTime=0;//how long to wait due to long key press
	timer.stop();
	timer.reset();
	timer.start();
	uint8_t howLong=0;
	uint8_t extra=1;
	howLong=convert.getHexNumberLength(newValue);
	convert.itox(newValue,z,howLong);
	oled.set_text_rc(1,0);
	oled.displayMessage(z,0,1);
	while(1)
	{
		float timeBefore= timer.read();
		uint8_t a = buttons.getButtonPressed(0);//check which button was pressed
		float timeAfter = timer.read();
		if((timeAfter - timeBefore) >= 0.3)
		{
			timesHeld=1;
		}
		else if((timeAfter - timeBefore) < 0.3 && timesHeld < 201)
		{
			timesHeld++;
		}
		if(a == 1)//if the OK button is pressed
		{
			if(maxValue <=0xFF)
			{
				(uint8_t&)(*currentValue)=newValue;
			}
			else if(maxValue > 0xFF && maxValue <= 0xFFFF)
			{
				(uint16_t&)(*currentValue)=newValue;
			}
			else if(maxValue > 0xFFFF && maxValue <= 0xFFFFFFFF)
			{
				(uint32_t&)(*currentValue)=newValue;
			}	
			else if(maxValue > 0xFFFFFFFF)
			{
				(uint64_t&)(*currentValue)=newValue;
			}				
			timer.stop();
			oled.clearScreen();
			return true;//return the value
		}
		else if (a == 4)//return button pressed
		{
			timer.stop();
			oled.clearScreen();
			return false;//thats how we know that the user cancelled
		}
		else if (a == 2)//Numbers up, the "+" button
		{
			if (timesHeld >= 15 && timesHeld < 100)
			{
				waitTime=0.1;
				newValue = (newValue + (step * 2));
			}
			else if (timesHeld >= 100 && timesHeld < 200)
			{
				waitTime=0;
				newValue = (newValue + (step * 4));
			}
			else if(timesHeld >= 200)
			{
				waitTime=0;
				newValue = (newValue + (step * 10));
			}
			else
			{
				waitTime=0.15;
				newValue = (newValue + step);
			}
			if(newValue > maxValue)
			{
				newValue = minValue;
			}
			oled.set_text_rc(1,0);
			for (uint8_t r=0; r< 16; r++)
			{
				oled.putc(' ');
			}
			howLong=convert.getHexNumberLength(newValue);
			convert.itox(newValue,z,howLong);
			oled.set_text_rc(1,0);
			oled.displayMessage(z,0,1);
			wait(waitTime);
		}
		else if (a == 3)//Numbers down, the "-" button
		{
				if (timesHeld >= 15 && timesHeld < 100)
				{
					waitTime=0.1;
					extra=2;
				}
				else if (timesHeld >= 100 && timesHeld < 200)
				{
					waitTime=0;
					extra=4;
				}			
				else if (timesHeld >= 200)
				{
					waitTime=0;
					extra=10;
				}						
				else
				{
					waitTime=0.15;
					extra=1;
				}												
				if(((newValue - (step * extra)) >= minValue) && (newValue >= (step * extra)))
				{
					newValue = (newValue - (step * extra));
				}		
				else
				{
					newValue = maxValue;
				}
				oled.set_text_rc(1,0);
				for (uint8_t r=0; r< 16; r++)
				{
					oled.putc(' ');
				}						
				howLong=convert.getHexNumberLength(newValue);
				convert.itox(newValue,z,howLong);
				oled.set_text_rc(1,0);
				oled.displayMessage(z,0,1);
				wait(waitTime);
			}	
	}		
}


bool CANbadger::displayFile(const char *filename)
{
	oled.clearScreen();
	if(isSDInserted == false)//if there is no SD
	{
		oled.displayMessage("SD not Inserted");
		buttons.getButtonPressed();
		return false;
	}
	uint32_t fileSize=getFileSize(filename);
	if(fileSize == 0)
	{
		oled.displayMessage("File is empty");
		buttons.getButtonPressed();
		return false;
	}
	if(!sd.openFile(filename, O_RDONLY))//if we cant open the file
	{
		oled.displayMessage("File error");
		buttons.getButtonPressed();
		return false;
	}
	uint32_t fileOffset=0;
	uint32_t a=0;
	bool isChanged=true;//to know if we need to refresh the screen
	while(1)
	{
		if(isChanged == true)
		{
			sd.lseekFile(fileOffset,SEEK_SET);//move the pointer to the current position
			oled.clearScreen();
			memset(tmpBuffer,0,300);
			if(fileSize < 256)
			{
				a=sd.read((char*)tmpBuffer,fileSize);
			}
			else
			{
				a=sd.read((char*)tmpBuffer,256);
			}
//			bool isEndOfFile = false;
//			if(a < 256)
//			{
//				isEndOfFile=true;
//			}
			oled.displayMessage((const char*)tmpBuffer);
		}
		isChanged=false;
		uint8_t b=buttons.getButtonPressed();
		switch(b)
		{
			case 1://Start Button
			{
				//do nothing
				break;
			}
			case 2://Down button
			{
				if(fileSize <= 256)//if the file is smaller than the screen
				{
					break;
				}
				else if(a < 256 && fileOffset != 0)//if we have reached the end of the file
				{
					fileOffset = 0;//scroll back to the beginning
					isChanged = true;
				}
				else//if we are not in the end of the file
				{
					fileOffset = (fileOffset + 16);
					isChanged = true;
				}
				break;
			}
			case 3://up button
			{
				if(fileSize <= 256)//if the file is smaller than the screen
				{
					break;
				}
				else if(a < 256 && fileOffset != 0)//if we have reached the end of the file
				{
					fileOffset = (fileSize - 256);//scroll to show the end of the file
					isChanged = true;
				}
				else//if we are not in the end of the file
				{
					fileOffset = (fileOffset - 16);
					isChanged = true;
				}
				break;
			}
			case 4://back button
			{
				oled.clearScreen();
				return true;
			}
		}
	}
}


bool CANbadger::sendCANFrame(uint32_t msgID, uint8_t *payload, uint8_t len, uint8_t bus, CANFormat frameFormat, CANType frameType, uint32_t timeout)
{
	uint32_t currentMS=0;
	if(timeout != 0)
	{
		if(bus == 1)
		{
			while(((LPC_CAN1->GSR & 4) == 0) && currentMS < (timeout * 10))//hack to fix a silicon bug.
			{
				wait(0.0001);
				currentMS++;
			}
			while (!can1.write(CANMessage(msgID, reinterpret_cast<char*>(payload), len, frameType, frameFormat)) && currentMS < (timeout * 10))
			{
				wait(0.0001);
				currentMS++;
			}//make sure the msg goes out
		}
		else
		{
			while(((LPC_CAN2->GSR & 4) == 0) && currentMS < (timeout * 10))//hack to fix a silicon bug.
			{
				wait(0.0001);
				currentMS++;
			}
			while (!can2.write(CANMessage(msgID, reinterpret_cast<char*>(payload), len, frameType, frameFormat)) && currentMS < (timeout * 10))
			{
				wait(0.0001);
				currentMS++;
			}//make sure the msg goes out
		}
	}
	else
	{
		if(bus == 1)
		{
			while(((LPC_CAN1->GSR & 4) == 0) && currentMS < (timeout * 10))//hack to fix a silicon bug.
			{
				wait(0.0001);
				currentMS++;
			}
			can1.write(CANMessage(msgID, reinterpret_cast<char*>(payload), len, frameType, frameFormat));
		}
		else
		{
			while(((LPC_CAN2->GSR & 4) == 0) && currentMS < (timeout * 10))//hack to fix a silicon bug.
			{
				wait(0.0001);
				currentMS++;
			}
			can2.write(CANMessage(msgID, reinterpret_cast<char*>(payload), len, frameType, frameFormat));
		}
	}
	if(currentMS >= (timeout * 10) && timeout != 0)
	{
		return false;
	}
	return true;
}

bool CANbadger::listAllFiles(char *lookupDir, vector<std::string>* filenames) {
	return sd.listAllFiles(lookupDir, filenames);
}

bool CANbadger::doesDirExist(const char *dirName) {
	return sd.doesDirExist(dirName);
}

bool CANbadger::getFileName(char *lookupDir)
{
	oled.clearScreen();
	if(isSDInserted == false)
	{
		oled.displayMessage("SD not Inserted");
		buttons.getButtonPressed();
		return false;
	}
	vector<std::string> filenames; //filenames are stored in a vector string
	sd.listAllFiles(lookupDir, &filenames);
	std::string s;
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	bool scrollScreen=true;//we use this to refresh the shown options on the screen
	uint16_t howManyFiles=(uint16_t)filenames.size();
	if(howManyFiles == 0)
	{
		oled.displayMessage("Folder is Empty");
		buttons.getButtonPressed();
		return false;
	}
	double pages = ((double)howManyFiles / 15.0) + 1.0;//check how many pages do we have with file names
	if (floor(pages) == pages)//if its a round number
	{
		pages = (pages - 1);//we need to substract the +1 we added
	}
	uint16_t numOfPages = (int)pages;//now get the round number
	uint16_t currentPage = 1;
	while(1)
	{
		if(scrollScreen == true)
		{
			oled.clearScreen();
			oled.displayMessage(" Choose a file:");
			uint16_t r = 1;//used to check that we are in the correct page;
			uint8_t pageCounter = 0;//to count items per page
			for(vector<std::string>::iterator it=filenames.begin(); it != filenames.end(); it++)  
			{
				if(r == currentPage && it != filenames.end())
				{
					const char *tmpPointer = (*(it)).c_str();
					char tmpMsg[16]={0};
					for(uint8_t e = 0; e < 15; e++)//we do this to limit the displayed len to 15 so we dont overflow
					{
						tmpMsg[e] = tmpPointer[e];
					}
					oled.displayMessage(tmpMsg,1,0,1);
				}
				pageCounter++;
				if(pageCounter == 15)
				{
					pageCounter = 0;
					r++;
				}
			}	
			oled.set_text_rc(chosenOption,0);//we set the cursor on current highlight
			oled.putc(arrow);		
			scrollScreen = false;
		}
		uint8_t a = buttons.getButtonPressed(0.15);//check which button was pressed
		if(a == 1)
		{
			numOfPages=(15 * (currentPage - 1) + oled.getCurrentRow()) - 1;//we just reuse this var to get the current file name
			strcpy(lookupDir, (*(filenames.begin() + numOfPages)).c_str());
			oled.clearScreen();
			return true;//full path written
		}
		else if (a == 4)//return button pressed
		{
			oled.clearScreen();
			return false;
		}
		else if (a == 2)//One option down, the "+" button
		{
			uint16_t b = oled.getCurrentRow();
			oled.set_text_rc(b,0);
			oled.putc(space);
			if(b == howManyFiles && howManyFiles < 15)//in order to not overwrite the header. This is the case where we have less than 15 files, so they fit in the screen
			{
				b = 1;
				oled.set_text_rc(b,0);
				oled.putc(arrow);
			}
			else if(b == 15 && currentPage < numOfPages)//if we have hit the end and there is more than one screen
			{
				currentPage++;
				if(currentPage > numOfPages)
				{
					currentPage = 1;
				}
				scrollScreen = true;//and refresh the entire screen
				chosenOption = 1;//because we are scrolling down, we need to set the cursor on the first option
			}
			else if(b + ((currentPage - 1) * 15) == howManyFiles)//if this is the last file
			{
				currentPage = 1;
				scrollScreen = true;//and refresh the entire screen
				chosenOption = 1;
			}
			else
			{
				oled.set_text_rc((b + 1),0);
				oled.putc(arrow);
			}
			
		}
		else if (a == 3)//One option up, the "-" button
		{
			uint16_t b = oled.getCurrentRow();
			oled.set_text_rc(b,0);
			oled.putc(space);
			if(b == 1 && howManyFiles <= 15)//in order to not overwrite the header. This is the case where we have less than 15 files, so they fit in the screen
			{
				b = howManyFiles;
				oled.set_text_rc(b,0);
				oled.putc(arrow);
			}
			else if(b == 1 && howManyFiles > 15)//if we have hit the top and there is more than one screen
			{
				if(currentPage == 1)//if we are on the first page
				{
					currentPage = numOfPages;//set the page pointer to the last page
					chosenOption = howManyFiles % 15;//get the remainder to set the pointer there
					//need to figure out how to set option pointer to last dynamically
					
				}
				else
				{
					currentPage--;
					chosenOption = 15;
				}
				scrollScreen = true;//and refresh the entire screen
			}
			else
			{
				oled.set_text_rc((b - 1),0);
				oled.putc(arrow);
			}
		}			
	}
}


void CANbadger::setCANPaddingByte(uint8_t interfaceNo, uint8_t pByte)
{
	if(interfaceNo == 1)
	{
		CAN1PaddingByte=pByte;
	}
	else
	{
		CAN2PaddingByte=pByte;
	}
	
}


bool CANbadger::startLog()//uint8_t interfaces)
{
	oled.clearScreen();
	if(isSDInserted == 0)
	{
		oled.displayMessage("SD Not detected");
		buttons.getButtonPressed();
		return false;//we cant log without an SD!
	}
	if(!getCANBadgerStatus(CAN1_LOGGING) && !getCANBadgerStatus(CAN2_LOGGING) && !getCANBadgerStatus(KLINE1_LOGGING) && !getCANBadgerStatus(KLINE2_LOGGING))//make sure that some interface has been enabled for logging
	{
		oled.displayMessage("Log not enabled");
		buttons.getButtonPressed();
		return false;//we cant log without an interface		
	}
	char filename[64]="/Logging/RAW/RAW_";
	char eXT[3]={0,0,0};
	if(!sd.getSequencialFileName(filename, eXT))
	{
		oled.clearScreen();
		oled.displayMessage(" Filename Error ");
		oled.displayMessage("****************",1);
		oled.displayMessage("  If limit of",1);
		oled.displayMessage(" 65535 files in",1);
		oled.displayMessage("  folder is",1);
		oled.displayMessage("  reached this",1);
		oled.displayMessage("  will happen",1);
		oled.displayMessage("****************",1);
		buttons.getButtonPressed();
		oled.clearScreen();
		return false;
	}
	if (!sd.openFile(filename, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
	{
		oled.displayMessage("SD Write error");
		buttons.getButtonPressed();
		return false;
	}
	bool wasCANBridgeEnabled=false;
	bool wasKLINEBridgeEnabled=false;
	//uint32_t frmCount=0;//to keep track of processed frames
	generalCounter1=0;//we initialize the buffer pointer for the interrupts and the pending packets
	generalCounter2=0;
	generalCounter3=0;//used as our buffer Pointer
	uint8_t data[269];//we will store the retrieved data from buffer here. 269 is the maximum length we will ever get
	timer.reset();//reset the timer
	if(!getCANBadgerStatus(CAN_BRIDGE_ENABLED) && (getCANBadgerStatus(CAN1_LOGGING) || getCANBadgerStatus(CAN2_LOGGING)))//enable the bridges so logging happens
	{
		if(!CANBridge(1))
		{
			oled.displayMessage("Bridge error");
			sd.closeFile();//clean up
			sd.deleteFile(filename);
			buttons.getButtonPressed();
			return false;//we are looking for a lot of conditions, but if somehow they are not met, then go back
		}
	}
	else if(getCANBadgerStatus(CAN_BRIDGE_ENABLED))
	{
		wasCANBridgeEnabled=true;//bridge was already enabled
	}
	if(!getCANBadgerStatus(KLINE_BRIDGE_ENABLED) && (getCANBadgerStatus(KLINE1_LOGGING) || getCANBadgerStatus(KLINE2_LOGGING)))
	{
		/*if(!KLINEBridge(1)) //need to create the kline bridge function
		{
			return false;//we are looking for a lot of conditions, but if somehow they are not met, then go back
		}*/
	}
	else if(getCANBadgerStatus(KLINE_BRIDGE_ENABLED))
	{
		wasKLINEBridgeEnabled=true;//bridge was already enabled
	}
	oled.displayMessage("Log interfaces:",1);
	if(getCANBadgerStatus(CAN1_LOGGING))//check on which interfaces to log. bitwise encoded
	{
		oled.displayMessage("-CAN1",1);
	}
	if(getCANBadgerStatus(CAN2_LOGGING))//check on which interfaces to log. bitwise encoded
	{
		oled.displayMessage("-CAN2",1);
	}
	if(getCANBadgerStatus(KLINE1_LOGGING))//check on which interfaces to log. bitwise encoded
	{
		oled.displayMessage("-KLINE1",1);
	}
	if(getCANBadgerStatus(KLINE2_LOGGING))//check on which interfaces to log. bitwise encoded
	{
		oled.displayMessage("-KLINE2",1);
	}
	oled.displayMessage(" ",1);
	oled.displayMessage(" Press back key ",1);
	oled.displayMessage("     to stop    ",1);
	/*	if(convert.getBit(interfaces,2))
	{
		setCANBadgerStatus(KLINE1_LOGGING,1);
	}	
	if(convert.getBit(interfaces,3))
	{
		setCANBadgerStatus(KLINE2_LOGGING,1);
	}	*/
	timer.stop();
	timer.reset();
	timer.start();//start it!
	while(buttons.isButtonPressed(4) == false)//log while the back button is not pressed
	{
		if(generalCounter2 > 0)//if we have pending data to retrieve from RAM
		{
			if(generalCounter3 == 4096)//if we have reached the end of the buffer space
			{
				generalCounter3 = 0;//reset it to make it circular!
			}
			readTmpBuffer(generalCounter3, 1, data);
			while(data[0] == 0)//to skip bytes intentionally left blank or if there was a read attempt that was interrupted
			{
				generalCounter3++;//we position the counter on the next byte
				if(generalCounter3 == 4096)//if we have reached the end of the buffer space
				{
					generalCounter3 = 0;//reset it to make it circular!
				}
				readTmpBuffer(generalCounter3, 1, data);
			}
			readTmpBuffer((generalCounter3 + 13),1,data);//retrieve the length of the payload
			uint32_t pLen=(data[0] + 14);//calculate the total length of the packet including header
			readTmpBuffer(generalCounter3,pLen,data);
			generalCounter3 = (generalCounter3 + pLen);//add the total length, and substract one so we are pointing at the last byte of the previous packet
			sd.write((char*)data,pLen);//write the data to the file
			generalCounter2--;//remove the packet we just wrote from the queue
			//frmCount++;
		}
	}
	oled.clearScreen();
/*	if(convert.getBit(interfaces,0))//disable logging
	{
		setCANBadgerStatus(CAN1_LOGGING,0);
	}
	if(convert.getBit(interfaces,1))
	{
		setCANBadgerStatus(CAN2_LOGGING,0);
	}	
	if(convert.getBit(interfaces,2))
	{
		setCANBadgerStatus(KLINE1_LOGGING,0);
	}	
	if(convert.getBit(interfaces,3))
	{
		setCANBadgerStatus(KLINE2_LOGGING,0);
	}		*/	
	bool CAN1Logging=false;
	bool CAN2Logging=false;
	bool KLINE1Logging=false;
	bool KLINE2Logging=false;
	if(getCANBadgerStatus(CAN1_LOGGING))//Disable logging so we dont end up getting more frames while we close the log
	{
		setCANBadgerStatus(CAN1_LOGGING,0);
		CAN1Logging=true;
	}
	if(getCANBadgerStatus(CAN2_LOGGING))//check on which interfaces to log. bitwise encoded
	{
		setCANBadgerStatus(CAN2_LOGGING,0);
		CAN2Logging=true;
	}
	if(getCANBadgerStatus(KLINE1_LOGGING))//check on which interfaces to log. bitwise encoded
	{
		setCANBadgerStatus(KLINE1_LOGGING,0);
		KLINE1Logging=true;
	}
	if(getCANBadgerStatus(KLINE2_LOGGING))//check on which interfaces to log. bitwise encoded
	{
		setCANBadgerStatus(KLINE2_LOGGING,0);
		KLINE2Logging=true;
	}	
	while(generalCounter2 > 0)//if we have pending data to retrieve from buffer after stopping the logger
	{
		if(generalCounter3 == 4096)//if we have reached the end of the buffer space
		{
			generalCounter3 = 0;//reset it to make it circular!
		}
		readTmpBuffer(generalCounter3, 1, data);
		while(data[0] == 0)//to skip bytes intentionally left blank
		{
			generalCounter3++;//we position the counter on the next byte
			if(generalCounter3 == 4096)//if we have reached the end of the buffer space
			{
				generalCounter3 = 0;//reset it to make it circular!
			}
			readTmpBuffer(generalCounter3, 1, data);
		}
		readTmpBuffer((generalCounter3 + 13),1,data);//retrieve the length of the payload
		uint32_t pLen=(data[0] + 14);//calculate the total length of the packet including header
		readTmpBuffer(generalCounter3,pLen,data);
		generalCounter3 = (generalCounter3 + pLen);//add the total length, and substract one so we are pointing at the last byte of the previous packet
		sd.write((char*)data,pLen);//write the data to the file
		generalCounter2--;//remove the packet we just wrote from the queue
		//frmCount++;
	}	
	if(wasCANBridgeEnabled == false)//if bridge was not enabled before logging, we disable it
	{
		CANBridge(0);
	}
	if(wasKLINEBridgeEnabled == false)//if bridge was not enabled before logging, we disable it
	{
		//KLINEBridge(0);
	}
	sd.closeFile();//close the log to save the file
/*	if(frmCount != generalCounter4)//used to measure performance
	{
		oled.displayMessage("Missed frames 1",1);
	}*/
	oled.clearScreen();
	oled.displayMessage("Log saved in:");
	oled.displayMessage(filename,1);
	if(CAN1Logging == true)//If Logging was enabled, re-enable it
	{
		setCANBadgerStatus(CAN1_LOGGING,1);
	}
	if(CAN2Logging == true)
	{
		setCANBadgerStatus(CAN2_LOGGING,1);
	}
	if(KLINE1Logging == true)
	{
		setCANBadgerStatus(KLINE1_LOGGING,1);
	}
	if(KLINE2Logging == true)
	{
		setCANBadgerStatus(KLINE2_LOGGING,1);
	}
	buttons.getButtonPressed();
	return 1;
}

///////////////////////
/////// MITM //////////
///////////////////////


// run MITM from a rule file
void CANbadger::MITMMode(char *filename, CANFormat format)
{
	oled.clearScreen();
	oled.displayMessage("Parsing rules..");
	clearRAM();//we fill RAM with 0xFF
	memset(tmpBuffer,0xFF,4096);//we also set up the buffer
	//char filename[]="/MITM/rules.txt";//path to rule file
	if(!sd.doesFileExist(filename))
	{

/*		if(uartMode)
			device.printf("Rules not found in SD, aborting...\n\n");
		else
			ethernetManager->debugLog("Rules not found on SD! Aborting!");
		setLED(LED_GREEN);*/
		oled.displayMessage("Rule not found",1);
		if(this->ethernet_manager == NULL) {buttons.getButtonPressed();}
		return;
	}
	CAN_MITM mitm(this, &can1, &can2, CANAny, tmpBuffer, &ram, &BackButton);//create the mitm object
	sd.openFile(filename, O_RDONLY);
	uint32_t rulesAllocated = 0;
	uint32_t IDsAllocated = 0;
	sd.lseekFile(0, SEEK_SET);
	while(1)//to read until the end file
	{
		uint32_t CondType = grabASCIIValue();//grab the condition type
		if (CondType == 0xFFFFFFFF)//check if EOF
		{
			break; //reached EOF
		}
		uint32_t targetID = grabASCIIValue();//grab the target ID
		uint8_t targetPayload[8] = {0};
		for (uint8_t a = 0;a<8; a++) //grab the target payload
		{
			targetPayload[a] = grabASCIIValue();
		}
		uint32_t action = grabASCIIValue();//grab the action data
		uint8_t actionPayload[8] = {0};
		for (uint8_t a = 0;a<8; a++) //grab the action payload
		{
			actionPayload[a] = grabASCIIValue();
		}
		 //now we check if there is already an entry for that ID
		uint32_t ruleOffset = mitm.tableLookUp(targetID);
		if(ruleOffset != 0xFFFFFFFF)//if an entry is found
		{
			if(!mitm.addRule(ruleOffset, CondType,targetPayload,action,actionPayload))//this works
			{
				/*if(uartMode)
					device.printf("Too many rules for ID 0x%x, Maximum number of rules per ID is 102...\n", targetID);
				else
					ethernetManager->sendFormattedDebugMessage("Too many rules for ID 0x%x, max. is 102!", targetID);*/
				oled.displayMessage("Too many rules",1);
			}
			else
			{
				rulesAllocated++;
			}
		}
		else //if an entry is not found
		{
			ruleOffset = mitm.getLastIDEntryOffset();//retrieve the last offset where a rule was stored
			if (ruleOffset == 0xFFFFFFFF)
			{
				/*if(uartMode)
					device.printf("Reached maximum number of IDs, will not be able to store rules for ID 0x%x...\n", targetID);
				else
					ethernetManager->sendFormattedDebugMessage("Reached max. number of IDs, will not be able to store rules for ID 0x%x..", targetID);*/
				oled.displayMessage("Rule limit hit",1);
			}
			else
			{
				uint32_t oPos = ruleOffset;
				ruleOffset = mitm.allocRAM(targetID, oPos);
				mitm.addRule(ruleOffset, CondType,targetPayload,action,actionPayload);
				IDsAllocated++;
				rulesAllocated++;
			}
		}
	}
//	device.printf("Done!\n\nLoaded a total of %d rules for %d IDs\n\n", rulesAllocated, IDsAllocated);
	oled.displayMessage("Done Loading",1);
	sd.closeFile();
	wait(2);
/*	if(uartMode)
		device.printf("Starting MITM mode, press any key to stop...\n\n");
	else
		ethernetManager->debugLog("Done loading rules, starting MITM mode..");*/
	oled.clearScreen();
	oled.displayMessage("MITM running");
	mitm.doMITM();
//	setLED(LED_GREEN);
/*	if(!uartMode)
		ethernetManager->debugLog("MITM stopped!");*/
	oled.clearScreen();
	oled.displayMessage("MITM stopped");
	wait(2);
}

// create a persistent MITM object, that can accept new rules
CAN_MITM* CANbadger::getNewMITM() {
	if(persistent_mitm != NULL) { delete persistent_mitm; }

	// clear RAM and tmpBufer containing old lookup table and old rules
	// fill with 0xFF to signal empty
	clearRAM();
	memset(tmpBuffer,0xFF,4096);

	persistent_mitm = new CAN_MITM(this, &can1, &can2, CANAny, tmpBuffer, &ram, &BackButton);
	return persistent_mitm;
}

// add a single rule (same format as .txt based rules) to persistent mitm object
bool CANbadger::addMITMRule(const char *rule) {
	if(persistent_mitm == NULL) { return false; }

	// declare space for parsed values
	uint32_t condType;
	uint32_t actionType;
	uint32_t targetID;
	uint8_t conditionPayload[8] = {0};
	uint8_t actionPayload[8] = {0};
	char* next;

	// parse condition and condition mask to uint32_t
	condType = atoh<uint32_t>(rule);
	next = strchr(rule, ',') + 1;

	// parse target ID
	targetID = atoh<uint32_t>(next);
	next = strchr(next, ',') + 1;

	// get contition payload as uint8_t array
	for(int i = 0; i < 8; i++) {
		conditionPayload[i] = atoh<uint8_t>(next);
		next = strchr(next, ',') + 1;
	}

	// parse action type and mask into uint32_t
	actionType = atoh<uint32_t>(next);
	next = strchr(next, ',') + 1;

	// get the action payload into a uint8_t array
	for(int i = 0; i < 8; i++) {
		actionPayload[i] = atoh<uint8_t>(next);
		next = strchr(next, ',') + 1;
	}

	// check for existing ID entry
	uint32_t ruleOffset = persistent_mitm->tableLookUp(targetID);


	if(ruleOffset != 0xFFFFFFFF) {  //if an entry is found, add the new rule
		return persistent_mitm->addRule(ruleOffset, condType,conditionPayload,actionType,actionPayload);
	}

	// get new offset for the new ID
	ruleOffset = persistent_mitm->getLastIDEntryOffset();  // retrieve the last offset where a rule was stored
	uint32_t oPos = ruleOffset;
	ruleOffset = persistent_mitm->allocRAM(targetID, oPos);  // get the new rule offset for the target ID
	if (ruleOffset == 0xFFFFFFFF) { return false; }  // no XRAM space available anymore

	// try to store the new rule and return the result
	return persistent_mitm->addRule(ruleOffset, condType,conditionPayload,actionType,actionPayload);
}

// start mitm with all rules added to persistent_mitm object, removes the persistent_mitm after use
bool CANbadger::startMITM() {
	if(persistent_mitm == NULL) { return false; }
	persistent_mitm->doMITM();

	// remove the CAN_MITM after it has done its job
	if(persistent_mitm != NULL) {
		delete persistent_mitm;
		persistent_mitm == NULL;
	}

	return true;
}


bool CANbadger::deleteFile(char *fileName)
{
	oled.clearScreen();
	oled.displayMessage("Delete file?:");
	oled.displayMessage(fileName,1);
	uint8_t a = buttons.getButtonPressed(0.15);//check which button was pressed
	oled.clearScreen();
	bool success = false;
	if(a == 1)
	{
		if(sd.deleteFile(fileName) == true)
		{
			oled.displayMessage("File Deleted");
			success = true;
		}
		else
		{
			oled.displayMessage("Error deleting");
		}
	}
	else
	{
		oled.displayMessage("Delete Canceled");
	}
	buttons.getButtonPressed();
	return success;
}


void CANbadger::doOLEDUDS(CAN *canbus, uint8_t interfaceno, bool isInSession)
{
	UDSCANHandler uds(canbus);//need to pass it to the functions below
	if(isInSession == true)//only used by security hijack
	{
		CANFormat formato;
		bool usePadding;
		if(getCANBadgerStatus(CAN1_STANDARD) == true)
		{
			formato = CANStandard;
		}
		else
		{
			formato = CANExtended;
		}
		if(getCANBadgerStatus(CAN1_USE_FULLFRAME) == true)
		{
			usePadding = true;
		}
		else
		{
			usePadding = false;
		}
		uds.setTransmissionParameters(localID, remoteID, formato, usePadding, CAN1PaddingByte);//so far we dont support extended addressing for hijack
		uds.setSessionStatus(true);
	}
	while(1)
	{
		oled.clearScreen();
		const char* options[15]={"Diag Session","R/W Data by ID", "DTC Information", "Comms Control", "R/W Mem by Addr", "Security Access", "Download/Upload"};
		uint8_t option = oled.showOLEDMenu((const char*)"UDS Menu", options, 8, &buttons);
		switch (option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				UDSCANDiagMenu(&uds, interfaceno);
				break;
			}
			case 2:
			{
				UDSCANDataMenu(&uds);
				break;
			}
			case 3:
			{
				UDSCANDTCMenu(&uds);
				break;
			}
			case 4:
			{
				UDSCANCommsControlMenu(&uds);
				break;
			}
			case 5:
			{
				UDSCANMemoryMenu(&uds);
				break;
			}
			case 6:
			{
				UDSCANSecAccessMenu(&uds);
				break;
			}
			case 7:
			{
				UDSCANTransferMenu(&uds);
				break;
			}
		}
	}
}

void CANbadger::UDSCANTransferMenu(UDSCANHandler *uds)
{
	const char* options[15]={"Set Address", "Set Data Format", "Upload from DUT"};
	uint8_t option = 1;
	uint64_t memAddress=0;//address to read from or write to
	uint32_t requestSize=0;//used to know how big the upload or download is
	uint8_t dataFormat=0;//used to store the data format
	while(1)
	{
		option = oled.showOLEDMenu("UDS Upload Menu", options, 3, &buttons);
		oled.clearScreen();
		switch(option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				uint8_t tmpbyte=0;
				uint8_t tmplen=1;
				uint64_t tmpMemAddr=0;
				if(!getHexValue("Address length:",(uint64_t*)(&tmplen),1,5,1))
				{
					break;
				}
				for(uint8_t a = 0; a < tmplen; a++)//now we get the length
				{
					char tmpstrng[32]="Enter MSB";
					char tmpstrng2[6];
					convert.itox((a + 1),tmpstrng2,1);
					strcat(tmpstrng,tmpstrng2);
					strcat(tmpstrng,":");
					if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
					{
						break;
					}
					tmpMemAddr = (tmpMemAddr << 8) + tmpbyte;
				}
				oled.clearScreen();
				oled.displayMessage("Entered Address:");
				oled.displayMessage("0x",1);
				char z[22];
				convert.itox(tmpMemAddr,z,(tmplen * 2));
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				tmplen=buttons.getButtonPressed();
				if(tmplen == 1)//if user presses ok
				{
					memAddress = tmpMemAddr;
				}
				break;
			}
			case 2:
			{
				oled.clearScreen();
				oled.displayMessage("****WARNING**** ");
				oled.displayMessage("  DO NOT CHANGE ",1);
				oled.displayMessage("THIS UNLESS YOU ",1);
				oled.displayMessage(" KNOW WHAT YOU",1);
				oled.displayMessage("   ARE DOING",1);
				oled.displayMessage("****WARNING**** ",1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() != 1)//if user doesnt press ok
				{
					break;
				}
				uint8_t tmpbyte=0;
				if(!getHexValue("Format Byte:",(uint64_t*)(&tmpbyte),0,0xFF,1))
				{
					break;
				}
				oled.clearScreen();
				oled.displayMessage("Entered Format:");
				oled.displayMessage("0x",1);
				char z[22];
				convert.itox(tmpbyte,z,2);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() == 1)//if user presses ok
				{
					dataFormat = tmpbyte;
				}
				break;
			}
			case 3:
			{
				uint8_t tmpbyte=0;
				uint32_t tmplen=1;
				uint32_t tmpRequestSize=0;
				if(!getHexValue("Upload size:",(uint64_t*)(&tmplen),1,4,1))
				{
					break;
				}
				for(uint8_t a = 0; a < tmplen; a++)//now we get the length
				{
					char tmpstrng[32]="Enter MSB";
					char tmpstrng2[6];
					convert.itox((a + 1),tmpstrng2,1);
					strcat(tmpstrng,tmpstrng2);
					strcat(tmpstrng,":");
					if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
					{
						break;
					}
					tmpRequestSize = (tmpRequestSize << 8) + tmpbyte;
				}
				oled.clearScreen();
				char z[22];
				oled.displayMessage("Entered Address:",1);
				oled.displayMessage("0x",1);
				convert.itox(memAddress,z,10);
				oled.displayMessage(z,0,1);
				oled.displayMessage("Entered Size:",1);
				oled.displayMessage("0x",1);
				convert.itox(tmpRequestSize,z,(tmplen * 2));
				oled.displayMessage(z,0,1);
				oled.displayMessage("Format: 0x",1);
				convert.itox(dataFormat,z,2);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() != 1)//if user doesnt press ok
				{
					break;
				}
				requestSize = tmpRequestSize;
				oled.clearScreen();
				oled.displayMessage("Request upload..");
				uint32_t reply = uds->requestUpload(dataFormat, memAddress, requestSize, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout",1);
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					uint32_t blockSize=(uds->getBlockSize(tmpBuffer) - 2);//substract 2 for the SID and block counter
					uint8_t blockCounter=1;//we start with 1, and let it overflow if it happens
					oled.displayMessage("Create file..",1);
					uint16_t logNumber=1;//for sequential log writing
					bool itsDone=false;//to know when we found an available filename
					char tmpbuf[64]={0};
					while(!itsDone)
					{
						char filename2[90]="/Transfers/CAN/Uploads/";
						memset(tmpbuf,0,64);
						char sequence[22];//if we need more than 5 digits for a log number, i think its time to cleanup the SD
						if(remoteID < 0x10000)
						{
							convert.itox(remoteID,sequence,4);
						}
						else
						{
							convert.itox(remoteID,sequence,8);
						}
						strcat(filename2,sequence);
						strcat(filename2,"_");
						sprintf(tmpbuf,"%x_",memAddress);
						strcat (filename2,tmpbuf);
						convert.itox(logNumber,sequence,4);
						strcat(filename2,sequence);
						strcat(filename2,".BIN");
						if(!sd.doesFileExist(filename2))
						{
							itsDone=true;
						}
						else
						{
							logNumber++;
						}
						if(logNumber > 65535)
						{
							oled.displayMessage("Logs folder full",1);
							buttons.getButtonPressed();
							break;
						}
					}
					if(itsDone == true)
					{
						char filename2[90]="/Transfers/CAN/Uploads/";
						char sequence[22];//if we need more than 5 digits for a log number, i think its time to cleanup the SD
						if(remoteID < 0x1000)
						{
							convert.itox(remoteID,sequence,4);
						}
						else
						{
							convert.itox(remoteID,sequence,8);
						}
						strcat(filename2,sequence);
						strcat(filename2,"_");
						memset(tmpbuf,0,64);
						sprintf(tmpbuf,"%x_",memAddress);
						strcat (filename2,tmpbuf);
						convert.itox(logNumber,sequence,4);
						strcat(filename2,sequence);
						strcat(filename2,".BIN");
						if (!sd.openFile(filename2, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
						{
							oled.displayMessage("SD Write error",1);
						}
						else
						{
							oled.displayMessage("Transfering Data",1);
							for(uint32_t w=0; w < requestSize; w=(w+blockSize))//loop for transfer data
							{
								reply = uds->transferData(blockCounter, tmpBuffer, 0, tmpBuffer);//get the block
								if(reply == 0)//if we have no reply
								{
									oled.displayMessage("Timeout,1");
									sd.closeFile();
									buttons.getButtonPressed();
									break;
								}
								else if ((reply & 0xFFFF0000) != 0)//if there was an error
								{
									uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
									sd.closeFile();
									displayError(tmpBuffer);
									break;
								}
								if((requestSize - w) >= blockSize)//if it is a full length reply
								{
									sd.write((char*)tmpBuffer + 2,blockSize);//write the reply to SD
								}
								else//but if it is not, we avoid potential padding bytes
								{
									sd.write((char*)tmpBuffer + 2,(requestSize - w));//write the reply to SD
								}
								blockCounter++;
							}
							sd.closeFile();
							oled.displayMessage("Dump saved to:",1);
							oled.displayMessage(filename2,1);
						}
						buttons.getButtonPressed();
					}
				}
				break;
			}
/*			case 4: //too dangerous to enable
			{
				char tmpFileName[90]="/Transfers/CAN/Downloads";
				char filename[90];
				if(getFileName(tmpFileName))
				{
					memset(filename,0,64);//clean the previous one
					strcat(filename,"/Transfers/CAN/Downloads/");
					strcat(filename, tmpFileName);//place the new one
				}
				else
				{
					break;
				}
				uint32_t totalLen = sd.getFileSize(filename);
				if (!sd.openFile(filename, O_RDONLY))//if we couldnt open the file, then same thing
				{
					oled.displayMessage("SD Error");
					buttons.getButtonPressed();
					break;
				}
				oled.clearScreen();
				char z[22];
				oled.displayMessage("Entered Address:",1);
				oled.displayMessage("0x",1);
				convert.itox(memAddress,z,10);
				oled.displayMessage(z,0,1);
				oled.displayMessage("File Size:",1);
				oled.displayMessage("0x",1);
				convert.itox(totalLen,z,8);
				oled.displayMessage(z,0,1);
				oled.displayMessage("Format: 0x",1);
				convert.itox(dataFormat,z,2);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() != 1)//if user doesnt press ok
				{
					break;
				}
				oled.clearScreen();
				oled.displayMessage("Request Download");
				uint32_t reply = uds->requestUpload(dataFormat, memAddress, requestSize, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout,1");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					uint32_t blockSize=(uds->getBlockSize(tmpBuffer) - 2);//substract 2 for the SID and block counter
					uint8_t blockCounter=1;//we start with 1, and let it overflow if it happens
					oled.displayMessage("Transfer Data...",1);
					bool isComplete=false;
					uint32_t reply=0;
					while(!isComplete)
					{
						uint32_t aa = sd.read((char*)tmpBuffer, blockSize);
						if(aa != blockSize)//if we have reached the end of the file
						{
							isComplete=true;
						}
						if(aa != 0)//corner case, but could happen
						{
							reply = uds->transferData(blockCounter, tmpBuffer, aa, tmpBuffer);
							if(reply == 0)//if we have no reply
							{
								oled.displayMessage("Timeout,1");
								buttons.getButtonPressed();
								break;
							}
							else if ((reply & 0xFFFF0000) != 0)//if there was an error
							{
								uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
								displayError(tmpBuffer);
								break;
							}
						}
						blockCounter++;
					}
					reply = uds->requestTransferExit(tmpBuffer, 0, tmpBuffer);
					sd.closeFile();
					if(isComplete)//if download was successful
					{
						oled.displayMessage("Download OK!",1);
						buttons.getButtonPressed();
					}
					break;
				}
				break;
			}*/
			default:
			{
				return;
			}
		}
	}
}



void CANbadger::UDSCANCommsControlMenu(UDSCANHandler *uds)
{
	UDSCANHandler* _uds = uds;
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	uint8_t ctrType = 0;
	uint8_t commType=1;
	uint16_t nodeID=0;
	char z[22];
	while(1)
	{
		oled.clearScreen();
		oled.displayMessage("UDS CommsC Menu");//show the header
		oled.displayMessage(" Ctrl: ",1);
		switch (ctrType)
		{
			case 0:
			{
				oled.displayMessage("TX RX",0,1);
				break;
			}
			case 1:
			{
				oled.displayMessage("ONLY RX",0,1);
				break;
			}
			case 2:
			{
				oled.displayMessage("ONLY TX",0,1);
				break;
			}
			case 3:
			{
				oled.displayMessage("NO TX RX",0,1);
				break;
			}
			case 4:
			{
				oled.displayMessage("ONLY RX E",0,1);
				break;
			}
			case 5:
			{
				oled.displayMessage("TX RX E",0,1);
				break;
			}
			default:
			{
				convert.itox(ctrType,z,2);
				oled.displayMessage(z,0,1);
				break;
			}
		}
		oled.displayMessage(" CType: ",1);//show if standard or extended format is being used
		switch (commType & 0x0F)
		{
			case 1:
			{
				oled.displayMessage("NORMAL",0,1);
				break;
			}
			case 2:
			{
				oled.displayMessage("N_MNGMNT",0,1);
				break;
			}
			case 3:
			{
				oled.displayMessage("ALL",0,1);
				break;
			}
		}
		oled.displayMessage(" Node: ",1);//show if standard or extended format is being used
		switch (commType & 0xF0)
		{
			case 0:
			{
				oled.displayMessage("THIS",0,1);
				break;
			}
			case 0xF0:
			{
				oled.displayMessage("NETWORK",0,1);
				break;
			}
			default://used for custom ones
			{
				convert.itox((commType >> 4),z,1);
				oled.displayMessage("0x",0,1);
				oled.displayMessage(z,0,1);
				break;
			}
		}
		if(ctrType == 4 || ctrType == 5)
		{
			oled.displayMessage(" NodeID: 0x",1);
			convert.itox(nodeID,z,4);
			oled.displayMessage(z,0,1);
		}
		oled.displayMessage(" GO!",1);
		oled.set_text_rc(chosenOption,0);//we set the cursor on current highlight
		oled.putc(arrow);
		bool optChanged=false;
		while(!optChanged)
		{
			uint8_t a = buttons.getButtonPressed();//check which button was pressed
			if(a == 1)
			{
				if(chosenOption == 1)
				{
					const char* options[15]={"TX-ON RX-ON", "TX-OFF RX-ON", "TX-ON RX-OFF", "TX-OFF RX-OFF", "TX-OFF ENH ADDR", "TX-ON ENH ADDR", "CUSTOM"};
					uint8_t option = oled.showOLEDMenu("Choose Type:", options, 7, &buttons);
					switch(option)
					{
						case 0://back button was pressed
						{
							break;
						}
						case 1:
						{
							ctrType=0;
							break;
						}
						case 2:
						{
							ctrType=1;
							break;
						}
						case 3:
						{
							ctrType=2;
							break;
						}
						case 4:
						{
							ctrType=3;
							break;
						}
						case 5:
						{
							ctrType=4;
							break;
						}
						case 6:
						{
							ctrType=5;
							break;
						}
						case 7:
						{
							getHexValue("Enter CTRL Type:",(uint64_t*)(&ctrType),6,0xFF,1);
							break;
						}
					}
					optChanged=true;//we need to reload the screen even if nothing changes
				}
				else if(chosenOption == 2)
				{
					const char* options[15]={"NORMAL", "NET MANAGEMENT", "ALL"};
					uint8_t option = oled.showOLEDMenu("Choose Type:", options, 3, &buttons);
					switch(option)
					{
						case 0://back button was pressed
						{
							break;
						}
						case 1:
						{
							commType = (commType & 0xF0);//clean up the lower part
							commType = (commType + 1);
							break;
						}
						case 2:
						{
							commType = (commType & 0xF0);//clean up the lower part
							commType = (commType + 2);
							break;
						}
						case 3:
						{
							commType = (commType & 0xF0);//clean up the lower part
							commType = (commType + 3);
							break;
						}
					}
					optChanged=true;//we need to reload the screen even if nothing changes
				}
				else if(chosenOption == 3)
				{
					const char* options[15]={"THIS NODE", "ALL NODES", "CUSTOM"};
					uint8_t option = oled.showOLEDMenu("Choose Subnet:", options, 3, &buttons);
					switch(option)
					{
						case 0://back button was pressed
						{
							break;
						}
						case 1:
						{
							commType = (commType & 0x0F);//clean up the lower part
							break;
						}
						case 2:
						{
							commType = (commType & 0x0F);//clean up the lower part
							commType = (commType + 0xF0);
							break;
						}
						case 3:
						{
							uint8_t tmpval=0;
							if(getHexValue("Enter Subnet:",(uint64_t*)(&tmpval),1,0xE,1))
							{
								commType = (commType & 0x0F);//clean up the lower part
								commType = (commType + (tmpval << 4));
							}
							break;
						}
					}
					optChanged=true;//we need to reload the screen even if nothing changes
				}
				else if(chosenOption == 4)
				{
					if(ctrType == 4 || ctrType == 5)
					{
						uint8_t tmpID=0;
						if(getHexValue("Upper NID Byte:",(uint64_t*)(&tmpID),0,0xFF,1))
						{
							nodeID = tmpID;
							nodeID = (nodeID << 8);
							if(getHexValue("Lower NID Byte:",(uint64_t*)(&tmpID),0,0xFF,1))
							{
								nodeID = (nodeID + tmpID);
							}
						}
					}
					else
					{
						uint32_t reply = _uds->communicationControl(ctrType, commType, tmpBuffer, nodeID);
						if(reply == 0)//if we have no reply
						{
							oled.clearScreen();
							oled.displayMessage("Timeout");
							buttons.getButtonPressed();
						}
						else if ((reply & 0xFFFF0000) != 0)//if there was an error
						{
							uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
							displayError(tmpBuffer);
						}
						else//but if it worked...
						{
							oled.clearScreen();
							oled.displayMessage("Success!");
							buttons.getButtonPressed();
						}
					}
					optChanged=true;//we need to reload the screen even if nothing changes
				}
				else if(chosenOption == 5)
				{
					uint32_t reply = _uds->communicationControl(ctrType, commType, tmpBuffer, nodeID);
					if(reply == 0)//if we have no reply
					{
						oled.clearScreen();
						oled.displayMessage("Timeout");
						buttons.getButtonPressed();
					}
					else if ((reply & 0xFFFF0000) != 0)//if there was an error
					{
						uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
						displayError(tmpBuffer);
					}
					else//but if it worked...
					{
						oled.clearScreen();
						oled.displayMessage("Success!");
						buttons.getButtonPressed();
					}
					optChanged=true;//we need to reload the screen even if nothing changes
				}

			}
			else if (a == 2)//One option down, the "+" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if((b == 4) && (ctrType != 4 && ctrType != 5))//in order to not overwrite the header
				{
					b = 1;
				}
				else if((b == 5) && (ctrType == 4 || ctrType == 5))//in order to not overwrite the header
				{
					b = 1;
				}
				else
				{
					b++;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 3)//One option up, the "-" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 1)//in order to not overwrite the header
				{
					if(ctrType != 4 && ctrType != 5)
					{
						b = 4;
					}
					else
					{
						b = 5;
					}
				}
				else
				{
					b--;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 4)
			{
				return;
			}
		}
	}
}



void CANbadger::UDSCANSecAccessMenu(UDSCANHandler *uds)
{
	const char* options[15]={"Request SEED", "Send KEY"};
	uint8_t option = 1;
	while(1)
	{
		option = oled.showOLEDMenu("UDS SecAcc Menu", options, 2, &buttons);
		oled.clearScreen();
		switch(option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				uint8_t lvl = 0;
				if(!getHexValue("SecAccess lvl:",(uint64_t*)(&lvl),0,0xFF,1))
				{
					break;
				}
				oled.displayMessage("Request lvl 0x");
				char z[22];
				convert.itox(lvl,z,2);
				oled.displayMessage(z,0,1);
				uint32_t reply = uds->requestSeed(lvl, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
						uint16_t lenn = reply & 0xFFFF;
						if(lenn < 56)//if we can fit it in the screen
						{
							oled.clearScreen();
							oled.displayMessage("SEED: 0x");
							char toShow[128]={0};
							for(uint8_t a = 2; a < lenn; a++)
							{
								char sequence[3]={0};
								convert.itox(tmpBuffer[a],sequence,2);
								strcat(toShow,sequence);
							}
							oled.displayMessage(toShow,0,1);
							buttons.getButtonPressed();
							oled.clearScreen();
						}
				}
				break;
			}
			case 2:
			{
				uint32_t key = 0;
				uint8_t lvl=0;
				uint8_t tmpValue=0;
				if(!getHexValue("SecAccess lvl:",(uint64_t*)(&lvl),0,0xFF,1))
				{
					break;
				}
				if(!getHexValue("Key Byte 1:",(uint64_t*)(&tmpValue),0,0xFF,1))
				{
					break;
				}
				key = tmpValue;
				key = (key << 8);
				if(!getHexValue("Key Byte 2:",(uint64_t*)(&tmpValue),0,0xFF,1))
				{
					break;
				}
				key = (key + tmpValue);
				key = (key << 8);
				if(!getHexValue("Key Byte 3:",(uint64_t*)(&tmpValue),0,0xFF,1))
				{
					break;
				}
				key = (key + tmpValue);
				key = (key << 8);
				if(!getHexValue("Key Byte 4:",(uint64_t*)(&tmpValue),0,0xFF,1))
				{
					break;
				}
				key = (key + tmpValue);
				oled.displayMessage("Reply lvl 0x");
				char z[22];
				convert.itox(lvl,z,2);
				oled.displayMessage(z,0,1);
				oled.displayMessage("Key 0x",1);
				convert.itox(key,z,8);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() != 1)//if start button is not pressed
				{
					break;
				}
				uint32_t reply = uds->sendKey(lvl, key, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					oled.clearScreen();
					oled.displayMessage("SeccAccess OK!");
					buttons.getButtonPressed();
				}
				break;
			}
			default:
			{
				return;
			}
		}
	}
}

void CANbadger::TP20SecAccessMenu(KWP2KTP20Handler *tp20)
{
	const char* options[15]={"Request SEED", "Send KEY"};
	uint8_t option = 1;
	while(1)
	{
		option = oled.showOLEDMenu("TP2 SecAcc Menu", options, 2, &buttons);
		oled.clearScreen();
		switch(option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				uint8_t lvl = 0;
				if(!getHexValue("SecAccess lvl:",(uint64_t*)(&lvl),0,0xFF,1))
				{
					break;
				}
				oled.displayMessage("Request lvl 0x");
				char z[22];
				convert.itox(lvl,z,2);
				oled.displayMessage(z,0,1);
				uint32_t reply = tp20->requestSeed(lvl, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					tp20->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
						uint16_t lenn = reply & 0xFFFF;
						if(lenn < 56)//if we can fit it in the screen
						{
							oled.clearScreen();
							oled.displayMessage("SEED: 0x");
							char toShow[128]={0};
							for(uint8_t a = 2; a < lenn; a++)
							{
								char sequence[3]={0};
								convert.itox(tmpBuffer[a],sequence,2);
								strcat(toShow,sequence);
							}
							oled.displayMessage(toShow,0,1);
							buttons.getButtonPressed();
							oled.clearScreen();
						}
				}
				break;
			}
			case 2:
			{
				uint32_t key = 0;
				uint8_t lvl=0;
				uint8_t tmpValue=0;
				if(!getHexValue("SecAccess lvl:",(uint64_t*)(&lvl),0,0xFF,1))
				{
					break;
				}
				if(!getHexValue("Key Byte 1:",(uint64_t*)(&tmpValue),0,0xFF,1))
				{
					break;
				}
				key = tmpValue;
				key = (key << 8);
				if(!getHexValue("Key Byte 2:",(uint64_t*)(&tmpValue),0,0xFF,1))
				{
					break;
				}
				key = (key + tmpValue);
				key = (key << 8);
				if(!getHexValue("Key Byte 3:",(uint64_t*)(&tmpValue),0,0xFF,1))
				{
					break;
				}
				key = (key + tmpValue);
				key = (key << 8);
				if(!getHexValue("Key Byte 4:",(uint64_t*)(&tmpValue),0,0xFF,1))
				{
					break;
				}
				key = (key + tmpValue);
				oled.displayMessage("Reply lvl 0x");
				char z[22];
				convert.itox(lvl,z,2);
				oled.displayMessage(z,0,1);
				oled.displayMessage("Key 0x",1);
				convert.itox(key,z,8);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() != 1)//if start button is not pressed
				{
					break;
				}
				uint32_t reply = tp20->sendKey(lvl, key, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					tp20->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					oled.clearScreen();
					oled.displayMessage("SeccAccess OK!");
					buttons.getButtonPressed();
				}
				break;
			}
			default:
			{
				return;
			}
		}
	}
}



void CANbadger::UDSCANDiagMenu(UDSCANHandler *uds, uint8_t interfaceno)
{
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	uint8_t addressType = 0;
	uint8_t addressByte = 0xF1;
	CANFormat formato;
	bool usePadding;
	uint8_t paddingByte;
	if(interfaceno == 1)//get the existing configuration
	{
		if(getCANBadgerStatus(CAN1_STANDARD) == true)
		{
			formato = CANStandard;
		}
		else
		{
			formato = CANExtended;
		}
		if(getCANBadgerStatus(CAN1_USE_FULLFRAME) == true)
		{
			usePadding = true;
			paddingByte = CAN1PaddingByte;
		}
		else
		{
			usePadding = false;
		}		
		
	}
	else
	{
		if(getCANBadgerStatus(CAN2_STANDARD) == true)
		{
			formato = CANStandard;
		}
		else
		{
			formato = CANExtended;
		}		
		if(getCANBadgerStatus(CAN2_USE_FULLFRAME) == true)
		{
			usePadding = true;
			paddingByte = CAN2PaddingByte;
		}
		else
		{
			usePadding = false;
		}	
	}
	while(1)
	{
		oled.clearScreen();
		oled.displayMessage(" UDS Diag Menu ");//show the header
		oled.displayMessage(" tID: 0x",1);
		char z[22];
		convert.itox(localID,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" rID: 0x",1);
		convert.itox(remoteID,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Diag type: 0x",1);
		convert.itox(currentDiagSession,z,2);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Status: ",1);
		if(uds->sessionStatus() == true)
		{
			oled.displayMessage("ONLINE",0,1);
		}
		else
		{
			oled.displayMessage("OFFLINE",0,1);
		}
		oled.displayMessage(" Format: ",1);//show if standard or extended format is being used
		if(formato == CANStandard)
		{
			oled.displayMessage("STD",0,1);
		}
		else
		{
			oled.displayMessage("XTD",0,1);
		}
		oled.displayMessage(" Padding: ",1);
		if(usePadding == true)
		{
			convert.itox(paddingByte,z,2);
		}
		else
		{
			z[0]='O';
			z[1]='F';
			z[2]='F';
			z[3]=0;
		}
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Addressing: ",1);
		if(addressType == 0)
		{
			z[0]='S';
			z[1]='T';
			z[2]='D';
			z[3]=0;
		}
		else
		{
			convert.itox(addressByte,z,2);
		}	
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Start Session",1);
		oled.set_text_rc(chosenOption,0);//we set the cursor on current highlight
		oled.putc(arrow);
		bool optChanged=false;
		while(!optChanged)
		{
			uint8_t a = buttons.getButtonPressed();//check which button was pressed
			if(a == 1)
			{
				if(chosenOption == 1)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=0;
					uint32_t tmplocalID=0;
					if(formato == CANStandard)
					{
						tmplen=2;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					else
					{
						tmplen=4;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x1F,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					oled.clearScreen();
					oled.displayMessage("Entered ID:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						localID = tmplocalID;
						uds->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					}
					optChanged=true;
				}
				else if(chosenOption == 2)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=0;
					uint32_t tmplocalID=0;
					if(formato == CANStandard)
					{
						tmplen=2;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					else
					{
						tmplen=4;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x1F,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					oled.clearScreen();
					oled.displayMessage("Entered RID:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						remoteID = tmplocalID;
						uds->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					}
					optChanged=true;
				}
				else if(chosenOption == 3)
				{				
					getHexValue("Enter Session:",(uint64_t*)(&currentDiagSession),0,0xFF,1);
					optChanged=true;
				}
				else if(chosenOption == 4)
				{
					if(uds->sessionStatus() == true)
					{
						uds->setSessionStatus(false);
					}
					else
					{
						oled.clearScreen();
						oled.displayMessage("Start diagnostic",1);
						oled.displayMessage(" session to go  ",1);
						oled.displayMessage("     online     ",1);
						wait(2);
					}
					optChanged=true;
				}
				else if(chosenOption == 5)
				{
					if(interfaceno == 1)//if CAN1
					{
						if(getCANBadgerStatus(CAN1_STANDARD) == true)
						{
							setCANBadgerStatus(CAN1_STANDARD,0);
							setCANBadgerStatus(CAN1_EXTENDED,1);
							formato=CANExtended;
							uds->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
						}
						else
						{
							setCANBadgerStatus(CAN1_STANDARD,1);
							setCANBadgerStatus(CAN1_EXTENDED,0);
							formato=CANStandard;
							uds->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
						}
					}
					else
					{
						if(getCANBadgerStatus(CAN2_STANDARD) == true)
						{
							setCANBadgerStatus(CAN2_STANDARD,0);
							setCANBadgerStatus(CAN2_EXTENDED,1);
							formato=CANExtended;
							uds->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
						}
						else
						{
							setCANBadgerStatus(CAN2_STANDARD,1);
							setCANBadgerStatus(CAN2_EXTENDED,0);
							formato=CANStandard;
							uds->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
						}				
					}
				}		
				else if(chosenOption == 6)
				{
					if(usePadding)
					{
						usePadding = false;
						if(interfaceno == 1)
						{
							setCANBadgerStatus(CAN1_USE_FULLFRAME,0);
						}
						else
						{
							setCANBadgerStatus(CAN2_USE_FULLFRAME,0);
						}
						uds->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					}
					else
					{
						usePadding = true;					
						getHexValue("Enter padding:",(uint64_t*)(&paddingByte),0,0xFF,1);
						if(interfaceno == 1)
						{
							setCANBadgerStatus(CAN1_USE_FULLFRAME,1);
							CAN1PaddingByte	= paddingByte;
						}
						else
						{
							setCANBadgerStatus(CAN2_USE_FULLFRAME,1);
							CAN2PaddingByte	= paddingByte;
						}
						uds->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
						
					}
					optChanged=true;
				}

				else if(chosenOption == 7)
				{
					if(addressType == 0)
					{
						if(getHexValue("Enter Address:",(uint64_t*)(&addressByte),0,0xFF,1))
						{
							addressType = 1;
							optChanged=true;
						}
					}
					else
					{
						addressType = 0;
						optChanged=true;
					}
					uds->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
				}
				else if(chosenOption == 8)
				{
					oled.clearScreen();
					oled.displayMessage("Connecting...");
					uds->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					generalCounter1 = uds->DiagSessionControl(currentDiagSession);//we just use the counter to grab the code
					if(generalCounter1 == 0)
					{
						oled.displayMessage("Timeout",1);
						wait(2);
					}
					else if((generalCounter1 & 0xFFFF0000) != 0)
					{
						uds->checkError((uint8_t)(generalCounter1 >> 16), tmpBuffer);
						displayError(tmpBuffer);
					}
					else
					{
						oled.displayMessage("Connected!",1);
						buttons.getButtonPressed();
					}
					optChanged=true;
				}
			}			
			else if (a == 2)//One option down, the "+" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 8)//in order to not overwrite the header
				{
					b = 1;
				}
				else
				{
					b++;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 3)//One option up, the "-" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 1)//in order to not overwrite the header
				{
					b = 8;
				}
				else
				{
					b--;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}	
			else if (a == 4)
			{
				return;
			}
		}
	}
}
	
void CANbadger::displayError(uint8_t* errorString)
{
	oled.clearScreen();
	oled.displayMessage("ERROR:");
	oled.displayMessage((const char *)errorString,1);
	buttons.getButtonPressed();
}

void CANbadger::UDSCANDataMenu(UDSCANHandler *uds)
{
	const char* options[15]={"Read by ID", "Write by ID"};
	uint8_t option = 1;
	while(1)
	{
		option = oled.showOLEDMenu(" UDS Data Menu", options, 2, &buttons);
		oled.clearScreen();
		switch(option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				uint16_t dataID = 0;
				uint8_t tmpDataID=0;
				if(!getHexValue("Upper ID Byte:",(uint64_t*)(&tmpDataID),0,0xFF,1))
				{
					break;
				}
				dataID = tmpDataID;
				dataID = (dataID << 8);
				if(!getHexValue("Lower ID Byte:",(uint64_t*)(&tmpDataID),0,0xFF,1))
				{
					break;
				}
				dataID = (dataID + tmpDataID);
				oled.displayMessage("DID: 0x");
				char z[22];
				convert.itox(dataID,z,4);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				uint8_t u = buttons.getButtonPressed();
				if(u == 4)//if back button pressed
				{
					break;
				}
				uint32_t reply = uds->readDataByID(dataID, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					uint16_t lenn = reply & 0xFFFF;
					if(lenn < 56)//if we can fit it in the screen
					{
						oled.clearScreen();
						oled.displayMessage("REPLY: ");
						char toShow[128]={0};
						for(uint8_t a = 3; a < lenn; a++)
						{
							char sequence[3]={0};
							convert.itox(tmpBuffer[a],sequence,2);
							strcat(toShow,sequence);
						}
						oled.displayMessage(toShow,0,1);
						buttons.getButtonPressed();
						oled.clearScreen();
					}
					char filename[90];
			    	memset(filename,0,90);
			    	strcat(filename, "/MemDumps/DID/UDS/");
					char tmpstrg[30];
					memset(tmpstrg,0,30);
					sprintf(tmpstrg,"%x_DID_%x_",remoteID,dataID);
					strcat(filename,tmpstrg);
			    	char fileExtension[6]=".BIN";
			    	if(!sd.getSequencialFileName(filename, fileExtension))
			    	{
			    		oled.clearScreen();
			    		oled.displayMessage(" Filename Error ");
			    		oled.displayMessage("****************",1);
			    		oled.displayMessage("  If limit of",1);
			    		oled.displayMessage(" 65535 files in",1);
			    		oled.displayMessage(" logs folder is",1);
			    		oled.displayMessage("  reached this",1);
			    		oled.displayMessage("  will happen",1);
			    		oled.displayMessage("****************",1);
			    		buttons.getButtonPressed();
			    		break;
			    	}
					if (!sd.openFile(filename, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
					{
						oled.displayMessage("SD Write error",1);
					}
					else
					{
						for(uint8_t a = 3; a < lenn; a++)
						{
							tmpBuffer[(a - 3)] = tmpBuffer[a];
						}
						lenn = (lenn - 3);
						sd.write((char*)tmpBuffer,lenn);
						sd.closeFile();
						oled.displayMessage("Dump saved to:");
						oled.displayMessage(filename,1);
						buttons.getButtonPressed();
					}
				}
				break;
			}
			case 2:
			{
				uint16_t dataID = 0;
				uint8_t tmpDataID=0;
				if(!getHexValue("Upper ID Byte:",(uint64_t*)(&tmpDataID),0,0xFF,1))
				{
					break;
				}
				dataID = tmpDataID;
				dataID = (dataID << 8);
				if(!getHexValue("Lower ID Byte:",(uint64_t*)(&tmpDataID),0,0xFF,1))
				{
					break;
				}
				dataID = (dataID + tmpDataID);
				oled.displayMessage("DID: 0x");
				char z[22];
				convert.itox(dataID,z,4);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				uint8_t u = buttons.getButtonPressed();
				if(u == 4)//if back button pressed
				{
					break;
				}
				char tmpFileName[90]="/MemDumps/DID/UDS";
				char filename[90];
				if(getFileName(tmpFileName))
				{
					memset(filename,0,64);//clean the previous one
					strcat(filename,"/MemDumps/DID/UDS/");
					strcat(filename, tmpFileName);//place the new one
				}
				else
				{
					break;
				}
				uint32_t totalLen = sd.getFileSize(filename);
				if (!sd.openFile(filename, O_RDONLY))//if we couldnt open the file, then same thing
				{
					oled.clearScreen();
					oled.displayMessage("SD read error");
					buttons.getButtonPressed();
					break;
				}
				memset(tmpBuffer,0,4096);
				uint32_t reply;
				oled.displayMessage("Writing data...");
				sd.read((char*)tmpBuffer, totalLen);
				sd.closeFile();
				reply = uds->writeDataByID(dataID, tmpBuffer, totalLen, tmpBuffer);//we send the request
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout",1);
					buttons.getButtonPressed();
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
				}
				else//but if it worked...
				{
					oled.displayMessage("Data Write OK",1);
					buttons.getButtonPressed();
				}
				break;
			}
			default:
			{
				return;
			}
		}	
	}
}
	
void CANbadger::UDSCANDTCMenu(UDSCANHandler *uds)
{
	const char* options[15]={"Read DTC number", "Read DTCs", "Clear ALL DTCs"};
	uint8_t option = 1;
	while(1)
	{
		option = oled.showOLEDMenu(" UDS DTC Menu", options, 3, &buttons);
		oled.clearScreen();
		switch(option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				oled.clearScreen();
				oled.displayMessage("Reading data...");
				uint32_t reply = uds->readNumberOfDTCs(tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout",1);
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					uint16_t numberOfDTCs=tmpBuffer[4];
					numberOfDTCs=(numberOfDTCs << 8) + tmpBuffer[5];
					char z[260];
					convert.itoa(numberOfDTCs,z,4);
					oled.displayMessage("DTCs found: ",1);
					oled.displayMessage(z,0,1);
					buttons.getButtonPressed();
				}
			}
			case 2:
			{
				oled.clearScreen();
				oled.displayMessage("Reading DTCs...");
				uint32_t reply = uds->readDTCs(tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout",1);
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					char z[260];
					for(uint32_t w = 3; w < reply; w = (w + 4))
					{
						uint32_t dtcCode=tmpBuffer[w];
						dtcCode = (dtcCode << 8) +  tmpBuffer[(w + 1)];
						dtcCode = (dtcCode << 8) +  tmpBuffer[(w + 2)];
						dtcCode = (dtcCode << 8) +  tmpBuffer[(w + 3)];
						memset(z,0,260);
						uds->checkDTC(dtcCode,z);
						oled.clearScreen();
						oled.displayMessage(z);
						if(buttons.getButtonPressed() != 1)//if any button but start is pressed, we assume user doesnt want to read the dtcs
						{
							break;
						}
					}
				}
				break;
			}
			case 3:
			{

				uint32_t reply = uds->clearDTCs(0xFFFFFF);//clear ALL DTCs
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					oled.clearScreen();
					oled.displayMessage("DTCs cleared");
					buttons.getButtonPressed();
					break;
				}
				break;
			}
		}
	}
	
}
				
				
void CANbadger::UDSCANMemoryMenu(UDSCANHandler *uds)
{
	const char* options[15]={"Set Address", "Read Memory", "Write Memory"};
	uint8_t option = 1;
	uint64_t memAddress=0;//address to read from or write to
	while(1)
	{
		option = oled.showOLEDMenu("UDS Memory Menu", options, 3, &buttons);
		oled.clearScreen();
		switch(option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				uint8_t tmpbyte=0;
				uint8_t tmplen=1;
				uint64_t tmpMemAddr=0;
				if(!getHexValue("Address length:",(uint64_t*)(&tmplen),1,5,1))
				{
					break;
				}
				for(uint8_t a = 0; a < tmplen; a++)//now we get the length
				{
					char tmpstrng[32]="Enter MSB";
					char tmpstrng2[6];
					convert.itox((a + 1),tmpstrng2,1);
					strcat(tmpstrng,tmpstrng2);
					strcat(tmpstrng,":");
					if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
					{
						break;
					}
					tmpMemAddr = (tmpMemAddr << 8) + tmpbyte;
				}
				oled.clearScreen();
				oled.displayMessage("Entered Address:");
				oled.displayMessage("0x",1);
				char z[22];
				convert.itox(tmpMemAddr,z,(tmplen * 2));
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				tmplen=buttons.getButtonPressed();
				if(tmplen == 1)//if user presses ok
				{
					memAddress = tmpMemAddr;
				}
				break;
			}
			case 2:
			{
				uint16_t readLen=0;
				if(!getDecValue("How many bytes?",(uint64_t*)(&readLen),1,4095,1))
				{
					break;
				}
				uint32_t reply = uds->readMemoryByAddress(memAddress, readLen, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					oled.clearScreen();
					if(readLen < 56)//if we can fit it in the screen
					{
						oled.displayMessage("REPLY: ");
						char toShow[128]={0};
						memset(toShow,0,128);
						for(uint8_t a = 0; a < readLen; a++)
						{
							char sequence[3]={0};
							convert.itox(tmpBuffer[(a+1)],sequence,2);
							strcat(toShow,sequence);
						}
						oled.displayMessage(toShow,0,1);
						buttons.getButtonPressed();
						oled.clearScreen();
					}
					oled.displayMessage("Saving to file..");
					uint16_t logNumber=1;//for sequential log writing
					bool itsDone=false;//to know when we found an available filename
					char tmpbuf[64]={0};
					while(!itsDone)
					{
						char filename2[90]="/MemDumps/MBA/";
						memset(tmpbuf,0,64);
						char sequence[22];//if we need more than 5 digits for a log number, i think its time to cleanup the SD
						if(remoteID < 0x10000)
						{
							convert.itox(remoteID,sequence,4);
						}
						else
						{
							convert.itox(remoteID,sequence,8);
						}
						strcat(filename2,sequence);
						strcat(filename2,"_");
						sprintf(tmpbuf,"%x_",memAddress);
						strcat (filename2,tmpbuf);
						convert.itox(logNumber,sequence,4);
						strcat(filename2,sequence);
						strcat(filename2,".BIN");
						if(!sd.doesFileExist(filename2))
						{
							itsDone=true;
						}
						else
						{
							logNumber++;
						}
						if(logNumber > 65535)
						{
							oled.displayMessage("Logs folder full",1);
							buttons.getButtonPressed();
							break;
						}
					}
					if(itsDone == true)
					{
						char filename2[90]="/MemDumps/MBA/";
						char sequence[22];//if we need more than 5 digits for a log number, i think its time to cleanup the SD
						if(remoteID < 0x1000)
						{
							convert.itox(remoteID,sequence,4);
						}
						else
						{
							convert.itox(remoteID,sequence,8);
						}
						strcat(filename2,sequence);
						strcat(filename2,"_");
						memset(tmpbuf,0,64);
						sprintf(tmpbuf,"%x_",memAddress);
						strcat (filename2,tmpbuf);
						convert.itox(logNumber,sequence,4);
						strcat(filename2,sequence);
						strcat(filename2,".BIN");
						if (!sd.openFile(filename2, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
						{
							oled.displayMessage("SD Write error",1);
						}
						else
						{
							for(uint8_t a = 0; a < readLen; a++)
							{
								tmpBuffer[a] = tmpBuffer[(a+1)];
							}
							sd.write((char*)tmpBuffer,readLen);
							sd.closeFile();
							oled.displayMessage("Dump saved to:",1);
							oled.displayMessage(filename2,1);
						}
						buttons.getButtonPressed();
					}
				}
				break;
			}
			case 3:
			{
				char tmpFileName[90]="/MemDumps/MBA";
				char filename[90];
				if(getFileName(tmpFileName))
				{
					memset(filename,0,64);//clean the previous one
					strcat(filename,"/MemDumps/MBA/");
					strcat(filename, tmpFileName);//place the new one
				}
				else
				{
					break;
				}
				uint32_t totalLen = sd.getFileSize(filename);
				if (!sd.openFile(filename, O_RDONLY))//if we couldnt open the file, then same thing
				{
					oled.displayMessage("SD Error");
					buttons.getButtonPressed();
					break;
				}
				oled.displayMessage("Write Address:");
				oled.displayMessage("0x",1);
				char z[22];
				convert.itox(memAddress,z,10);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() != 1)//if user doesnt press ok
				{
					break;
				}
				oled.clearScreen();
				oled.displayMessage("Requesting Write");
				memset(tmpBuffer,0,4096);
				uint32_t reply;
				if(totalLen < 4086)//make sure we dont read more than can be written
				{
					sd.read((char*)tmpBuffer, totalLen);
					reply = uds->writeMemoryByAddress(memAddress, totalLen, tmpBuffer);//we send the request
				}
				else
				{
					sd.read((char*)tmpBuffer, 4086);
					reply = uds->writeMemoryByAddress(memAddress, 4086, tmpBuffer);//we send the request
				}
				sd.closeFile();
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout",1);
					buttons.getButtonPressed();
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
				}
				else//but if it worked...
				{
					oled.displayMessage("Mem Write OK",1);
					buttons.getButtonPressed();
				}
				break;
			}
			default:
			{
				return;
			}
		}
	}
	
}


void CANbadger::KWP2KCANDiagMenu(KWP2KCANHandler *kwp, uint8_t interfaceno)
{
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	uint8_t addressType = 0;
	uint8_t addressByte = 0xF1;
	CANFormat formato;
	bool usePadding;
	uint8_t paddingByte;
	if(interfaceno == 1)//get the existing configuration
	{
		if(getCANBadgerStatus(CAN1_STANDARD) == true)
		{
			formato = CANStandard;
		}
		else
		{
			formato = CANExtended;
		}
		if(getCANBadgerStatus(CAN1_USE_FULLFRAME) == true)
		{
			usePadding = true;
			paddingByte = CAN1PaddingByte;
		}
		else
		{
			usePadding = false;
		}

	}
	else
	{
		if(getCANBadgerStatus(CAN2_STANDARD) == true)
		{
			formato = CANStandard;
		}
		else
		{
			formato = CANExtended;
		}
		if(getCANBadgerStatus(CAN2_USE_FULLFRAME) == true)
		{
			usePadding = true;
			paddingByte = CAN2PaddingByte;
		}
		else
		{
			usePadding = false;
		}
	}
	while(1)
	{
		oled.clearScreen();
		oled.displayMessage("KWP2K Diag Menu");//show the header
		oled.displayMessage(" tID: 0x",1);
		char z[22];
		convert.itox(localID,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" rID: 0x",1);
		convert.itox(remoteID,z,8);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Diag type: 0x",1);
		convert.itox(currentDiagSession,z,2);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Status: ",1);
		if(kwp->sessionStatus() == true)
		{
			oled.displayMessage("ONLINE",0,1);
		}
		else
		{
			oled.displayMessage("OFFLINE",0,1);
		}
		oled.displayMessage(" Format: ",1);//show if standard or extended format is being used
		if(formato == CANStandard)
		{
			oled.displayMessage("STD",0,1);
		}
		else
		{
			oled.displayMessage("XTD",0,1);
		}
		oled.displayMessage(" Padding: ",1);
		if(usePadding == true)
		{
			convert.itox(paddingByte,z,2);
		}
		else
		{
			z[0]='O';
			z[1]='F';
			z[2]='F';
			z[3]=0;
		}
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Addressing: ",1);
		if(addressType == 0)
		{
			z[0]='S';
			z[1]='T';
			z[2]='D';
			z[3]=0;
		}
		else
		{
			convert.itox(addressByte,z,2);
		}
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Start Session",1);
		oled.set_text_rc(chosenOption,0);//we set the cursor on current highlight
		oled.putc(arrow);
		bool optChanged=false;
		while(!optChanged)
		{
			uint8_t a = buttons.getButtonPressed();//check which button was pressed
			if(a == 1)
			{
				if(chosenOption == 1)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=0;
					uint32_t tmplocalID=0;
					if(formato == CANStandard)
					{
						tmplen=2;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					else
					{
						tmplen=4;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x1F,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					oled.clearScreen();
					oled.displayMessage("Entered ID:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						localID = tmplocalID;
						kwp->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					}
					optChanged=true;
				}
				else if(chosenOption == 2)
				{
					uint8_t tmpbyte=0;
					uint8_t tmplen=0;
					uint32_t tmplocalID=0;
					if(formato == CANStandard)
					{
						tmplen=2;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x7,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					else
					{
						tmplen=4;
						for(uint8_t a = 0; a < tmplen; a++)//now we get the length
						{
							char tmpstrng[32]="Enter MSB";
							char tmpstrng2[6];
							convert.itox((a + 1),tmpstrng2,1);
							strcat(tmpstrng,tmpstrng2);
							strcat(tmpstrng,":");
							if(a==0)
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0x1F,1))
								{
									break;
								}
							}
							else
							{
								if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
								{
									break;
								}
							}
							tmplocalID = (tmplocalID << 8) + tmpbyte;
						}
					}
					oled.clearScreen();
					oled.displayMessage("Entered RID:");
					oled.displayMessage("0x",1);
					char z[22];
					convert.itox(tmplocalID,z,(tmplen * 2));
					oled.displayMessage(z,0,1);
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					tmplen=buttons.getButtonPressed();
					if(tmplen == 1)//if user presses ok
					{
						remoteID = tmplocalID;
						kwp->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					}
					optChanged=true;
				}
				else if(chosenOption == 3)
				{
					getHexValue("Enter Session:",(uint64_t*)(&currentDiagSession),0,0xFF,1);
					optChanged=true;
				}
				else if(chosenOption == 4)
				{
					if(kwp->sessionStatus() == true)
					{
						kwp->setSessionStatus(false);
					}
					else
					{
						oled.clearScreen();
						oled.displayMessage("Start diagnostic",1);
						oled.displayMessage(" session to go  ",1);
						oled.displayMessage("     online     ",1);
						wait(2);
					}
					optChanged=true;
				}
				else if(chosenOption == 5)
				{
					if(interfaceno == 1)//if CAN1
					{
						if(getCANBadgerStatus(CAN1_STANDARD) == true)
						{
							setCANBadgerStatus(CAN1_STANDARD,0);
							setCANBadgerStatus(CAN1_EXTENDED,1);
							formato=CANExtended;
							kwp->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
						}
						else
						{
							setCANBadgerStatus(CAN1_STANDARD,1);
							setCANBadgerStatus(CAN1_EXTENDED,0);
							formato=CANStandard;
							kwp->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
						}
					}
					else
					{
						if(getCANBadgerStatus(CAN2_STANDARD) == true)
						{
							setCANBadgerStatus(CAN2_STANDARD,0);
							setCANBadgerStatus(CAN2_EXTENDED,1);
							formato=CANExtended;
							kwp->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
						}
						else
						{
							setCANBadgerStatus(CAN2_STANDARD,1);
							setCANBadgerStatus(CAN2_EXTENDED,0);
							formato=CANStandard;
							kwp->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
						}
					}
				}
				else if(chosenOption == 6)
				{
					if(usePadding)
					{
						usePadding = false;
						if(interfaceno == 1)
						{
							setCANBadgerStatus(CAN1_USE_FULLFRAME,0);
						}
						else
						{
							setCANBadgerStatus(CAN2_USE_FULLFRAME,0);
						}
						kwp->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					}
					else
					{
						usePadding = true;
						getHexValue("Enter padding:",(uint64_t*)(&paddingByte),0,0xFF,1);
						if(interfaceno == 1)
						{
							setCANBadgerStatus(CAN1_USE_FULLFRAME,1);
							CAN1PaddingByte	= paddingByte;
						}
						else
						{
							setCANBadgerStatus(CAN2_USE_FULLFRAME,1);
							CAN2PaddingByte	= paddingByte;
						}
						kwp->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);

					}
					optChanged=true;
				}

				else if(chosenOption == 7)
				{
					if(addressType == 0)
					{
						if(getHexValue("Enter Address:",(uint64_t*)(&addressByte),0,0xFF,1))
						{
							addressType = 1;
							optChanged=true;
						}
					}
					else
					{
						addressType = 0;
						optChanged=true;
					}
					kwp->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
				}
				else if(chosenOption == 8)
				{
					oled.clearScreen();
					oled.displayMessage("Connecting...");
					kwp->setTransmissionParameters(localID, remoteID, formato, usePadding, paddingByte, addressType);
					generalCounter1 = kwp->startComms(tmpBuffer, currentDiagSession);//we just use the counter to grab the code
					if(generalCounter1 == 0)
					{
						oled.displayMessage("Timeout",1);
						wait(2);
					}
					else if((generalCounter1 & 0xFFFF0000) != 0)
					{
						kwp->checkError((uint8_t)(generalCounter1 >> 16), tmpBuffer);
						displayError(tmpBuffer);
					}
					else
					{
						oled.displayMessage("Connected!",1);
						buttons.getButtonPressed();
					}
					optChanged=true;
				}
			}
			else if (a == 2)//One option down, the "+" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 8)//in order to not overwrite the header
				{
					b = 1;
				}
				else
				{
					b++;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 3)//One option up, the "-" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 1)//in order to not overwrite the header
				{
					b = 8;
				}
				else
				{
					b--;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 4)
			{
				return;
			}
		}
	}
}

void CANbadger::KWP2KCANDataMenu(KWP2KCANHandler *kwp)
{
	const char* options[15]={"Read by LID", "Write by LID", "Read by CID", "Write by CID", "Read ECUID"};
	uint8_t option = 1;
	uint8_t lID=0;//local ID
	uint16_t cID=0;//common id
	uint8_t ecuID=0;//ecu id
	while(1)
	{
		option = oled.showOLEDMenu("KWP2K Data Menu", options, 5, &buttons);
		oled.clearScreen();
		switch(option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				if(!getHexValue("Local ID:",(uint64_t*)(&lID),0,0xFF,1))
				{
					break;
				}
				uint32_t reply = kwp->readDataByLocalID(lID, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					kwp->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					uint16_t lenn = reply & 0xFFFF;
					if(lenn < 56)//if we can fit it in the screen
					{
						oled.clearScreen();
						oled.displayMessage("REPLY: ");
						char toShow[128]={0};
						for(uint8_t a = 3; a < lenn; a++)
						{
							char sequence[3]={0};
							convert.itox(tmpBuffer[a],sequence,2);
							strcat(toShow,sequence);
						}
						oled.displayMessage(toShow,0,1);
						buttons.getButtonPressed();
						oled.clearScreen();
					}
					char filename[90];
			    	memset(filename,0,90);
			    	strcat(filename, "/MemDumps/DID/KWP2K/LID/");
					char tmpstrg[30];
					memset(tmpstrg,0,30);
					sprintf(tmpstrg,"%x_ID_%x_",remoteID,lID);
					strcat(filename,tmpstrg);
			    	char fileExtension[6]=".BIN";
			    	if(!sd.getSequencialFileName(filename, fileExtension))
			    	{
			    		oled.clearScreen();
			    		oled.displayMessage(" Filename Error ");
			    		oled.displayMessage("****************",1);
			    		oled.displayMessage("  If limit of",1);
			    		oled.displayMessage(" 65535 files in",1);
			    		oled.displayMessage(" logs folder is",1);
			    		oled.displayMessage("  reached this",1);
			    		oled.displayMessage("  will happen",1);
			    		oled.displayMessage("****************",1);
			    		buttons.getButtonPressed();
			    		break;
			    	}
					if (!sd.openFile(filename, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
					{
						oled.displayMessage("SD Write error",1);
					}
					else
					{
						for(uint8_t a = 2; a < lenn; a++)
						{
							tmpBuffer[(a - 2)] = tmpBuffer[a];
						}
						lenn = (lenn - 2);
						sd.write((char*)tmpBuffer,lenn);
						sd.closeFile();
						oled.displayMessage("Dump saved to:");
						oled.displayMessage(filename,1);
						buttons.getButtonPressed();
					}
				}
				break;
			}
			case 2:
			{
				if(!getHexValue("Local ID:",(uint64_t*)(&lID),0,0xFF,1))
				{
					break;
				}
				char tmpFileName[90]="/MemDumps/DID/KWP2K/LID";
				char filename[90];
				if(getFileName(tmpFileName))
				{
					memset(filename,0,90);//clean the previous one
					strcat(filename,"/MemDumps/DID/KWP2K/LID/");
					strcat(filename, tmpFileName);//place the new one
				}
				else
				{
					break;
				}
				uint32_t totalLen = sd.getFileSize(filename);
				if (!sd.openFile(filename, O_RDONLY))//if we couldnt open the file, then same thing
				{
					oled.clearScreen();
					oled.displayMessage("SD read error");
					buttons.getButtonPressed();
					break;
				}
				memset(tmpBuffer,0,4096);
				uint32_t reply;
				oled.displayMessage("Writing data...");
				if(totalLen < 254)//make sure we dont read more than can be written
				{
					sd.read((char*)tmpBuffer, totalLen);
					reply = kwp->writeDataByLocalID(lID, tmpBuffer, totalLen, tmpBuffer);
				}
				else
				{
					sd.read((char*)tmpBuffer, 254);
					reply = kwp->writeDataByLocalID(lID, tmpBuffer, 254, tmpBuffer);//we send the request
				}
				sd.closeFile();
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout",1);
					buttons.getButtonPressed();
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					kwp->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
				}
				else//but if it worked...
				{
					oled.displayMessage("Data Write OK",1);
					buttons.getButtonPressed();
				}
				break;
			}
			case 3:
			{
				uint8_t tmpDataID=0;
				if(!getHexValue("Upper CID Byte:",(uint64_t*)(&tmpDataID),0,0xFF,1))
				{
					break;
				}
				cID = tmpDataID;
				cID = (cID << 8);
				if(!getHexValue("Lower CID Byte:",(uint64_t*)(&tmpDataID),0,0xFF,1))
				{
					break;
				}
				cID = (cID + tmpDataID);
				oled.displayMessage("CID: 0x");
				char z[22];
				convert.itox(cID,z,4);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				uint8_t u = buttons.getButtonPressed();
				if(u == 4)//if back button pressed
				{
					break;
				}
				uint32_t reply = kwp->readDataByCommonID(cID, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					kwp->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					uint16_t lenn = reply & 0xFFFF;
					if(lenn < 56)//if we can fit it in the screen
					{
						oled.clearScreen();
						oled.displayMessage("REPLY: ");
						char toShow[128]={0};
						for(uint8_t a = 3; a < lenn; a++)
						{
							char sequence[3]={0};
							convert.itox(tmpBuffer[a],sequence,2);
							strcat(toShow,sequence);
						}
						oled.displayMessage(toShow,0,1);
						buttons.getButtonPressed();
						oled.clearScreen();
					}
					char filename[90];
			    	memset(filename,0,90);
			    	strcat(filename, "/MemDumps/DID/KWP2K/CID/");
					char tmpstrg[30];
					memset(tmpstrg,0,30);
					sprintf(tmpstrg,"%x_ID_%x_",remoteID,cID);
					strcat(filename,tmpstrg);
			    	char fileExtension[6]=".BIN";
			    	if(!sd.getSequencialFileName(filename, fileExtension))
			    	{
			    		oled.clearScreen();
			    		oled.displayMessage(" Filename Error ");
			    		oled.displayMessage("****************",1);
			    		oled.displayMessage("  If limit of",1);
			    		oled.displayMessage(" 65535 files in",1);
			    		oled.displayMessage(" logs folder is",1);
			    		oled.displayMessage("  reached this",1);
			    		oled.displayMessage("  will happen",1);
			    		oled.displayMessage("****************",1);
			    		buttons.getButtonPressed();
			    		break;
			    	}
					if (!sd.openFile(filename, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
					{
						oled.displayMessage("SD Write error",1);
					}
					else
					{
						for(uint8_t a = 2; a < lenn; a++)
						{
							tmpBuffer[(a - 2)] = tmpBuffer[a];
						}
						lenn = (lenn - 2);
						sd.write((char*)tmpBuffer,lenn);
						sd.closeFile();
						oled.displayMessage("Dump saved to:");
						oled.displayMessage(filename,1);
						buttons.getButtonPressed();
					}
				}
				break;
			}
			case 4:
			{
				uint8_t tmpDataID=0;
				if(!getHexValue("Upper CID Byte:",(uint64_t*)(&tmpDataID),0,0xFF,1))
				{
					break;
				}
				cID = tmpDataID;
				cID = (cID << 8);
				if(!getHexValue("Lower CID Byte:",(uint64_t*)(&tmpDataID),0,0xFF,1))
				{
					break;
				}
				cID = (cID + tmpDataID);
				oled.displayMessage("CID: 0x");
				char z[22];
				convert.itox(cID,z,4);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				uint8_t u = buttons.getButtonPressed();
				if(u == 4)//if back button pressed
				{
					break;
				}
				char tmpFileName[90]="/MemDumps/DID/KWP2K/CID";
				char filename[90];
				if(getFileName(tmpFileName))
				{
					memset(filename,0,90);//clean the previous one
					strcat(filename,"/MemDumps/DID/KWP2K/CID/");
					strcat(filename, tmpFileName);//place the new one
				}
				else
				{
					break;
				}
				uint32_t totalLen = sd.getFileSize(filename);
				if (!sd.openFile(filename, O_RDONLY))//if we couldnt open the file, then same thing
				{
					oled.clearScreen();
					oled.displayMessage("SD read error");
					buttons.getButtonPressed();
					break;
				}
				memset(tmpBuffer,0,4096);
				uint32_t reply;
				oled.displayMessage("Writing data...");
				if(totalLen < 254)//make sure we dont read more than can be written
				{
					sd.read((char*)tmpBuffer, totalLen);
					reply = kwp->writeDataByCommonID(cID, tmpBuffer, totalLen, tmpBuffer);
				}
				else
				{
					sd.read((char*)tmpBuffer, 254);
					reply = kwp->writeDataByLocalID(cID, tmpBuffer, 254, tmpBuffer);//we send the request
				}
				sd.closeFile();
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout",1);
					buttons.getButtonPressed();
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					kwp->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
				}
				else//but if it worked...
				{
					oled.displayMessage("Data Write OK",1);
					buttons.getButtonPressed();
				}
				break;
			}
			case 5:
			{
				if(!getHexValue("ECU ID:",(uint64_t*)(&ecuID),0,0xFF,1))
				{
					break;
				}
				uint32_t reply = kwp->readECUID(ecuID, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					kwp->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					uint16_t lenn = reply & 0xFFFF;
					if(lenn < 56)//if we can fit it in the screen
					{
						oled.clearScreen();
						oled.displayMessage("REPLY: ");
						char toShow[128]={0};
						for(uint8_t a = 3; a < lenn; a++)
						{
							char sequence[3]={0};
							convert.itox(tmpBuffer[a],sequence,2);
							strcat(toShow,sequence);
						}
						oled.displayMessage(toShow,0,1);
						buttons.getButtonPressed();
						oled.clearScreen();
					}
					char filename[90];
			    	memset(filename,0,90);
			    	strcat(filename, "/MemDumps/DID/KWP2K/ECUID/");
					char tmpstrg[30];
					memset(tmpstrg,0,30);
					sprintf(tmpstrg,"%x_ID_%x_",remoteID,ecuID);
					strcat(filename,tmpstrg);
			    	char fileExtension[6]=".BIN";
			    	if(!sd.getSequencialFileName(filename, fileExtension))
			    	{
			    		oled.clearScreen();
			    		oled.displayMessage(" Filename Error ");
			    		oled.displayMessage("****************",1);
			    		oled.displayMessage("  If limit of",1);
			    		oled.displayMessage(" 65535 files in",1);
			    		oled.displayMessage(" logs folder is",1);
			    		oled.displayMessage("  reached this",1);
			    		oled.displayMessage("  will happen",1);
			    		oled.displayMessage("****************",1);
			    		buttons.getButtonPressed();
			    		break;
			    	}
					if (!sd.openFile(filename, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
					{
						oled.displayMessage("SD Write error",1);
					}
					else
					{
						for(uint8_t a = 2; a < lenn; a++)
						{
							tmpBuffer[(a - 2)] = tmpBuffer[a];
						}
						lenn = (lenn - 2);
						sd.write((char*)tmpBuffer,lenn);
						sd.closeFile();
						oled.displayMessage("Dump saved to:");
						oled.displayMessage(filename,1);
						buttons.getButtonPressed();
					}
				}
				break;
			}
			default:
			{
				return;
			}
		}
	}
}

void CANbadger::TP20DiagMenu(KWP2KTP20Handler *tp20)
{
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	uint32_t cID=0x200;//TP20 channel negotiation ID
	uint8_t ecuID=0x01;//ID for the target ECU
	currentDiagSession = 0x0;//set it to no diag session, which seems to be the most common case
	while(1)
	{
		oled.clearScreen();
		oled.displayMessage(" TP2 Diag Menu");//show the header
		oled.displayMessage(" cID: 0x",1);
		char z[22];
		convert.itox(cID,z,3);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" tID: 0x",1);
		convert.itox(ecuID,z,2);
		oled.displayMessage(z,0,1);
		oled.displayMessage(" Diag type: ",1);
		if(currentDiagSession != 0)
		{
			oled.displayMessage("0x",0,1);
			convert.itox(currentDiagSession,z,2);
			oled.displayMessage(z,0,1);
		}
		else
		{
			oled.displayMessage("NONE",0,1);
		}
		oled.displayMessage(" Status: ",1);
		if(tp20->sessionStatus() == true)
		{
			oled.displayMessage("ONLINE",0,1);
		}
		else
		{
			oled.displayMessage("OFFLINE",0,1);
		}
		oled.displayMessage(" GO!",1);
		oled.set_text_rc(chosenOption,0);//we set the cursor on current highlight
		oled.putc(arrow);
		bool optChanged=false;
		while(!optChanged)
		{
			uint8_t a = buttons.getButtonPressed();//check which button was pressed
			if(a == 1)
			{
				if(chosenOption == 1)
				{
					uint8_t tmpcID=0;
					uint8_t q = getHexValue("Enter LSB:",(uint64_t*)(&tmpcID),0,0xEF,1);
					if(q != 0)
					{
						cID = 0x200 + tmpcID;
					}
					optChanged=true;
				}
				else if(chosenOption == 2)
				{
					getHexValue("Enter Target ID:",(uint64_t*)(&ecuID),0,0xFF,1);
					optChanged=true;
				}
				else if(chosenOption == 3)
				{
					getHexValue("Enter Diag lvl:",(uint64_t*)(&currentDiagSession),0,0xFF,1);
					optChanged=true;
				}
				else if(chosenOption == 4)
				{
					if(tp20->sessionStatus() == true)
					{
						tp20->closeChannel();
					}
					else
					{
						oled.clearScreen();
						oled.displayMessage("Start diagnostic",1);
						oled.displayMessage(" session to go  ",1);
						oled.displayMessage("     online     ",1);
						buttons.getButtonPressed();
					}
					optChanged=true;
				}
				else if(chosenOption == 5)
				{
					oled.clearScreen();
					if (tp20->sessionStatus() == false || currentDiagSession == 0)
					{
						oled.displayMessage("Connecting...");
						if(tp20->sessionStatus() == true)//to be on the default session, we need to start from scratch
						{
							tp20->closeChannel();
						}
						uint32_t reply = tp20->channelSetup(cID, ecuID);
						if(reply == 0)
						{
							oled.displayMessage("Channel Timeout",1);
							buttons.getButtonPressed();
							optChanged=true;
							break;
						}
						else if(reply != 1)
						{
							oled.displayMessage("Channel Error",1);
							buttons.getButtonPressed();
							optChanged=true;
							break;
						}
						else
						{
							oled.displayMessage("Channel OK!",1);
							remoteID=ecuID;//just as a reference for logs
							if(currentDiagSession == 0)
							{
								oled.displayMessage("Session set ok!",1);
							}
						}
					}
					if(currentDiagSession != 0)//if the user wants to set a specific diag session
					{
						uint32_t reply = tp20->startComms(tmpBuffer,currentDiagSession);
						if(reply == 0)//if we have no reply
						{
							oled.displayMessage("Request Timeout",1);
							buttons.getButtonPressed();
							tp20->closeChannel();
							optChanged=true;
							break;
						}
						else if ((reply & 0xFFFF0000) != 0)//if there was an error
						{
							tp20->checkError((uint8_t)(reply >> 16), tmpBuffer);
							displayError(tmpBuffer);
							optChanged=true;
							break;
						}
						else
						{
							oled.displayMessage("Session set ok!",1);
						}
					}
					optChanged=true;
					buttons.getButtonPressed();
				}
			}
			else if (a == 2)//One option down, the "+" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 5)//in order to not overwrite the header
				{
					b = 1;
				}
				else
				{
					b++;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 3)//One option up, the "-" button
			{
				uint8_t b = oled.getCurrentRow();
				oled.set_text_rc(b,0);
				oled.putc(space);
				if(b == 1)//in order to not overwrite the header
				{
					b = 5;
				}
				else
				{
					b--;
				}
				oled.set_text_rc(b,0);
				oled.putc(arrow);
				chosenOption=b;
			}
			else if (a == 4)
			{
				return;
			}
		}
	}
}

void CANbadger::TP20DataMenu(KWP2KTP20Handler *tp20)
{
	const char* options[15]={"Read by LID", "Write by LID", "Read by CID", "Write by CID", "Read ECUID"};
	uint8_t option = 1;
	uint8_t lID=0;//local ID
	uint16_t cID=0;//common id
	uint8_t ecuID=0;//ecu id
	while(1)
	{
		option = oled.showOLEDMenu(" TP2 Data Menu", options, 5, &buttons);
		oled.clearScreen();
		switch(option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				if(!getHexValue("Local ID:",(uint64_t*)(&lID),0,0xFF,1))
				{
					break;
				}
				uint32_t reply = tp20->readDataByLocalID(lID, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					tp20->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					uint16_t lenn = reply & 0xFFFF;
					if(lenn < 56)//if we can fit it in the screen
					{
						oled.clearScreen();
						oled.displayMessage("REPLY: ");
						char toShow[128]={0};
						for(uint8_t a = 3; a < lenn; a++)
						{
							char sequence[3]={0};
							convert.itox(tmpBuffer[a],sequence,2);
							strcat(toShow,sequence);
						}
						oled.displayMessage(toShow,0,1);
						buttons.getButtonPressed();
						oled.clearScreen();
					}
					char filename[90];
			    	memset(filename,0,90);
			    	strcat(filename, "/MemDumps/DID/TP20/LID/");
					char tmpstrg[30];
					memset(tmpstrg,0,30);
					sprintf(tmpstrg,"%x_ID_%x_",remoteID,lID);
					strcat(filename,tmpstrg);
			    	char fileExtension[6]=".BIN";
			    	if(!sd.getSequencialFileName(filename, fileExtension))
			    	{
			    		oled.clearScreen();
			    		oled.displayMessage(" Filename Error ");
			    		oled.displayMessage("****************",1);
			    		oled.displayMessage("  If limit of",1);
			    		oled.displayMessage(" 65535 files in",1);
			    		oled.displayMessage(" logs folder is",1);
			    		oled.displayMessage("  reached this",1);
			    		oled.displayMessage("  will happen",1);
			    		oled.displayMessage("****************",1);
			    		buttons.getButtonPressed();
			    		break;
			    	}
					if (!sd.openFile(filename, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
					{
						oled.displayMessage("SD Write error",1);
					}
					else
					{
						for(uint8_t a = 2; a < lenn; a++)
						{
							tmpBuffer[(a - 2)] = tmpBuffer[a];
						}
						lenn = (lenn - 2);
						sd.write((char*)tmpBuffer,lenn);
						sd.closeFile();
						oled.displayMessage("Dump saved to:");
						oled.displayMessage(filename,1);
						buttons.getButtonPressed();
					}
				}
				break;
			}
			case 2:
			{
				if(!getHexValue("Local ID:",(uint64_t*)(&lID),0,0xFF,1))
				{
					break;
				}
				char tmpFileName[90]="/MemDumps/DID/TP20/LID";
				char filename[90];
				if(getFileName(tmpFileName))
				{
					memset(filename,0,90);//clean the previous one
					strcat(filename,"/MemDumps/DID/TP20/LID/");
					strcat(filename, tmpFileName);//place the new one
				}
				else
				{
					break;
				}
				uint32_t totalLen = sd.getFileSize(filename);
				if (!sd.openFile(filename, O_RDONLY))//if we couldnt open the file, then same thing
				{
					oled.clearScreen();
					oled.displayMessage("SD read error");
					buttons.getButtonPressed();
					break;
				}
				memset(tmpBuffer,0,4096);
				uint32_t reply;
				oled.displayMessage("Writing data...");
				if(totalLen < 254)//make sure we dont read more than can be written
				{
					sd.read((char*)tmpBuffer, totalLen);
					reply = tp20->writeDataByLocalID(lID, tmpBuffer, totalLen, tmpBuffer);
				}
				else
				{
					sd.read((char*)tmpBuffer, 254);
					reply = tp20->writeDataByLocalID(lID, tmpBuffer, 254, tmpBuffer);//we send the request
				}
				sd.closeFile();
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout",1);
					buttons.getButtonPressed();
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					tp20->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
				}
				else//but if it worked...
				{
					oled.displayMessage("Data Write OK",1);
					buttons.getButtonPressed();
				}
				break;
			}
			case 3:
			{
				uint8_t tmpDataID=0;
				if(!getHexValue("Upper CID Byte:",(uint64_t*)(&tmpDataID),0,0xFF,1))
				{
					break;
				}
				cID = tmpDataID;
				cID = (cID << 8);
				if(!getHexValue("Lower CID Byte:",(uint64_t*)(&tmpDataID),0,0xFF,1))
				{
					break;
				}
				cID = (cID + tmpDataID);
				oled.displayMessage("CID: 0x");
				char z[22];
				convert.itox(cID,z,4);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				uint8_t u = buttons.getButtonPressed();
				if(u == 4)//if back button pressed
				{
					break;
				}
				uint32_t reply = tp20->readDataByCommonID(cID, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					tp20->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					uint16_t lenn = reply & 0xFFFF;
					if(lenn < 56)//if we can fit it in the screen
					{
						oled.clearScreen();
						oled.displayMessage("REPLY: ");
						char toShow[128]={0};
						for(uint8_t a = 3; a < lenn; a++)
						{
							char sequence[3]={0};
							convert.itox(tmpBuffer[a],sequence,2);
							strcat(toShow,sequence);
						}
						oled.displayMessage(toShow,0,1);
						buttons.getButtonPressed();
						oled.clearScreen();
					}
					char filename[90];
			    	memset(filename,0,90);
			    	strcat(filename, "/MemDumps/DID/TP20/CID/");
					char tmpstrg[30];
					memset(tmpstrg,0,30);
					sprintf(tmpstrg,"%x_ID_%x_",remoteID,cID);
					strcat(filename,tmpstrg);
			    	char fileExtension[6]=".BIN";
			    	if(!sd.getSequencialFileName(filename, fileExtension))
			    	{
			    		oled.clearScreen();
			    		oled.displayMessage(" Filename Error ");
			    		oled.displayMessage("****************",1);
			    		oled.displayMessage("  If limit of",1);
			    		oled.displayMessage(" 65535 files in",1);
			    		oled.displayMessage(" logs folder is",1);
			    		oled.displayMessage("  reached this",1);
			    		oled.displayMessage("  will happen",1);
			    		oled.displayMessage("****************",1);
			    		buttons.getButtonPressed();
			    		break;
			    	}
					if (!sd.openFile(filename, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
					{
						oled.displayMessage("SD Write error",1);
					}
					else
					{
						for(uint8_t a = 2; a < lenn; a++)
						{
							tmpBuffer[(a - 2)] = tmpBuffer[a];
						}
						lenn = (lenn - 2);
						sd.write((char*)tmpBuffer,lenn);
						sd.closeFile();
						oled.displayMessage("Dump saved to:");
						oled.displayMessage(filename,1);
						buttons.getButtonPressed();
					}
				}
				break;
			}
			case 4:
			{
				uint8_t tmpDataID=0;
				if(!getHexValue("Upper CID Byte:",(uint64_t*)(&tmpDataID),0,0xFF,1))
				{
					break;
				}
				cID = tmpDataID;
				cID = (cID << 8);
				if(!getHexValue("Lower CID Byte:",(uint64_t*)(&tmpDataID),0,0xFF,1))
				{
					break;
				}
				cID = (cID + tmpDataID);
				oled.displayMessage("CID: 0x");
				char z[22];
				convert.itox(cID,z,4);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				uint8_t u = buttons.getButtonPressed();
				if(u == 4)//if back button pressed
				{
					break;
				}
				char tmpFileName[90]="/MemDumps/DID/TP20/CID";
				char filename[90];
				if(getFileName(tmpFileName))
				{
					memset(filename,0,90);//clean the previous one
					strcat(filename,"/MemDumps/DID/TP20/CID/");
					strcat(filename, tmpFileName);//place the new one
				}
				else
				{
					break;
				}
				uint32_t totalLen = sd.getFileSize(filename);
				if (!sd.openFile(filename, O_RDONLY))//if we couldnt open the file, then same thing
				{
					oled.clearScreen();
					oled.displayMessage("SD read error");
					buttons.getButtonPressed();
					break;
				}
				memset(tmpBuffer,0,4096);
				uint32_t reply;
				oled.displayMessage("Writing data...");
				if(totalLen < 254)//make sure we dont read more than can be written
				{
					sd.read((char*)tmpBuffer, totalLen);
					reply = tp20->writeDataByCommonID(cID, tmpBuffer, totalLen, tmpBuffer);
				}
				else
				{
					sd.read((char*)tmpBuffer, 254);
					reply = tp20->writeDataByLocalID(cID, tmpBuffer, 254, tmpBuffer);//we send the request
				}
				sd.closeFile();
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout",1);
					buttons.getButtonPressed();
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					tp20->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
				}
				else//but if it worked...
				{
					oled.displayMessage("Data Write OK",1);
					buttons.getButtonPressed();
				}
				break;
			}
			case 5:
			{
				if(!getHexValue("ECU ID:",(uint64_t*)(&ecuID),0,0xFF,1))
				{
					break;
				}
				uint32_t reply = tp20->readECUID(ecuID, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					tp20->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					uint16_t lenn = reply & 0xFFFF;
					if(lenn < 56)//if we can fit it in the screen
					{
						oled.clearScreen();
						oled.displayMessage("REPLY: ");
						char toShow[128]={0};
						for(uint8_t a = 3; a < lenn; a++)
						{
							char sequence[3]={0};
							convert.itox(tmpBuffer[a],sequence,2);
							strcat(toShow,sequence);
						}
						oled.displayMessage(toShow,0,1);
						buttons.getButtonPressed();
						oled.clearScreen();
					}
					char filename[90];
			    	memset(filename,0,90);
			    	strcat(filename, "/MemDumps/DID/TP20/ECUID/");
					char tmpstrg[30];
					memset(tmpstrg,0,30);
					sprintf(tmpstrg,"%x_ID_%x_",remoteID,ecuID);
					strcat(filename,tmpstrg);
			    	char fileExtension[6]=".BIN";
			    	if(!sd.getSequencialFileName(filename, fileExtension))
			    	{
			    		oled.clearScreen();
			    		oled.displayMessage(" Filename Error ");
			    		oled.displayMessage("****************",1);
			    		oled.displayMessage("  If limit of",1);
			    		oled.displayMessage(" 65535 files in",1);
			    		oled.displayMessage(" logs folder is",1);
			    		oled.displayMessage("  reached this",1);
			    		oled.displayMessage("  will happen",1);
			    		oled.displayMessage("****************",1);
			    		buttons.getButtonPressed();
			    		break;
			    	}
					if (!sd.openFile(filename, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
					{
						oled.displayMessage("SD Write error",1);
					}
					else
					{
						for(uint8_t a = 2; a < lenn; a++)
						{
							tmpBuffer[(a - 2)] = tmpBuffer[a];
						}
						lenn = (lenn - 2);
						sd.write((char*)tmpBuffer,lenn);
						sd.closeFile();
						oled.displayMessage("Dump saved to:");
						oled.displayMessage(filename,1);
						buttons.getButtonPressed();
					}
				}
				break;
			}
			default:
			{
				return;
			}
		}
	}
}


void CANbadger::doOLEDKWP2K(CAN *canbus, uint8_t interfaceNo, bool isInSession)
{
	KWP2KCANHandler kwp(canbus, interfaceNo);//need to pass it to the functions below
	if(isInSession == true)//only used by security hijack
	{
		kwp.attachToSession(localID,remoteID);
	}
	while(1)
	{
		oled.clearScreen();
		const char* options[15]={"Diag Session","R/W Data by ID", "DTC Information", "R/W Mem by Addr", "Request Upload"};
		uint8_t option = oled.showOLEDMenu((const char*)"KWP2K Menu", options, 6, &buttons);
		switch (option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				KWP2KCANDiagMenu(&kwp, interfaceNo);
				break;
			}
			case 2:
			{
				KWP2KCANDataMenu(&kwp);
				break;
			}
			case 3:
			{
				KWP2KCANDTCMenu(&kwp);
				break;
			}
			case 4:
			{
				KWP2KMemoryMenu(&kwp);
				break;
			}
			case 5:
			{
				KWP2KCANTransferMenu(&kwp);
				break;
			}
		}
	}
}

void CANbadger::TP20TransferMenu(KWP2KTP20Handler *tp20)
{
	const char* options[15]={"Set Address", "Set Data Format", "Upload from DUT"};
	uint8_t option = 1;
	uint64_t memAddress=0;//address to read from or write to
	uint8_t memLen = 1;//length of the address field
	uint32_t requestSize=0;//used to know how big the upload or download is
	uint8_t requestLen = 1;//length of the request size field
	uint8_t dataFormat=0;//used to store the data format
	while(1)
	{
		option = oled.showOLEDMenu("TP2 Upld Menu", options, 3, &buttons);
		oled.clearScreen();
		switch(option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				uint8_t tmpbyte=0;
				uint8_t tmplen=1;
				uint64_t tmpMemAddr=0;
				if(!getHexValue("Address length:",(uint64_t*)(&tmplen),1,5,1))
				{
					break;
				}
				for(uint8_t a = 0; a < tmplen; a++)//now we get the length
				{
					char tmpstrng[32]="Enter MSB";
					char tmpstrng2[6];
					convert.itox((a + 1),tmpstrng2,1);
					strcat(tmpstrng,tmpstrng2);
					strcat(tmpstrng,":");
					if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
					{
						break;
					}
					tmpMemAddr = (tmpMemAddr << 8) + tmpbyte;
				}
				oled.clearScreen();
				oled.displayMessage("Entered Address:");
				oled.displayMessage("0x",1);
				char z[22];
				convert.itox(tmpMemAddr,z,(tmplen * 2));
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				tmplen=buttons.getButtonPressed();
				if(tmplen == 1)//if user presses ok
				{
					memAddress = tmpMemAddr;
					memLen = tmplen;
				}
				break;
			}
			case 2:
			{
				oled.clearScreen();
				oled.displayMessage("****WARNING**** ");
				oled.displayMessage("  DO NOT CHANGE ",1);
				oled.displayMessage("THIS UNLESS YOU ",1);
				oled.displayMessage(" KNOW WHAT YOU",1);
				oled.displayMessage("   ARE DOING",1);
				oled.displayMessage("****WARNING**** ",1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() != 1)//if user doesnt press ok
				{
					break;
				}
				uint8_t tmpbyte=0;
				if(!getHexValue("Format Byte:",(uint64_t*)(&tmpbyte),0,0xFF,1))
				{
					break;
				}
				oled.clearScreen();
				oled.displayMessage("Entered Format:");
				oled.displayMessage("0x",1);
				char z[22];
				convert.itox(tmpbyte,z,2);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() == 1)//if user presses ok
				{
					dataFormat = tmpbyte;
				}
				break;
			}
			case 3:
			{
				uint8_t tmpbyte=0;
				uint32_t tmplen=requestLen;
				uint32_t tmpRequestSize=0;
				if(!getHexValue("Upload size:",(uint64_t*)(&tmplen),1,4,1))
				{
					break;
				}
				for(uint8_t a = 0; a < tmplen; a++)//now we get the length
				{
					char tmpstrng[32]="Enter MSB";
					char tmpstrng2[6];
					convert.itox((a + 1),tmpstrng2,1);
					strcat(tmpstrng,tmpstrng2);
					strcat(tmpstrng,":");
					if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
					{
						break;
					}
					tmpRequestSize = (tmpRequestSize << 8) + tmpbyte;
				}
				oled.clearScreen();
				char z[22];
				oled.displayMessage("Entered Address:",1);
				oled.displayMessage("0x",1);
				convert.itox(memAddress,z,10);
				oled.displayMessage(z,0,1);
				oled.displayMessage("Entered Size:",1);
				oled.displayMessage("0x",1);
				convert.itox(tmpRequestSize,z,(tmplen * 2));
				oled.displayMessage(z,0,1);
				oled.displayMessage("Format: 0x",1);
				convert.itox(dataFormat,z,2);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() != 1)//if user doesnt press ok
				{
					break;
				}
				requestSize = tmpRequestSize;
				requestLen = tmplen;
				uint32_t tmpAddr = memAddress;
				oled.clearScreen();
				oled.displayMessage("Request upload..");
				for(uint8_t x = memLen; x < 0; x--)
				{
					tmpBuffer[x - 1] = (uint8_t)tmpAddr;
					tmpAddr = (tmpAddr >> 8);
				}
				tmpBuffer[memLen] = dataFormat;
				for(uint8_t x = tmplen; x < 0; x--)
				{
					tmpBuffer[x + memLen + 1] = (uint8_t)tmpRequestSize;
					tmpRequestSize = (tmpRequestSize >> 8);
				}
				uint32_t reply = tp20->requestUpload(tmpBuffer, requestLen + memLen + 1, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout,1");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					tp20->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					oled.displayMessage("Create file..",1);
					uint16_t logNumber=1;//for sequential log writing
					bool itsDone=false;//to know when we found an available filename
					char tmpbuf[64]={0};
					while(!itsDone)
					{
						char filename2[90]="/Transfers/CAN/Uploads/";
						memset(tmpbuf,0,64);
						char sequence[22];//if we need more than 5 digits for a log number, i think its time to cleanup the SD
						if(remoteID < 0x10000)
						{
							convert.itox(remoteID,sequence,4);
						}
						else
						{
							convert.itox(remoteID,sequence,8);
						}
						strcat(filename2,sequence);
						strcat(filename2,"_");
						sprintf(tmpbuf,"%x_",memAddress);
						strcat (filename2,tmpbuf);
						convert.itox(logNumber,sequence,4);
						strcat(filename2,sequence);
						strcat(filename2,".BIN");
						if(!sd.doesFileExist(filename2))
						{
							itsDone=true;
						}
						else
						{
							logNumber++;
						}
						if(logNumber > 65535)
						{
							oled.displayMessage("Logs folder full",1);
							buttons.getButtonPressed();
							break;
						}
					}
					if(itsDone == true)
					{
						char filename2[90]="/Transfers/CAN/Uploads/";
						char sequence[22];//if we need more than 5 digits for a log number, i think its time to cleanup the SD
						if(remoteID < 0x1000)
						{
							convert.itox(remoteID,sequence,4);
						}
						else
						{
							convert.itox(remoteID,sequence,8);
						}
						strcat(filename2,sequence);
						strcat(filename2,"_");
						memset(tmpbuf,0,64);
						sprintf(tmpbuf,"%x_",memAddress);
						strcat (filename2,tmpbuf);
						convert.itox(logNumber,sequence,4);
						strcat(filename2,sequence);
						strcat(filename2,".BIN");
						uint32_t replySize = 0;
						if (!sd.openFile(filename2, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
						{
							oled.displayMessage("SD Write error",1);
						}
						else
						{
							oled.displayMessage("Transfering Data",1);
							for(uint32_t w=0; w < requestSize; w=(w+replySize))//loop for transfer data
							{
								reply = tp20->transferData(tmpBuffer, 0, tmpBuffer);//get the block
								if(reply == 0)//if we have no reply
								{
									oled.displayMessage("Timeout",1);
									sd.closeFile();
									buttons.getButtonPressed();
									break;
								}
								else if ((reply & 0xFFFF0000) != 0)//if there was an error
								{
									tp20->checkError((uint8_t)(reply >> 16), tmpBuffer);
									sd.closeFile();
									displayError(tmpBuffer);
									break;
								}
								sd.write((char*)tmpBuffer + 1,reply - 1);//write the reply to SD
							}
							sd.closeFile();
							oled.displayMessage("Dump saved to:",1);
							oled.displayMessage(filename2,1);
						}
						buttons.getButtonPressed();
					}
				}
				break;
			}
/*			case 4: //since it requires additional routines and it can brick systems, it is disabled
			{
				char tmpFileName[90]="/Transfers/CAN/Downloads";
				char filename[90];
				if(getFileName(tmpFileName))
				{
					memset(filename,0,64);//clean the previous one
					strcat(filename,"/Transfers/CAN/Downloads/");
					strcat(filename, tmpFileName);//place the new one
				}
				else
				{
					break;
				}
				uint32_t totalLen = sd.getFileSize(filename);
				if (!sd.openFile(filename, O_RDONLY))//if we couldnt open the file, then same thing
				{
					oled.displayMessage("SD Error");
					buttons.getButtonPressed();
					break;
				}
				oled.clearScreen();
				char z[22];
				oled.displayMessage("Entered Address:",1);
				oled.displayMessage("0x",1);
				convert.itox(memAddress,z,10);
				oled.displayMessage(z,0,1);
				oled.displayMessage("File Size:",1);
				oled.displayMessage("0x",1);
				convert.itox(totalLen,z,8);
				oled.displayMessage(z,0,1);
				oled.displayMessage("Format: 0x",1);
				convert.itox(dataFormat,z,2);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() != 1)//if user doesnt press ok
				{
					break;
				}
				oled.clearScreen();
				oled.displayMessage("Request Download");
				uint32_t reply = tp20->requestUpload(dataFormat, memAddress, requestSize, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout,1");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					uint32_t blockSize=(uds->getBlockSize(tmpBuffer) - 2);//substract 2 for the SID and block counter
					uint8_t blockCounter=1;//we start with 1, and let it overflow if it happens
					oled.displayMessage("Transfer Data...",1);
					bool isComplete=false;
					uint32_t reply=0;
					while(!isComplete)
					{
						uint32_t aa = sd.read((char*)tmpBuffer, blockSize);
						if(aa != blockSize)//if we have reached the end of the file
						{
							isComplete=true;
						}
						if(aa != 0)//corner case, but could happen
						{
							reply = uds->transferData(blockCounter, tmpBuffer, aa, tmpBuffer);
							if(reply == 0)//if we have no reply
							{
								oled.displayMessage("Timeout,1");
								buttons.getButtonPressed();
								break;
							}
							else if ((reply & 0xFFFF0000) != 0)//if there was an error
							{
								uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
								displayError(tmpBuffer);
								break;
							}
						}
						blockCounter++;
					}
					reply = uds->requestTransferExit(tmpBuffer, 0, tmpBuffer);
					sd.closeFile();
					if(isComplete)//if download was successful
					{
						oled.displayMessage("Download OK!",1);
						buttons.getButtonPressed();
					}
					break;
				}
				break;
			}*/
			default:
			{
				return;
			}
		}
	}
}


void CANbadger::KWP2KCANTransferMenu(KWP2KCANHandler *kwp)
{
	const char* options[15]={"Set Address", "Set Data Format", "Upload from DUT"};
	uint8_t option = 1;
	uint64_t memAddress=0;//address to read from or write to
	uint8_t memLen = 1;//length of the address field
	uint32_t requestSize=0;//used to know how big the upload or download is
	uint8_t requestLen = 1;//length of the request size field
	uint8_t dataFormat=0;//used to store the data format
	while(1)
	{
		option = oled.showOLEDMenu("KWP2K Upld Menu", options, 3, &buttons);
		oled.clearScreen();
		switch(option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				uint8_t tmpbyte=0;
				uint8_t tmplen=1;
				uint64_t tmpMemAddr=0;
				if(!getHexValue("Address length:",(uint64_t*)(&tmplen),1,5,1))
				{
					break;
				}
				for(uint8_t a = 0; a < tmplen; a++)//now we get the length
				{
					char tmpstrng[32]="Enter MSB";
					char tmpstrng2[6];
					convert.itox((a + 1),tmpstrng2,1);
					strcat(tmpstrng,tmpstrng2);
					strcat(tmpstrng,":");
					if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
					{
						break;
					}
					tmpMemAddr = (tmpMemAddr << 8) + tmpbyte;
				}
				oled.clearScreen();
				oled.displayMessage("Entered Address:");
				oled.displayMessage("0x",1);
				char z[22];
				convert.itox(tmpMemAddr,z,(tmplen * 2));
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				tmplen=buttons.getButtonPressed();
				if(tmplen == 1)//if user presses ok
				{
					memAddress = tmpMemAddr;
					memLen = tmplen;
				}
				break;
			}
			case 2:
			{
				oled.clearScreen();
				oled.displayMessage("****WARNING**** ");
				oled.displayMessage("  DO NOT CHANGE ",1);
				oled.displayMessage("THIS UNLESS YOU ",1);
				oled.displayMessage(" KNOW WHAT YOU",1);
				oled.displayMessage("   ARE DOING",1);
				oled.displayMessage("****WARNING**** ",1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() != 1)//if user doesnt press ok
				{
					break;
				}
				uint8_t tmpbyte=0;
				if(!getHexValue("Format Byte:",(uint64_t*)(&tmpbyte),0,0xFF,1))
				{
					break;
				}
				oled.clearScreen();
				oled.displayMessage("Entered Format:");
				oled.displayMessage("0x",1);
				char z[22];
				convert.itox(tmpbyte,z,2);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() == 1)//if user presses ok
				{
					dataFormat = tmpbyte;
				}
				break;
			}
			case 3:
			{
				uint8_t tmpbyte=0;
				uint32_t tmplen=requestLen;
				uint32_t tmpRequestSize=0;
				if(!getHexValue("Upload size:",(uint64_t*)(&tmplen),1,4,1))
				{
					break;
				}
				for(uint8_t a = 0; a < tmplen; a++)//now we get the length
				{
					char tmpstrng[32]="Enter MSB";
					char tmpstrng2[6];
					convert.itox((a + 1),tmpstrng2,1);
					strcat(tmpstrng,tmpstrng2);
					strcat(tmpstrng,":");
					if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
					{
						break;
					}
					tmpRequestSize = (tmpRequestSize << 8) + tmpbyte;
				}
				oled.clearScreen();
				char z[22];
				oled.displayMessage("Entered Address:",1);
				oled.displayMessage("0x",1);
				convert.itox(memAddress,z,10);
				oled.displayMessage(z,0,1);
				oled.displayMessage("Entered Size:",1);
				oled.displayMessage("0x",1);
				convert.itox(tmpRequestSize,z,(tmplen * 2));
				oled.displayMessage(z,0,1);
				oled.displayMessage("Format: 0x",1);
				convert.itox(dataFormat,z,2);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() != 1)//if user doesnt press ok
				{
					break;
				}
				requestSize = tmpRequestSize;
				requestLen = tmplen;
				uint32_t tmpAddr = memAddress;
				oled.clearScreen();
				oled.displayMessage("Request upload..");
				for(uint8_t x = memLen; x < 0; x--)
				{
					tmpBuffer[x - 1] = (uint8_t)tmpAddr;
					tmpAddr = (tmpAddr >> 8);
				}
				tmpBuffer[memLen] = dataFormat;
				for(uint8_t x = tmplen; x < 0; x--)
				{
					tmpBuffer[x + memLen + 1] = (uint8_t)tmpRequestSize;
					tmpRequestSize = (tmpRequestSize >> 8);
				}
				uint32_t reply = kwp->requestUpload(tmpBuffer, requestLen + memLen + 1, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout,1");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					kwp->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					oled.displayMessage("Create file..",1);
					uint16_t logNumber=1;//for sequential log writing
					bool itsDone=false;//to know when we found an available filename
					char tmpbuf[64]={0};
					while(!itsDone)
					{
						char filename2[90]="/Transfers/CAN/Uploads/";
						memset(tmpbuf,0,64);
						char sequence[22];//if we need more than 5 digits for a log number, i think its time to cleanup the SD
						if(remoteID < 0x10000)
						{
							convert.itox(remoteID,sequence,4);
						}
						else
						{
							convert.itox(remoteID,sequence,8);
						}
						strcat(filename2,sequence);
						strcat(filename2,"_");
						sprintf(tmpbuf,"%x_",memAddress);
						strcat (filename2,tmpbuf);
						convert.itox(logNumber,sequence,4);
						strcat(filename2,sequence);
						strcat(filename2,".BIN");
						if(!sd.doesFileExist(filename2))
						{
							itsDone=true;
						}
						else
						{
							logNumber++;
						}
						if(logNumber > 65535)
						{
							oled.displayMessage("Logs folder full",1);
							buttons.getButtonPressed();
							break;
						}
					}
					if(itsDone == true)
					{
						char filename2[90]="/Transfers/CAN/Uploads/";
						char sequence[22];//if we need more than 5 digits for a log number, i think its time to cleanup the SD
						if(remoteID < 0x1000)
						{
							convert.itox(remoteID,sequence,4);
						}
						else
						{
							convert.itox(remoteID,sequence,8);
						}
						strcat(filename2,sequence);
						strcat(filename2,"_");
						memset(tmpbuf,0,64);
						sprintf(tmpbuf,"%x_",memAddress);
						strcat (filename2,tmpbuf);
						convert.itox(logNumber,sequence,4);
						strcat(filename2,sequence);
						strcat(filename2,".BIN");
						uint32_t replySize = 0;
						if (!sd.openFile(filename2, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
						{
							oled.displayMessage("SD Write error",1);
						}
						else
						{
							oled.displayMessage("Transfering Data",1);
							for(uint32_t w=0; w < requestSize; w=(w+replySize))//loop for transfer data
							{
								reply = kwp->transferData(tmpBuffer, 0, tmpBuffer);//get the block
								if(reply == 0)//if we have no reply
								{
									oled.displayMessage("Timeout",1);
									sd.closeFile();
									buttons.getButtonPressed();
									break;
								}
								else if ((reply & 0xFFFF0000) != 0)//if there was an error
								{
									kwp->checkError((uint8_t)(reply >> 16), tmpBuffer);
									sd.closeFile();
									displayError(tmpBuffer);
									break;
								}
								sd.write((char*)tmpBuffer + 1,reply - 1);//write the reply to SD
							}
							sd.closeFile();
							oled.displayMessage("Dump saved to:",1);
							oled.displayMessage(filename2,1);
						}
						buttons.getButtonPressed();
					}
				}
				break;
			}
/*			case 4: //since it requires additional routines and it can brick systems, it is disabled
			{
				char tmpFileName[90]="/Transfers/CAN/Downloads";
				char filename[90];
				if(getFileName(tmpFileName))
				{
					memset(filename,0,64);//clean the previous one
					strcat(filename,"/Transfers/CAN/Downloads/");
					strcat(filename, tmpFileName);//place the new one
				}
				else
				{
					break;
				}
				uint32_t totalLen = sd.getFileSize(filename);
				if (!sd.openFile(filename, O_RDONLY))//if we couldnt open the file, then same thing
				{
					oled.displayMessage("SD Error");
					buttons.getButtonPressed();
					break;
				}
				oled.clearScreen();
				char z[22];
				oled.displayMessage("Entered Address:",1);
				oled.displayMessage("0x",1);
				convert.itox(memAddress,z,10);
				oled.displayMessage(z,0,1);
				oled.displayMessage("File Size:",1);
				oled.displayMessage("0x",1);
				convert.itox(totalLen,z,8);
				oled.displayMessage(z,0,1);
				oled.displayMessage("Format: 0x",1);
				convert.itox(dataFormat,z,2);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() != 1)//if user doesnt press ok
				{
					break;
				}
				oled.clearScreen();
				oled.displayMessage("Request Download");
				uint32_t reply = uds->requestUpload(dataFormat, memAddress, requestSize, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout,1");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					uint32_t blockSize=(uds->getBlockSize(tmpBuffer) - 2);//substract 2 for the SID and block counter
					uint8_t blockCounter=1;//we start with 1, and let it overflow if it happens
					oled.displayMessage("Transfer Data...",1);
					bool isComplete=false;
					uint32_t reply=0;
					while(!isComplete)
					{
						uint32_t aa = sd.read((char*)tmpBuffer, blockSize);
						if(aa != blockSize)//if we have reached the end of the file
						{
							isComplete=true;
						}
						if(aa != 0)//corner case, but could happen
						{
							reply = uds->transferData(blockCounter, tmpBuffer, aa, tmpBuffer);
							if(reply == 0)//if we have no reply
							{
								oled.displayMessage("Timeout,1");
								buttons.getButtonPressed();
								break;
							}
							else if ((reply & 0xFFFF0000) != 0)//if there was an error
							{
								uds->checkError((uint8_t)(reply >> 16), tmpBuffer);
								displayError(tmpBuffer);
								break;
							}
						}
						blockCounter++;
					}
					reply = uds->requestTransferExit(tmpBuffer, 0, tmpBuffer);
					sd.closeFile();
					if(isComplete)//if download was successful
					{
						oled.displayMessage("Download OK!",1);
						buttons.getButtonPressed();
					}
					break;
				}
				break;
			}*/
			default:
			{
				return;
			}
		}
	}
}



void CANbadger::doOLEDTP20(CAN *canbus, uint8_t interfaceNo, bool isInSession, uint8_t tpCounter)
{
	KWP2KTP20Handler tp20(canbus, interfaceNo);//need to pass it to the functions below
	if(isInSession == true)//only used by security hijack
	{
		tp20.attachToSession(localID,remoteID, tpCounter);
	}
	while(1)
	{
		oled.clearScreen();
		const char* options[15]={"Diag Session","R/W Data by ID", "DTC Information", "R/W Mem by Addr", "Security Access", "Request Upload"};
		uint8_t option = oled.showOLEDMenu((const char*)"TP2.0 Menu", options, 6, &buttons);
		switch (option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				TP20DiagMenu(&tp20);
				break;
			}
			case 2:
			{
				TP20DataMenu(&tp20);
				break;
			}
			case 3:
			{
				TP20DTCMenu(&tp20);
				break;
			}
			case 4:
			{
				TP20MemoryMenu(&tp20);
				break;
			}
			case 5:
			{
				TP20SecAccessMenu(&tp20);
				break;
			}
			case 6:
			{
				TP20TransferMenu(&tp20);
				break;
			}
			default:
			{
				return;
			}
		}
	}
}

void CANbadger::TP20MemoryMenu(KWP2KTP20Handler *tp20)
{
	const char* options[15]={"Set Address", "Read Memory", "Write Memory"};
	uint8_t option = 1;
	uint64_t memAddress=0;//address to read from or write to
	while(1)
	{
		option = oled.showOLEDMenu("TP2 Memory Menu", options, 3, &buttons);
		oled.clearScreen();
		switch(option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				uint8_t tmpbyte=0;
				uint8_t tmplen=1;
				uint64_t tmpMemAddr=0;
				if(!getHexValue("Address length:",(uint64_t*)(&tmplen),1,5,1))
				{
					break;
				}
				for(uint8_t a = 0; a < tmplen; a++)//now we get the length
				{
					char tmpstrng[32]="Enter MSB";
					char tmpstrng2[6];
					convert.itox((a + 1),tmpstrng2,1);
					strcat(tmpstrng,tmpstrng2);
					strcat(tmpstrng,":");
					if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
					{
						break;
					}
					tmpMemAddr = (tmpMemAddr << 8) + tmpbyte;
				}
				oled.clearScreen();
				oled.displayMessage("Entered Address:");
				oled.displayMessage("0x",1);
				char z[22];
				convert.itox(tmpMemAddr,z,(tmplen * 2));
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				tmplen=buttons.getButtonPressed();
				if(tmplen == 1)//if user presses ok
				{
					memAddress = tmpMemAddr;
				}
				break;
			}
			case 2:
			{
				uint16_t readLen=0;
				if(!getDecValue("How many bytes?",(uint64_t*)(&readLen),1,4095,1))
				{
					break;
				}
				uint32_t reply = tp20->readMemoryByAddress(memAddress, readLen, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					tp20->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					oled.clearScreen();
					if(readLen < 56)//if we can fit it in the screen
					{
						oled.displayMessage("REPLY: ");
						char toShow[128]={0};
						memset(toShow,0,128);
						for(uint8_t a = 0; a < readLen; a++)
						{
							char sequence[3]={0};
							convert.itox(tmpBuffer[(a+1)],sequence,2);
							strcat(toShow,sequence);
						}
						oled.displayMessage(toShow,0,1);
						buttons.getButtonPressed();
						oled.clearScreen();
					}
					oled.displayMessage("Saving to file..");
					uint16_t logNumber=1;//for sequential log writing
					bool itsDone=false;//to know when we found an available filename
					char tmpbuf[64]={0};
					while(!itsDone)
					{
						char filename2[90]="/MemDumps/MBA/";
						memset(tmpbuf,0,64);
						char sequence[22];//if we need more than 5 digits for a log number, i think its time to cleanup the SD
						if(remoteID < 0x10000)
						{
							convert.itox(remoteID,sequence,4);
						}
						else
						{
							convert.itox(remoteID,sequence,8);
						}
						strcat(filename2,sequence);
						strcat(filename2,"_");
						sprintf(tmpbuf,"%x_",memAddress);
						strcat (filename2,tmpbuf);
						convert.itox(logNumber,sequence,4);
						strcat(filename2,sequence);
						strcat(filename2,".BIN");
						if(!sd.doesFileExist(filename2))
						{
							itsDone=true;
						}
						else
						{
							logNumber++;
						}
						if(logNumber > 65535)
						{
							oled.displayMessage("Logs folder full",1);
							buttons.getButtonPressed();
							break;
						}
					}
					if(itsDone == true)
					{
						char filename2[90]="/MemDumps/MBA/";
						char sequence[22];//if we need more than 5 digits for a log number, i think its time to cleanup the SD
						if(remoteID < 0x1000)
						{
							convert.itox(remoteID,sequence,4);
						}
						else
						{
							convert.itox(remoteID,sequence,8);
						}
						strcat(filename2,sequence);
						strcat(filename2,"_");
						memset(tmpbuf,0,64);
						sprintf(tmpbuf,"%x_",memAddress);
						strcat (filename2,tmpbuf);
						convert.itox(logNumber,sequence,4);
						strcat(filename2,sequence);
						strcat(filename2,".BIN");
						if (!sd.openFile(filename2, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
						{
							oled.displayMessage("SD Write error",1);
						}
						else
						{
							for(uint8_t a = 0; a < readLen; a++)
							{
								tmpBuffer[a] = tmpBuffer[(a+1)];
							}
							sd.write((char*)tmpBuffer,readLen);
							sd.closeFile();
							oled.displayMessage("Dump saved to:",1);
							oled.displayMessage(filename2,1);
						}
						buttons.getButtonPressed();
					}
				}
				break;
			}
			case 3:
			{
				char tmpFileName[90]="/MemDumps/MBA";
				char filename[90];
				if(getFileName(tmpFileName))
				{
					memset(filename,0,64);//clean the previous one
					strcat(filename,"/MemDumps/MBA/");
					strcat(filename, tmpFileName);//place the new one
				}
				else
				{
					break;
				}
				uint32_t totalLen = sd.getFileSize(filename);
				if (!sd.openFile(filename, O_RDONLY))//if we couldnt open the file, then same thing
				{
					oled.displayMessage("SD Error");
					buttons.getButtonPressed();
					break;
				}
				oled.displayMessage("Write Address:");
				oled.displayMessage("0x",1);
				char z[22];
				convert.itox(memAddress,z,10);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() != 1)//if user doesnt press ok
				{
					break;
				}
				oled.clearScreen();
				oled.displayMessage("Requesting Write");
				memset(tmpBuffer,0,4096);
				uint32_t reply;
				sd.read((char*)tmpBuffer, totalLen);
				sd.closeFile();
				reply = tp20->writeMemoryByAddress(tmpBuffer, memAddress, totalLen, tmpBuffer);//we send the request
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout",1);
					buttons.getButtonPressed();
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					tp20->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
				}
				else//but if it worked...
				{
					oled.displayMessage("Mem Write OK",1);
					buttons.getButtonPressed();
				}
				break;
			}
			default:
			{
				return;
			}
		}
	}

}

void CANbadger::KWP2KMemoryMenu(KWP2KCANHandler *kwp)
{
	const char* options[15]={"Set Address", "Read Memory", "Write Memory"};
	uint8_t option = 1;
	uint64_t memAddress=0;//address to read from or write to
	while(1)
	{
		option = oled.showOLEDMenu("KWP2K Mem Menu", options, 3, &buttons);
		oled.clearScreen();
		switch(option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				uint8_t tmpbyte=0;
				uint8_t tmplen=1;
				uint64_t tmpMemAddr=0;
				if(!getHexValue("Address length:",(uint64_t*)(&tmplen),1,5,1))
				{
					break;
				}
				for(uint8_t a = 0; a < tmplen; a++)//now we get the length
				{
					char tmpstrng[32]="Enter MSB";
					char tmpstrng2[6];
					convert.itox((a + 1),tmpstrng2,1);
					strcat(tmpstrng,tmpstrng2);
					strcat(tmpstrng,":");
					if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
					{
						break;
					}
					tmpMemAddr = (tmpMemAddr << 8) + tmpbyte;
				}
				oled.clearScreen();
				oled.displayMessage("Entered Address:");
				oled.displayMessage("0x",1);
				char z[22];
				convert.itox(tmpMemAddr,z,(tmplen * 2));
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				tmplen=buttons.getButtonPressed();
				if(tmplen == 1)//if user presses ok
				{
					memAddress = tmpMemAddr;
				}
				break;
			}
			case 2:
			{
				uint16_t readLen=0;
				if(!getDecValue("How many bytes?",(uint64_t*)(&readLen),1,4095,1))
				{
					break;
				}
				uint32_t reply = kwp->readMemoryByAddress(memAddress, readLen, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					kwp->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					oled.clearScreen();
					if(readLen < 56)//if we can fit it in the screen
					{
						oled.displayMessage("REPLY: ");
						char toShow[128]={0};
						memset(toShow,0,128);
						for(uint8_t a = 0; a < readLen; a++)
						{
							char sequence[3]={0};
							convert.itox(tmpBuffer[(a+1)],sequence,2);
							strcat(toShow,sequence);
						}
						oled.displayMessage(toShow,0,1);
						buttons.getButtonPressed();
						oled.clearScreen();
					}
					oled.displayMessage("Saving to file..");
					uint16_t logNumber=1;//for sequential log writing
					bool itsDone=false;//to know when we found an available filename
					char tmpbuf[64]={0};
					while(!itsDone)
					{
						char filename2[90]="/MemDumps/MBA/";
						memset(tmpbuf,0,64);
						char sequence[22];//if we need more than 5 digits for a log number, i think its time to cleanup the SD
						if(remoteID < 0x10000)
						{
							convert.itox(remoteID,sequence,4);
						}
						else
						{
							convert.itox(remoteID,sequence,8);
						}
						strcat(filename2,sequence);
						strcat(filename2,"_");
						sprintf(tmpbuf,"%x_",memAddress);
						strcat (filename2,tmpbuf);
						convert.itox(logNumber,sequence,4);
						strcat(filename2,sequence);
						strcat(filename2,".BIN");
						if(!sd.doesFileExist(filename2))
						{
							itsDone=true;
						}
						else
						{
							logNumber++;
						}
						if(logNumber > 65535)
						{
							oled.displayMessage("Logs folder full",1);
							buttons.getButtonPressed();
							break;
						}
					}
					if(itsDone == true)
					{
						char filename2[90]="/MemDumps/MBA/";
						char sequence[22];//if we need more than 5 digits for a log number, i think its time to cleanup the SD
						if(remoteID < 0x1000)
						{
							convert.itox(remoteID,sequence,4);
						}
						else
						{
							convert.itox(remoteID,sequence,8);
						}
						strcat(filename2,sequence);
						strcat(filename2,"_");
						memset(tmpbuf,0,64);
						sprintf(tmpbuf,"%x_",memAddress);
						strcat (filename2,tmpbuf);
						convert.itox(logNumber,sequence,4);
						strcat(filename2,sequence);
						strcat(filename2,".BIN");
						if (!sd.openFile(filename2, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
						{
							oled.displayMessage("SD Write error",1);
						}
						else
						{
							for(uint8_t a = 0; a < readLen; a++)
							{
								tmpBuffer[a] = tmpBuffer[(a+1)];
							}
							sd.write((char*)tmpBuffer,readLen);
							sd.closeFile();
							oled.displayMessage("Dump saved to:",1);
							oled.displayMessage(filename2,1);
						}
						buttons.getButtonPressed();
					}
				}
				break;
			}
			case 3:
			{
				char tmpFileName[90]="/MemDumps/MBA";
				char filename[90];
				if(getFileName(tmpFileName))
				{
					memset(filename,0,64);//clean the previous one
					strcat(filename,"/MemDumps/MBA/");
					strcat(filename, tmpFileName);//place the new one
				}
				else
				{
					break;
				}
				uint32_t totalLen = sd.getFileSize(filename);
				if (!sd.openFile(filename, O_RDONLY))//if we couldnt open the file, then same thing
				{
					oled.displayMessage("SD Error");
					buttons.getButtonPressed();
					break;
				}
				oled.displayMessage("Write Address:");
				oled.displayMessage("0x",1);
				char z[22];
				convert.itox(memAddress,z,10);
				oled.displayMessage(z,0,1);
				oled.displayMessage(" ",1);
				oled.displayMessage("Confirm?",1);
				if(buttons.getButtonPressed() != 1)//if user doesnt press ok
				{
					break;
				}
				oled.clearScreen();
				oled.displayMessage("Requesting Write");
				memset(tmpBuffer,0,4096);
				uint32_t reply;
				sd.read((char*)tmpBuffer, totalLen);
				sd.closeFile();
				reply = kwp->writeMemoryByAddress(tmpBuffer, memAddress, totalLen, tmpBuffer);//we send the request
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout",1);
					buttons.getButtonPressed();
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					kwp->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
				}
				else//but if it worked...
				{
					oled.displayMessage("Mem Write OK",1);
					buttons.getButtonPressed();
				}
				break;
			}
			default:
			{
				return;
			}
		}
	}

}

void CANbadger::KWP2KCANDTCMenu(KWP2KCANHandler *kwp)
{
	const char* options[15]={"Enter params","Read DTC number", "Read DTCs", "Clear ALL DTCs"};
	uint8_t params[8];//used to store the param sent for dtc operations
	memset(params,0,8);
	uint8_t paramLen=0;
	uint8_t option = 1;
	while(1)
	{
		option = oled.showOLEDMenu(" KWP2K DTC Menu", options, 4, &buttons);
		oled.clearScreen();
		switch(option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				uint8_t tmplen = paramLen;
				if(!getHexValue("Enter param len:",(uint64_t*)(&tmplen),0,8,1))
				{
					break;
				}
				if(tmplen == 0)
				{
					oled.clearScreen();
					oled.displayMessage("No params");
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					generalCounter1=buttons.getButtonPressed();
					if(generalCounter1 == 1)//if user presses ok
					{
						paramLen=(uint8_t)tmplen;
					}
					break;
				}
				uint8_t tmparray[8];
				for(uint8_t a = 0; a < tmplen; a++)//now we get the length
				{
					uint8_t tmpbyte = params[a];
					char tmpstrng[32]="Enter Byte ";
					char tmpstrng2[32];
					convert.itox((a + 1),tmpstrng2,1);
					strcat(tmpstrng,tmpstrng2);
					strcat(tmpstrng,":");
					if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
					{
						break;
					}
					tmparray[a]=tmpbyte;
				}
				oled.clearScreen();
				oled.displayMessage("Entered Params:");
				oled.displayMessage("",1);
				char z[22];
				for(uint8_t e=0; e < tmplen; e++)
				{
					memset(z,0,22);
					uint8_t u=tmparray[e];
					convert.itox(u,z,2);
					oled.displayMessage(z,0,1);
				}
				oled.displayMessage("",1);
				oled.displayMessage("Confirm?",1);
				generalCounter1=buttons.getButtonPressed();
				if(generalCounter1 == 1)//if user presses ok
				{
					for(uint8_t e=0; e < tmplen; e++)
					{
						params[e]=tmparray[e];
					}
					paramLen=tmplen;
				}
				break;
			}
			case 2:
			{
				oled.clearScreen();
				oled.displayMessage("Reading DTCs...");
				for(uint8_t a = 0; a < paramLen; a++)//copy the params so they dont get overwritten
				{
					tmpBuffer[a] = params[a];
				}
				uint32_t reply = kwp->readDTCs(tmpBuffer, paramLen, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout",1);
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					kwp->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					uint8_t numberOfDTCs=kwp->getDTCList(tmpBuffer, tmpBuffer);
					char z[260];
					convert.itoa(numberOfDTCs,z,2);
					oled.displayMessage("DTCs found: ",1);
					oled.displayMessage(z,0,1);
					buttons.getButtonPressed();
				}
				break;
			}
			case 3:
			{
				oled.clearScreen();
				oled.displayMessage("Reading DTCs...");
				for(uint8_t a = 0; a < paramLen; a++)//copy the params so they dont get overwritten
				{
					tmpBuffer[a] = params[a];
				}
				uint32_t reply = kwp->readDTCs(tmpBuffer, paramLen, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout",1);
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					kwp->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					uint8_t numberOfDTCs=kwp->getDTCList(tmpBuffer, tmpBuffer);
					char z[260];
					for(uint32_t w = 0; w < (numberOfDTCs * 3); w = (w + 3))
					{
						uint16_t dtcCode=tmpBuffer[w];
						dtcCode = (dtcCode << 8) +  tmpBuffer[(w + 1)];
						memset(z,0,260);
						uint8_t statusByte=tmpBuffer[(w + 2)];
						kwp->checkDTC(dtcCode,statusByte,z);
						oled.clearScreen();
						oled.displayMessage(z);
						if(buttons.getButtonPressed() != 1)//if any button but start is pressed, we assume user doesnt want to read the dtcs
						{
							break;
						}
					}
				}
				break;
			}
			case 4:
			{

				uint32_t reply = kwp->clearDTCs(params, paramLen, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					kwp->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					oled.clearScreen();
					oled.displayMessage("DTCs cleared");
					buttons.getButtonPressed();
					break;
				}
				break;
			}
		}
	}

}



void CANbadger::TP20DTCMenu(KWP2KTP20Handler *tp20)
{
	const char* options[15]={"Enter params","Read DTC number", "Read DTCs", "Clear ALL DTCs"};
	uint8_t params[8];//used to store the param sent for dtc operations
	memset(params,0,8);
	uint8_t paramLen=0;
	uint8_t option = 1;
	while(1)
	{
		option = oled.showOLEDMenu(" TP2 DTC Menu", options, 4, &buttons);
		oled.clearScreen();
		switch(option)
		{
			case 0:
			{
				return;
			}
			case 1:
			{
				uint8_t tmplen = paramLen;
				if(!getHexValue("Enter param len:",(uint64_t*)(&tmplen),0,8,1))
				{
					break;
				}
				if(tmplen == 0)
				{
					oled.clearScreen();
					oled.displayMessage("No params");
					oled.displayMessage(" ",1);
					oled.displayMessage("Confirm?",1);
					generalCounter1=buttons.getButtonPressed();
					if(generalCounter1 == 1)//if user presses ok
					{
						paramLen=(uint8_t)tmplen;
					}
					break;
				}
				uint8_t tmparray[8];
				for(uint8_t a = 0; a < tmplen; a++)//now we get the length
				{
					uint8_t tmpbyte = params[a];
					char tmpstrng[32]="Enter Byte ";
					char tmpstrng2[32];
					convert.itox((a + 1),tmpstrng2,1);
					strcat(tmpstrng,tmpstrng2);
					strcat(tmpstrng,":");
					if(!getHexValue(tmpstrng,(uint64_t*)(&tmpbyte),0,0xFF,1))
					{
						break;
					}
					tmparray[a]=tmpbyte;
				}
				oled.clearScreen();
				oled.displayMessage("Entered Params:");
				oled.displayMessage("",1);
				char z[22];
				for(uint8_t e=0; e < tmplen; e++)
				{
					memset(z,0,22);
					uint8_t u=tmparray[e];
					convert.itox(u,z,2);
					oled.displayMessage(z,0,1);
				}
				oled.displayMessage("",1);
				oled.displayMessage("Confirm?",1);
				generalCounter1=buttons.getButtonPressed();
				if(generalCounter1 == 1)//if user presses ok
				{
					for(uint8_t e=0; e < tmplen; e++)
					{
						params[e]=tmparray[e];
					}
					paramLen=tmplen;
				}
				break;
			}
			case 2:
			{
				oled.clearScreen();
				oled.displayMessage("Reading DTCs...");
				for(uint8_t a = 0; a < paramLen; a++)//copy the params so they dont get overwritten
				{
					tmpBuffer[a] = params[a];
				}
				uint32_t reply = tp20->readDTCs(tmpBuffer, paramLen, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout",1);
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					tp20->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					uint8_t numberOfDTCs=tp20->getDTCList(tmpBuffer, tmpBuffer);
					char z[260];
					convert.itoa(numberOfDTCs,z,2);
					oled.displayMessage("DTCs found: ",1);
					oled.displayMessage(z,0,1);
					buttons.getButtonPressed();
				}
				break;
			}
			case 3:
			{
				oled.clearScreen();
				oled.displayMessage("Reading DTCs...");
				for(uint8_t a = 0; a < paramLen; a++)//copy the params so they dont get overwritten
				{
					tmpBuffer[a] = params[a];
				}
				uint32_t reply = tp20->readDTCs(tmpBuffer, paramLen, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.displayMessage("Timeout",1);
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					tp20->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					uint8_t numberOfDTCs=tp20->getDTCList(tmpBuffer, tmpBuffer);
					char z[260];
					for(uint32_t w = 0; w < (numberOfDTCs * 3); w = (w + 3))
					{
						uint16_t dtcCode=tmpBuffer[w];
						dtcCode = (dtcCode << 8) +  tmpBuffer[(w + 1)];
						memset(z,0,260);
						uint8_t statusByte=tmpBuffer[(w + 2)];
						tp20->checkDTC(dtcCode,statusByte,z);
						oled.clearScreen();
						oled.displayMessage(z);
						if(buttons.getButtonPressed() != 1)//if any button but start is pressed, we assume user doesnt want to read the dtcs
						{
							break;
						}
					}
				}
				break;
			}
			case 4:
			{

				uint32_t reply = tp20->clearDTCs(params, paramLen, tmpBuffer);
				if(reply == 0)//if we have no reply
				{
					oled.clearScreen();
					oled.displayMessage("Timeout");
					buttons.getButtonPressed();
					break;
				}
				else if ((reply & 0xFFFF0000) != 0)//if there was an error
				{
					tp20->checkError((uint8_t)(reply >> 16), tmpBuffer);
					displayError(tmpBuffer);
					break;
				}
				else//but if it worked...
				{
					oled.clearScreen();
					oled.displayMessage("DTCs cleared");
					buttons.getButtonPressed();
					break;
				}
				break;
			}
		}
	}
	
}


bool CANbadger::startUDSSession(uint8_t interface)
{
	if((getCANBadgerStatus(CAN1_INT_ENABLED) == 1 && interface == 1) || (getCANBadgerStatus(CAN2_INT_ENABLED) == 1 && interface == 2) || getCANBadgerStatus(CAN1_TO_CAN2_BRIDGE) == 1 || getCANBadgerStatus(CAN1_TO_CAN2_BRIDGE) == 1) //need to make sure nothing that would interfere is enabled
	{
		oled.clearScreen();
		oled.displayMessage("Disable BR/INT");
		buttons.getButtonPressed();
		return false;
	}
	if(interface == 1 || interface == 2)//oled is 1 for CAN1 and 2 for CAN2
	{
		if(interface == 1)
		{
			doOLEDUDS(&can1,1);
		}
		else
		{
			doOLEDUDS(&can2,2);
		}
	}
	else if (interface == 3 || interface == 4)//serial/k-line
	{
		
	}
	else if (interface == 5 || interface == 6)//ethernet
	{
		
	}
	else
	{
		return 0;
	}
	return 1;
}
		
bool CANbadger::startTP20Session(uint8_t interface)
{
	if((getCANBadgerStatus(CAN1_INT_ENABLED) == 1 && interface == 1) || (getCANBadgerStatus(CAN2_INT_ENABLED) == 1 && interface == 2) || getCANBadgerStatus(CAN1_TO_CAN2_BRIDGE) == 1 || getCANBadgerStatus(CAN1_TO_CAN2_BRIDGE) == 1) //need to make sure nothing that would interfere is enabled
	{
		return false;
	}
	if(interface == 1 || interface == 2)//oled is 1 for CAN1 and 2 for CAN2
	{
		if(interface == 1)
		{
			doOLEDTP20(&can1,1);
		}
		else
		{
			doOLEDTP20(&can2,2);
		}
	}
	else if (interface == 3 || interface == 4)//serial
	{
		
	}
	else if (interface == 5 || interface == 6)//ethernet
	{
		
	}
	else
	{
		return 0;
	}
	return 1;
}






char* CANbadger::getEthernetIPAddress()
{
	return eth.getIPAddress();
}

bool CANbadger::DHCPEthernetConnect()
{
	eth.init();
	if (eth.connect() != 0)
	{
		return false;
	}
	return true;
}

bool CANbadger::manualEthernetConnect(const char *ip, const char *mask, const char *gateway)
{
	eth.init(ip, mask, gateway);
	if (eth.connect() != 0)
	{
		return false;
	}
	return true;	
}

char* CANbadger::getEthernetMACAddress()
{
	return eth.getMACAddress();
}

char* CANbadger::getEthernetGateway()
{
	return eth.getGateway();
}

char* CANbadger::getEthernetNetworkMask()
{
	return eth.getNetworkMask();
}

void CANbadger::ethernetDisconnect()
{
	eth.disconnect();
}


void CANbadger::setLED(uint8_t color)
{
	if(color == LED_OFF )//turn it off
	{
		GLED=0;
		RLED=0;
	}
	else if(color == LED_RED)//Red
	{
		GLED=0;
		RLED=1;
	}
	else if(color == LED_GREEN)//Green
	{
		GLED=1;
		RLED=0;
	}
	else if(color == LED_ORANGE)//Orange
	{
		GLED=1;
		RLED=1;
	}
}

void CANbadger::blinkLED(uint8_t times, uint8_t color)
{
	// remember LED state and restore it once we're done
	int r_state = RLED.read();
	int g_state = GLED.read();
	for(uint8_t a=0;a<times;a++)
	{
		setLED(LED_OFF);
		wait(0.2);
		setLED(color);
		wait(0.2);
	}
	RLED = r_state;
	GLED = g_state;
}

void CANbadger::dumpSerial()
{
    wait(0.001);
    while(device.readable())
    {
        uint8_t trash=device.getc();
        wait(0.001);//wait for next char
    }//get the first char, trash the rest
}


uint32_t CANbadger::detectCANSpeed(uint8_t busno)
{
	uint32_t a;


	if(busno == 1) {
		CANbadger_CAN can(&can1);
		a=can.detectSpeed();
	} else {
		CANbadger_CAN can(&can2);
		a=can.detectSpeed();
	}
	if(a == 0) {canbadger_settings->setSpeed(busno, 500000); return 0;}
	canbadger_settings->setSpeed(busno, a);
	return a;
} 


bool CANbadger::setCANSpeed(uint8_t bus, uint32_t speed)
{
	if(bus == 1)
	{
		if(!can1.frequency(speed)) {return 0;}
		//canbadger_settings->setSpeed(1, speed);
	}
	else
	{
		if(!can2.frequency(speed)) {return 0;}
		canbadger_settings->setSpeed(2, speed);
	}
	return 1;
}

void CANbadger::checkSPISpeed(uint8_t memType)
{
	uint32_t desired_speed = 10000000;
	if(memType == 1)//RAM
	{
		desired_speed = 20000000;
	}
	if(canbadger_settings->getSpeed(0) != desired_speed)
	{
		memory.frequency(desired_speed);
		canbadger_settings->setSpeed(0, desired_speed);
	}
}

void CANbadger::clearRAM()
{
	checkSPISpeed(1);
	ram.clearRAM();
}

bool CANbadger::writeRAM(uint32_t startAdr, uint32_t len, const uint8_t* data)
{
	checkSPISpeed(1);
	return ram.write(startAdr,len,data);
}

bool CANbadger::readRAM(uint32_t address, uint32_t len, uint8_t* data)
{
	checkSPISpeed(1);
	return ram.read(address,len,data);
}


void CANbadger::clearEEPROM()
{
	eeprom.clearMem();
}

bool CANbadger::writeEEPROM(unsigned int startAdr, unsigned int len, const uint8_t* data)
{
	// write the value, update the checksum
	if(eeprom.write(startAdr, len, data)) {
		// recalcuate checksum
		uint8_t eeprom_data[EEPROM_CS_OFFS];
		eeprom.read(0, EEPROM_CS_OFFS, eeprom_data);
		uint32_t new_checksum = crc_32(eeprom_data, EEPROM_CS_OFFS);
		// always store msb first
		uint8_t new_checksum_packed[] = {
				(new_checksum >> 24) & 0xff,
				(new_checksum >> 16) & 0xff,
				(new_checksum >> 8) & 0xff,
				new_checksum & 0xff
		};
		// finally, write the updated checksum
		bool cs_write = eeprom.write(EEPROM_CS_OFFS, 4, new_checksum_packed);
		return cs_write && checkEEPROM();
	}
	return false;
}

bool CANbadger::readEEPROM(unsigned int startAdr, unsigned int len, uint8_t* data)
{
	// we first verify the eeprom's checksum
	// if it doesn't match, we always return 0xFF (eeprom is empty)
	if(!this->checkEEPROM()) {
		memset(data, 0xff, len);
		return false;
	}
	return eeprom.read(startAdr, len, data);
}

bool CANbadger::checkEEPROM() {
	// returns true if eeprom data is valid
	// returns false otherwise
	// first read the whole EEP and calcuate the data's checksum
	// if the stored checksum matches the data's checksum, we can return true
	uint32_t stored_checksum;
	uint8_t eeprom_data[EEPROM_CS_OFFS + 4];


	eeprom.read(0, EEPROM_CS_OFFS + 4, eeprom_data);
	stored_checksum = (eeprom_data[EEPROM_CS_OFFS] << 24) + (eeprom_data[EEPROM_CS_OFFS + 1] << 16) + (eeprom_data[EEPROM_CS_OFFS + 2] << 8) + eeprom_data[EEPROM_CS_OFFS + 3];
	uint32_t data_checksum = crc_32(eeprom_data, EEPROM_CS_OFFS);
	if(stored_checksum == data_checksum) {
		return true;
	}
	return false;
}

void CANbadger::readEEPROMUID(uint8_t* data)
{
	eeprom.read_UID(data);
}	

void CANbadger::setKLINESpeed(uint8_t bus, uint32_t speed)
{
	switch(bus) {
	case 1:
		kline1.baud(speed);
		break;
	case 2:
		kline2.baud(speed);
	}
	canbadger_settings->setSpeed(bus + 2, speed);
}
	
	

void CANbadger::doCANBridge(void)
{
	CANMessage canMsg(0,CANAny);//we create a message for any kind of ID
	if(getCANBadgerStatus(CAN1_TO_CAN2_BRIDGE) || getCANBadgerStatus(CAN1_LOGGING))//if CAN1 to CAN2 bridge is enabled or we want to log it
	{
		while(can1.read(canMsg) != 0)
		{
			if(getCANBadgerStatus(CAN1_LOGGING))
			{
				uint8_t tmpbuf[22]={0};//to store the info
				uint32_t us=timer.read_ms();//get the current time
				tmpbuf[1]=(us >> 24);//store it 
				tmpbuf[2]=(us >> 16);
				tmpbuf[3]=(us >> 8);
				tmpbuf[4]=us;
				uint32_t temp_speed = canbadger_settings->getSpeed(1);
				tmpbuf[9]=(temp_speed >> 24);//get the current speed and store it
				tmpbuf[10]=(temp_speed >> 16);
				tmpbuf[11]=(temp_speed >> 8);
				tmpbuf[12]=temp_speed;
				if(getCANBadgerStatus(CAN1_STANDARD))
				{
					tmpbuf[0]=21; //Bus 1, CAN, Standard frame
				}
				else
				{
					tmpbuf[0]=37; //Bus 1, CAN, Extended frame
				}
				tmpbuf[5]=(canMsg.id >> 24);//get the ID
				tmpbuf[6]=(canMsg.id >> 16);
				tmpbuf[7]=(canMsg.id >> 8);
				tmpbuf[8]=canMsg.id;
				tmpbuf[13]=canMsg.len;//get the data length
				for(uint8_t a=0; a<tmpbuf[13];a++)//and get the data
				{
					tmpbuf[(a+14)]=canMsg.data[a];
				}
				if((generalCounter1 + (tmpbuf[13] + 14)) > 4096)//if we would overflow
				{
					uint8_t tmp[22]={0};//we will write zeros so the parser knows to skip those bytes
					memset(tmp,0,22);
					writeTmpBuffer(generalCounter1,(4096 - generalCounter1),tmp);
					generalCounter1=0;
				}
				writeTmpBuffer(generalCounter1, (tmpbuf[13] + 14), tmpbuf);
				generalCounter1 = (generalCounter1 + (tmpbuf[13] + 14));//we are using generalCounter1 as the Array pointer
				generalCounter2++;//and we are using generalCounter2 to indicate that we have pushed a new frame to the buffer
			}
			if(getCANBadgerStatus(CAN1_TO_CAN2_BRIDGE))//if bridge is enabled
			{
				if(getCANBadgerStatus(CAN2_USE_FULLFRAME))
				{
					for(uint8_t a=canMsg.len; a< 8; a++)
					{
						canMsg.data[a] = 0;
					}
					canMsg.len=8;
				}
				if(getCANBadgerStatus(CAN1_STANDARD))//If CAN1 is set as standard, then we use the standard message
				{
					if(getCANBadgerStatus(CAN2_STANDARD))//then, we check if CAN2 is set to Standard or Extended mode to do the bridge
					{
						canMsg.format=CANStandard;
					}
					else if (getCANBadgerStatus(CAN2_EXTENDED))
					{
						if(canMsg.id < 0x800)
						{
							canMsg.format=CANStandard;
						}
						else
						{
							canMsg.format=CANExtended;
						}
					}
				}
				else if(getCANBadgerStatus(CAN1_EXTENDED))
				{
					if(getCANBadgerStatus(CAN1_TO_CAN2_BRIDGE))//if bridge is enabled
					{
						if(getCANBadgerStatus(CAN2_STANDARD))//then, we check if CAN2 is set to Standard or Extended mode to do the bridge
						{
							if(canMsg.id <= 0x7FF)//we can only send an extended ID in Standard CAN if its under 0x800
							{
								canMsg.format=CANStandard;
							}
						}
						else
						{
							if(canMsg.id < 0x800)
							{
								canMsg.format=CANStandard;
							}
							else
							{
								canMsg.format=CANExtended;
							}
						}
					}
				}
				while((LPC_CAN2->GSR & 4) == 0){}
				while(can2.write(canMsg) == 0){}
				canMsg.format=CANAny;
			}
			wait(0.0003);
		}
	}
	if(getCANBadgerStatus(CAN2_TO_CAN1_BRIDGE) || getCANBadgerStatus(CAN2_LOGGING))//if CAN2 to CAN1 bridge is enabled or we want to log it
	{
		canMsg.id=0;//reset it in case it was used by CAN1
		while(can2.read(canMsg)!= 0)
		{
			if(getCANBadgerStatus(CAN2_LOGGING))
			{
				uint8_t tmpbuf[22]={0};//to store the info
				uint32_t us=timer.read_ms();//get the current time
				tmpbuf[1]=(us >> 24);//store it 
				tmpbuf[2]=(us >> 16);
				tmpbuf[3]=(us >> 8);
				tmpbuf[4]=us;
				uint32_t temp_speed = canbadger_settings->getSpeed(2);
				tmpbuf[9]=(temp_speed >> 24);//get the current speed and store it
				tmpbuf[10]=(temp_speed >> 16);
				tmpbuf[11]=(temp_speed >> 8);
				tmpbuf[12]=temp_speed;
				if(getCANBadgerStatus(CAN2_STANDARD))
				{
					tmpbuf[0]=22; //Bus 2, CAN, Standard frame
				}
				else
				{
					tmpbuf[0]=38; //Bus 2, CAN, Extended frame
				}
				tmpbuf[5]=(canMsg.id >> 24);//get the ID
				tmpbuf[6]=(canMsg.id >> 16);
				tmpbuf[7]=(canMsg.id >> 8);
				tmpbuf[8]=canMsg.id;
				tmpbuf[13]=canMsg.len;//get the data length
				for(uint8_t a=0; a<tmpbuf[13];a++)//and get the data
				{
					tmpbuf[(a+14)]=canMsg.data[a];
				}
				if((generalCounter1 + (tmpbuf[13] + 14)) > 4096)//if we would overflow
				{
					uint8_t tmp[22]={0};//we will write zeros so the parser knows to skip those bytes
					memset(tmp,0,22);
					writeTmpBuffer(generalCounter1,(4096 - generalCounter1),tmp);
					generalCounter1=0;
				}
				writeTmpBuffer(generalCounter1, (tmpbuf[13] + 14), tmpbuf);
				generalCounter1 = (generalCounter1 + (tmpbuf[13] + 14));//we are using generalCounter1 as the RAM pointer
				generalCounter2++;//and we are using generalCounter2 to indicate that we have pushed a new frame to the RAM
			}
			if(getCANBadgerStatus(CAN2_TO_CAN1_BRIDGE))//if bridge is enabled
			{
				if(getCANBadgerStatus(CAN1_USE_FULLFRAME))
				{
					for(uint8_t a=canMsg.len; a< 8; a++)
					{
						canMsg.data[a] = 0;
					}
					canMsg.len=8;
				}
				if(getCANBadgerStatus(CAN2_STANDARD))//If CAN2 is set as standard, then we use the standard message
				{
					if(getCANBadgerStatus(CAN1_STANDARD))//then, we check if CAN1 is set to Standard or Extended mode to do the bridge
					{
							canMsg.format=CANStandard;
					}
					else if (getCANBadgerStatus(CAN1_EXTENDED))
					{
						if(canMsg.id < 0x800)
						{
							canMsg.format=CANStandard;
						}
						else
						{
							canMsg.format=CANExtended;
						}
					}
				}
				else if(getCANBadgerStatus(CAN2_EXTENDED))
				{
					if(getCANBadgerStatus(CAN1_STANDARD))//then, we check if CAN1 is set to Standard or Extended mode to do the bridge
					{
						if(canMsg.id <= 0x7FF)//we can only send an extended ID in Standard CAN if its under 0x800
						{
							canMsg.format=CANStandard;
						}
					}
					else
					{
						if(canMsg.id < 0x800)
						{
							canMsg.format=CANStandard;
						}
						else
						{
							canMsg.format=CANExtended;
						}
					}
				}
				while((LPC_CAN1->GSR & 4) == 0){}
				while(can1.write(canMsg) == 0){}
				canMsg.format=CANAny;
			}
			wait(0.0003);
		}
	}
}


bool CANbadger::dumpCANUDSTransfer(uint32_t targetID, uint8_t bus, uint8_t dumpFormat, uint32_t timeout)
{
	if(isSDInserted == 0)
	{
		return false;//we cant dump without an SD!
	}
	uint16_t logNumber=0;//for sequential log writing
	bool itsDone=false;//to know when we found an available filename
	char extnsion[5]=".bin";
	uint32_t totalBytes=0;
	while(!itsDone)
	{
		char filename2[70]="/Transfers/CAN/";
		char idNum[17];
		char nderscr[2]="_";
		convert.itox(targetID,idNum);
		strcat(filename2,idNum);
		strcat(filename2,nderscr);
		char sequence[17];//if we need more than 16 hex digits for a dump number, i think its time to cleanup the SD
		convert.itox(logNumber,sequence,4);
		strcat(filename2,sequence);
		strcat(filename2,extnsion);
		if(!sd.doesFileExist(filename2))
		{
			itsDone=true;
		}
		else
		{
			logNumber++;
		}
		if(logNumber > 65535)
		{
			return false;//i think that 65535 dumps with the same name are enough
		}
	}
	char filename2[70]="/Transfers/CAN/";
	char idNum[17];
	char nderscr[2]="_";
	convert.itox(targetID,idNum);
	strcat(filename2,idNum);
	strcat(filename2,nderscr);
	char sequence[17];//if we need more than 16 hex digits for a dump number, i think its time to cleanup the SD
	convert.itox(logNumber,sequence,4);
	strcat(filename2,sequence);
	strcat(filename2,extnsion);
  if (!sd.openFile(filename2, O_WRONLY | O_CREAT | O_TRUNC))//if we cant create a file, return
  {
     return false;
	}
//	enableCANBridge(3);//enable bridge on both interfaces
	if(bus == 1)//check on which interfaces to log. bitwise encoded
	{
		setCANBadgerStatus(CAN1_LOGGING,1);
	}
	else
	{
		setCANBadgerStatus(CAN2_LOGGING,1);
	}	
	generalCounter1=0;//we initialize the buffer pointer for the interrupts and the pending packets
	generalCounter2=0;
	generalCounter3=0;//used as our buffer Pointer
	generalCounter4=0;//used as the data array pointer
	uint8_t data[4094];//we will store the retrieved data from buffer here. 4094 is the maximum length we will ever get
	uint8_t tmpData[22];
	uint32_t transferLen=0;//to store the transfer length
	bool isTransferring=false;//for each transfer request
	bool downloadStarted=false;//to indicate if a download was detected
	uint32_t cTimeout=0;//current timeout. Used to determine when a transfer is done and close the file
	itsDone=false;
	CANBridge(true);//start the bridge
	while(buttons.isButtonPressed(4) == false && itsDone == false)//wait while the back button is not pressed or we didnt dump the file
	{
		if(generalCounter2 > 0)//if we have pending data to retrieve from XRAM
		{
			readTmpBuffer(generalCounter3, 1, tmpData);
			while(tmpData[0] == 0)//to skip bytes intentionally left blank or if there was a read attempt that was interrupted
			{
				generalCounter3++;//we position the counter on the next byte
				if(generalCounter3 == 4096)//if we have reached the end of the buffer space
				{
					generalCounter3 = 0;//reset it to make it circular!
				}
				readTmpBuffer(generalCounter3, 1, tmpData);
			}
			readTmpBuffer((generalCounter3 + 13),1,tmpData);//retrieve the length of the payload
			uint8_t pLen=(tmpData[0] + 14);//calculate the total length of the packet including header
			readTmpBuffer(generalCounter3,pLen,tmpData);
			generalCounter3 = (generalCounter3 + pLen);//add the total length, and substract one so we are pointing at the last byte of the previous packet
			generalCounter2--;//remove the packet we just got from the queue
			uint32_t ID= ((uint32_t)tmpData[5] << 24) + ((uint32_t)tmpData[6] << 16) + ((uint32_t)tmpData[7] << 8) + ((uint32_t)tmpData[8]);
			if(ID == targetID)//if it is the taget ID
			{
				if(isTransferring == true && downloadStarted == true && (tmpData[14] & 0xF0) == 0x20)//if we have a transfer active and this is a following frame
				{
					uint8_t toAdd=(tmpData[13] - 1);
					if(toAdd < transferLen)
					{
						for(uint8_t a=0; a< toAdd; a++) //add it to the buffer to be written
						{
							data[(generalCounter4 + a)]=tmpData[(a + 15)];//start from the first data byte
						}
						transferLen = (transferLen - toAdd);
						generalCounter4 = (generalCounter4 + toAdd);
					}
					else
					{
						for(uint8_t a=0; a< transferLen; a++) //add it to the buffer to be written
						{
							data[(generalCounter4 + a)]=tmpData[(a + 15)];//start from the first data byte
						}
						generalCounter4 = (generalCounter4 + transferLen);
						transferLen = 0;
						isTransferring=false;//we are done with this specific transfer
						sd.write((char*)data,generalCounter4);//write the previous data to the file
						totalBytes = (totalBytes + generalCounter4);
						generalCounter4 = 0;//reset the data counter
					}
					cTimeout=0;//reset the transfer timeout
				}
				else if (isTransferring == false && (((tmpData[14] & 0xF0) == 0x10 && tmpData[16] == UDS_TRANSFER_DATA) || ((tmpData[14] & 0xF0) == 0x00 && tmpData[15] == UDS_TRANSFER_DATA)))//if we are in a download and this is a transfer package
				{
					if(downloadStarted == false)//we just got a transfer request, so lets flag it
					{
						downloadStarted = true;
					}
					uint8_t toAdd;//used to know the data bytes
					uint8_t toskip;//used to know how many bytes to skip from the log payload
					if((tmpData[14] & 0xF0) == 0x10)//if multiframe
					{
						transferLen=(tmpData[14] & 0x0F);//we need to remove the SID and block number from this calculation
						transferLen=(transferLen << 8);
						transferLen= (transferLen + tmpData[15]) - 2;
						toAdd=(tmpData[13] - 4);//calculate the bytes to add, removing the frame type, length, SID and block number
						toskip=18;
					}
					else
					{
						transferLen=tmpData[14];
						toAdd=(tmpData[13] - 3);//calculate the bytes to add, removing the frame type, SID and block number
						toskip=17;
					}
					if(toAdd < transferLen)
					{
						for(uint8_t a=0; a< toAdd; a++) //add it to the buffer to be written
						{
							data[(generalCounter4 + a)]=tmpData[(a + toskip)];//start from the first data byte
						}
						transferLen = (transferLen - toAdd);
						generalCounter4 = (generalCounter4 + toAdd);
						isTransferring=true;//set the flag to indicate that a transfer just started
					}
					else
					{
						for(uint8_t a=0; a< transferLen; a++) //add it to the buffer to be written
						{
							data[(generalCounter4 + a)]=tmpData[(a + toskip)];//start from the first data byte
						}
						generalCounter4 = (generalCounter4 + transferLen);
						transferLen = 0;
						isTransferring=false;//we are done with this specific transfer
						sd.write((char*)data,generalCounter4);//write the previous data to the file
						totalBytes = (totalBytes + generalCounter4);
						generalCounter4 = 0;//reset the data counter
					}
					cTimeout=0;//reset the transfer timeout
				}
				else if(((tmpData[14] & 0xF0) != 00 && (tmpData[14] & 0xF0) != 0x10 && (tmpData[14] & 0xF0) != 0x20 && (tmpData[14] & 0xF0) != 0x30) || (downloadStarted == true && 	(tmpData[14] & 0xF0) == 0x00  && tmpData[15] == UDS_REQUEST_TRANSFER_EXIT))//transfer exit or this would mean some loader or something that takes over UDS is now talking
				{
					itsDone=true;
					if(bus == 1)//disable interfaces logging. bitwise encoded
					{
						setCANBadgerStatus(CAN1_LOGGING,0);
					}
					else
					{
						setCANBadgerStatus(CAN2_LOGGING,0);
					}							
				}
				else 
				{
					if(downloadStarted == true)
					{
						cTimeout++;
					}
				}									
			}				
		}
		else
		{
			if(downloadStarted == true)
			{
				cTimeout++;
				wait(0.001);
			}
		}
		if(cTimeout > timeout)
		{
			itsDone=true;
			if(bus == 1)//check on which interfaces to log. bitwise encoded
			{
				setCANBadgerStatus(CAN1_LOGGING,0);
			}
			else
			{
				setCANBadgerStatus(CAN2_LOGGING,0);
			}				
		}
	}
	sd.closeFile();//close the log to save the file
	char tmpName2[256]="/Transfers/CAN/";
	strcat(tmpName2,idNum);
	strcat(tmpName2,nderscr);
	strcat(tmpName2,sequence);
	char extnsion2[3]=".h";
	strcat(tmpName2,extnsion2);	
	if(dumpFormat == 1)//if we requested to generate the C code for it
	{
		generatePayloadFromBin(filename2,tmpName2, totalBytes);
	}
	return true;
	
}

bool CANbadger::generatePayloadFromArray(uint8_t *source, const char *dest, uint32_t size)
{
 	if (!sd.openFile(dest, O_WRONLY | O_CREAT | O_TRUNC))//if we couldnt create the new file, then same thing
  {
		return false;
  }
	char ch[256]={0};
	memset(ch,0,256);
	sprintf(ch,"//This file was automatically generated by the CANBadger\n\n\nuint8_t payload[%d]=\n{\n", size);
	writeToLog(ch);
	generalCounter1 = 0;
	bool po=false;//to avoid adding the colon to the first char
	while(generalCounter1 < size)
	{
		memset(ch,0,256);
		char r[2];//buffer
		r[0] = source[generalCounter1];
		generalCounter1++;
		if(po == false)
		{
			sprintf(ch,"	0x%x",r[0]);
			po=true;
		}
		else
		{
			sprintf(ch,"	,0x%x",r[0]);
		}
		writeToLog(ch);
		for(uint8_t c=0;c < 15; c++)
		{
			r[0] = source[generalCounter1];
			generalCounter1++;
			sprintf(ch,",0x%x",r[0]);
			writeToLog(ch);
			if(generalCounter1 == size)
			{
				break;
			}
		}
		sprintf(ch,"\n");
		writeToLog(ch);
	}
	sprintf(ch,"};\n");
	writeToLog(ch);
	sd.closeFile();//close the files
	return true;
}

bool CANbadger::generatePayloadFromBin(const char *source, const char *dest, uint32_t size)
{
	if(!sd.doesFileExist(source))//if the file you want to parse doesnt exist, then nope
	{
		return false;
	}	
 	if (!sd.openFile(dest, O_WRONLY | O_CREAT | O_TRUNC) || !sd.openFile(source, O_RDONLY,2))//if we couldnt create the new file, then same thing
 	{
		return false;
 	}
	char ch[256]={0};
	memset(ch,0,256);
	sprintf(ch,"//This file was automatically generated by the CANBadger\n\n\nuint8_t payload[%d]=\n{\n", size);
	writeToLog(ch);
	uint32_t a = 1;
	bool po=false;//to avoid adding the colon to the first char
	while(a != 0)
	{	
		memset(ch,0,256);
		char r[2];//buffer
		a=sd.read(r, 1, 2);//get the first byte
		if(a == 0)
		{
			break;
		}
		if(po == false)
		{
			sprintf(ch,"	0x%x",r[0]);	
			po=true;
		}
		else
		{
			sprintf(ch,"	,0x%x",r[0]);
		}
		writeToLog(ch);
		for(uint8_t c=0;c < 15; c++)
		{
			a=sd.read(r, 1, 2);//get the header
			if(a == 0)
			{
				break;
			}
			sprintf(ch,",0x%x",r[0]);
			writeToLog(ch);
		}
		sprintf(ch,"\n");
		writeToLog(ch);
	}
	sprintf(ch,"};\n");
	writeToLog(ch);
	sd.closeFile();//close the files
	sd.closeFile(2);
	return true;
}


bool CANbadger::writeToLog(char *stuff) //just to make it easier
{
	if (sd.write(stuff,convert.getArrayLength(stuff)) != convert.getArrayLength(stuff))
	{
		return 0;
	}
	return 1;
}


void CANbadger::writeTmpBuffer(uint32_t startAdr, uint32_t len, const uint8_t* data)
{
	if(startAdr + len > 4096)//dont write over the buffer size
	{
		return;
	}
	for(uint32_t a = 0; a < len; a++)
	{
		tmpBuffer[(a + startAdr)] = data[a];
	}
}

void CANbadger::readTmpBuffer(uint32_t startAdr, uint32_t len, uint8_t* data)
{
	if(startAdr + len > 4096)//dont read over the buffer size
	{
		return;
	}
	for(uint32_t a = 0; a < len; a++)
	{
		data[a] = tmpBuffer[(a + startAdr)];
	}
}


void CANbadger::enableCANBridge(uint8_t interface)
{
	if(interface == 0)
	{
		setCANBadgerStatus(CAN1_TO_CAN2_BRIDGE,0);
		setCANBadgerStatus(CAN2_TO_CAN1_BRIDGE,0);
	}
	else if(interface == 1)
	{
		setCANBadgerStatus(CAN1_TO_CAN2_BRIDGE,1);
		setCANBadgerStatus(CAN2_TO_CAN1_BRIDGE,0);
	}
	else if (interface == 2)
	{
		setCANBadgerStatus(CAN1_TO_CAN2_BRIDGE,0);
		setCANBadgerStatus(CAN2_TO_CAN1_BRIDGE,1);		
	}
	else
	{
		setCANBadgerStatus(CAN1_TO_CAN2_BRIDGE,1);
		setCANBadgerStatus(CAN2_TO_CAN1_BRIDGE,1);
	}	
}

bool CANbadger::CANBridge(bool enable)
{
	//to disable it, we need to make sure that nothing is currently using it. or do we?
	if(enable == false && getCANBadgerStatus(CAN_BRIDGE_ENABLED))// && !getCANBadgerStatus(CAN1_TO_CAN2_BRIDGE) && !getCANBadgerStatus(CAN2_TO_CAN1_BRIDGE) && !getCANBadgerStatus(CAN1_LOGGING) && !getCANBadgerStatus(CAN2_LOGGING))
	{
		setCANBadgerStatus(CAN_BRIDGE_ENABLED,0);
		can1.attach(0);
		can2.attach(0);
		return true;
	}
	//to enable it, we dont need to check anything as it will just start immediately
	else if(enable == true && !getCANBadgerStatus(CAN_BRIDGE_ENABLED))
	{
		setCANBadgerStatus(CAN_BRIDGE_ENABLED,1);
		if(!getCANBadgerStatus(KLINE1_LOGGING) && !getCANBadgerStatus(KLINE2_LOGGING))//if we are not using XRAM to log on KLINE
		{
			ram.clearRAM();
		}
		can1.attach(this,&CANbadger::doCANBridge, CAN::RxIrq);
		can2.attach(this,&CANbadger::doCANBridge, CAN::RxIrq);
		return true;
	}
	return false;
}


void CANbadger::setCANBadgerStatus(uint8_t statusType, uint8_t value)
{
	this->canbadger_settings->setStatus(statusType, value);
}

bool CANbadger::getCANBadgerStatus(uint8_t statusType)
{
	return this->canbadger_settings->getStatus(statusType);
}

void CANbadger::setCanbadgerSettings(CanbadgerSettings *cbs)
{
	this->canbadger_settings = cbs;

	setCANSpeed(1, this->canbadger_settings->getSpeed(1));
	setCANSpeed(2, this->canbadger_settings->getSpeed(2));
	checkSPISpeed(1);
}

CanbadgerSettings* CANbadger::getCanbadgerSettings() {
	return this->canbadger_settings;
}


void CANbadger::setCommandQueue(Mail<EthernetMessage, 16> *comQ)
{
	this->commandQueue = comQ;
}

void CANbadger::setEthernetManager(EthernetManager *em)
{
	this->ethernet_manager = em;
}

void CANbadger::ethernetLoop() {
	oled.clearScreen();
	oled.displayMessage(" Ethernet Mode");
	oled.displayMessage("Press any button",1);
	oled.displayMessage("   to reboot",1);

	bool pressed = false;

	while(!pressed) {
		for(uint8_t i = 1; i <= 4; i++) {
			if(buttons.isButtonPressed(i)) {
				pressed = true;
				break;
			}
		}
		Thread::yield();
		ethernet_manager->run();

		osEvent evt = this->commandQueue->get(1);
		if(evt.status == osEventMail) {
			EthernetMessage *msg;
			msg = 0;
			msg = (EthernetMessage*) evt.value.p;
			if(msg != 0) {
				handleEthernetMessage(msg, this);
			}
			this->commandQueue->free(msg);
			delete msg;
		}
	}

	NVIC_SystemReset();
}

EthernetManager* CANbadger::getEthernetManager()
{
	return this->ethernet_manager;
}

CAN* CANbadger::getCANClient(uint8_t interface)
{
	if(interface == 0)
	{
		return &can1;
	}
	else if (interface == 1)
	{
		return &can2;
	}
	else
	{
		return NULL;
	}
}

void CANbadger::setUDSHandler(UDSCANHandler *udsh)
{
	if(udsh == NULL)
	{
		delete this->uds_handler;
		this->uds_handler = NULL;
	}
	else
	{
		this->uds_handler = udsh;
	}
}

UDSCANHandler* CANbadger::getUDSHandler()
{
	return this->uds_handler;
}

size_t CANbadger::getFileSize(const char *fileName)
{
	return sd.getFileSize(fileName);
}

size_t CANbadger::readFile(const char *filename, uint8_t *dst, size_t offset, size_t length)
{
	return sd.readFile(filename, dst, offset, length);
}

void CANbadger::removeFileNoninteractive(const char *filename)
{
	sd.removeFileNoninteractive(filename);
}

bool CANbadger::doesFileExist(const char *filename) {
	return sd.doesFileExist(filename);
}

size_t CANbadger::writeFile(const char *filename, uint8_t *data, size_t offset, size_t length)
{
	return sd.writeFile(filename, data, offset, length);
}

FileHandler* CANbadger::getFileHandler() {
	return &sd;
}

Timer* CANbadger::getTimer() {
	return &timer;
}

GPIOHandler* CANbadger::getGPIOHandler() {
	return &relays;
}

// send a simple command to the display to check if we get a positive response
bool CANbadger::checkForDisplay() {
	char cmnd_data[] = {0x15, 0, 1};
	return oled.send(0x00, cmnd_data, 3);
}
