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


/**Current tasks
-Write log handler
-rewrite UDS library so it uses the TP library. this adds flexibility to be used with any interface (eth, can, kline...)

-Ethernet
-usb serial
*/

#ifndef __CANBADGER_H__
#define __CANBADGER_H__


#include <Extensions/gpio_handler.hpp>
#include "mbed.h"
#include "SDFileSystem.h"
#include <string.h>
#include <vector>
#include <inttypes.h>
#include <cinttypes>
#include "UDSCAN.h"
#include "TP.h"
#include "tp20.h"
#include "AES.h"
#include "IAP.h"
#include "USBSerial.h"
#include "atoh.h"
#include "EthernetInterface.h"
#include "ethernet_message.hpp"
#include "ethernet_manager.hpp"
#include "canbadger_settings.hpp"
#include "conversions.h"
#include "Ser25aa02e48.h"
#include "SER23LC1024.h"
#include "SSD1327.h"
#include "RTOS_SPI.h"
#include "USBMSD_SD.h"
#include "kline.h"
#include "rtos.h"
#include "mitm_helper.hpp"
#include "CAN_MITM.h"
#include "command_handler.hpp"
#include "buttons.h"
#include "fileHandler.h"
#include "socketcan.h"
#include "DiagScan.h"
#include "PID.h"
#include "kwp2k_can.h"
#include "kwp2k_tp20.h"



/*Make sure all the below defines correspond to your board revision*/

/*CAN stuff*/
#define CAN1_RX P0_0
#define CAN1_TX P0_1
#define CAN2_RX P0_4
#define CAN2_TX P0_5

/*EEPROM and SPI RAM pins*/
#define RAM_MOSI P0_18
#define RAM_MISO P0_17
#define RAM_SCK  P0_15
#define RAM_CS   P0_19
#define RAM_HOLD P0_21
#define ROM_HOLD P0_22
#define ROM_CS   P0_16
#define RAM_FREQUENCY 20000000
#define EEPROM_FREQUENCY 10000000

/*K-LINE stuff*/
#define K_LINE1_TX P2_0
#define K_LINE1_RX P2_1
#define K_LINE2_TX P0_10
#define K_LINE2_RX P0_11


/*LED stuff*///based on REV.A first batch
#define GLED_PIN P3_25
#define RLED_PIN P3_26


/****General defines****/

/*LED states*/

#define LED_OFF 0
#define LED_RED 1
#define LED_GREEN 2
#define LED_ORANGE 3

/***Defines for the Constructor***/
#define ENABLE_CAN1          0
#define ENABLE_CAN2          1
#define ENABLE_KLINE1        2
#define ENABLE_KLINE2        3
#define ENABLE_SD  	         4
#define ENABLE_RAM        	 5
#define ENABLE_EEPROM        6
#define ENABLE_BUTTONS       7
#define ENABLE_OLED	         8
#define ENABLE_LED	         9

#define ENABLE_ETHERNET     11
#define ENABLE_USB	        12
#define ENABLE_KEYBOARD     13

/***Defines for the Status manager***/
#define SD_ENABLED 0
#define USB_SERIAL_ENABLED 1
#define ETHERNET_ENABLED 2
#define OLED_ENABLED 3
#define KEYBOARD_ENABLED 4
#define LEDS_ENABLE 5
#define KLINE1_INT_ENABLED 6
#define KLINE2_INT_ENABLED 7
#define CAN1_INT_ENABLED 8
#define CAN2_INT_ENABLED 9
#define KLINE_BRIDGE_ENABLED 10
#define CAN_BRIDGE_ENABLED 11
#define CAN1_LOGGING 12
#define CAN2_LOGGING 13
#define KLINE1_LOGGING 14
#define KLINE2_LOGGING 15
#define CAN1_STANDARD 16
#define CAN1_EXTENDED 17
#define CAN2_STANDARD 18
#define CAN2_EXTENDED 19
#define CAN1_TO_CAN2_BRIDGE 20
#define CAN2_TO_CAN1_BRIDGE 21
#define KLINE1_TO_KLINE2_BRIDGE 22
#define KLINE2_TO_KLINE1_BRIDGE 23
#define UDS_CAN1_ENABLED 24
#define UDS_CAN2_ENABLED 25
#define CAN1_USE_FULLFRAME 26
#define CAN2_USE_FULLFRAME 27
#define CAN1_MONITOR 28
#define CAN2_MONITOR 29



static const char fwVersion[9]={'F','W',' ','V','1','.','0','A',0};//fw version string

class CanbadgerSettings;
class EthernetManager;
class CAN_MITM;

class CANbadger
{
	public:
				/**
            	create the handler class
				 */
				CANbadger();
	
				~CANbadger();

		        /** Sets the LED to a specific Color.

							@param color is used to set the LED to a specific color.
								The options are as follow:
								LED_OFF
								LED_RED
								LED_GREEN
								LED_ORANGE
				*/
				void setLED(uint8_t color);

				void test();

				/** Blinks the LED a number of times in a specific color for 0.2 seconds each time.
				            @param times is how many times should it blink
							@param color determines on which color should it blink. Same options as setLED.

							@return TRUE if the frame was sent within the required timeout, FALSE if it wasnt.

				*/
				void blinkLED(uint8_t times, uint8_t color);

		        /** Reads all the characters in the serial buffer and dumps them. Used to empty the uart buffer.

				*/
				void dumpSerial();

				
				/** Enables CAN Bridge mode between CAN1 and CAN2
				 
				    @param enable specifies the interface number
						@param silent boolean indicating whether to go into silent mode or not
						@return True if the operation was performed, false if it was not.
				 */				

				bool CANBridge(bool enable);
				

		        /** Returns the status of a flag regarding the current status of the CT
		            @param statusType is the status to retrieve.
						These are all the available status:
						SD_ENABLED
						USB_SERIAL_ENABLED
						ETHERNET_ENABLED
						OLED_ENABLED
						KEYBOARD_ENABLED
						LEDS_ENABLE
						KLINE1_INT_ENABLED
						KLINE2_INT_ENABLED
						CAN1_INT_ENABLED
						CAN2_INT_ENABLED
						KLINE_BRIDGE_ENABLED
						CAN_BRIDGE_ENABLED
						CAN1_LOGGING
						CAN2_LOGGING
						KLINE1_LOGGING
						KLINE2_LOGGING
						CAN1_STANDARD
						CAN1_EXTENDED
						CAN2_STANDARD
						CAN2_EXTENDED
						CAN1_TO_CAN2_BRIDGE
						CAN2_TO_CAN1_BRIDGE
						KLINE1_TO_KLINE2_BRIDGE
						KLINE2_TO_KLINE1_BRIDGE
						UDS_CAN1_ENABLED
						UDS_CAN2_ENABLED
						CAN1_USE_FULLFRAME
						CAN2_USE_FULLFRAME
						CAN1_MONITOR
						CAN2_MONITOR

					@return TRUE if the requested status is set, FALSE if it is not set

				*/
				bool getCANBadgerStatus(uint8_t statusType);

		        /** Sets the status of a flag regarding the status of the CT
		            @param statusType is the status to set. They are the same as above.
					@param value is what to set the status to (must be 0 or 1 since it is boolean).

					@return TRUE if the requested status is set, FALSE if it is not set

				*/
				void setCANBadgerStatus(uint8_t statusType, uint8_t value);

				/** Sets the speed for CAN
				 	@param bus is the bus number (1 or 2) to change the speed on
		            @param speed is the speed in bps

					@return TRUE if the requested baudrate is set, FALSE if it is not set

				*/
				bool setCANSpeed(uint8_t bus, uint32_t speed);

				/** Sets the speed for K-LINE
				 	@param bus is the bus number (1 or 2) to change the speed on
		            @param speed is the speed in bps

					@return TRUE if the requested baudrate is set, FALSE if it is not set

				*/
				void setKLINESpeed(uint8_t bus, uint32_t speed);

		        /** Clears the external RAM by filling it with 0xFF

				*/
				void clearRAM();
				
		        /** Writes an array of data to the external RAM
		            @param startAdr is the start address to write to
					@param len is the total length of the data to be written
					@param data is the array to be written to the external RAM

					@return TRUE if the request succeeded, FALSE if it did not

				*/
				bool writeRAM(uint32_t startAdr, uint32_t len, const uint8_t* data);

		        /** Reads data from the external RAM into an array
		            @param address is the start address to read from
					@param len is the total length of the data to be read
					@param data is the array where data will be stored to

					@return TRUE if the request succeeded, FALSE if it did not

				*/
				bool readRAM(uint32_t address, uint32_t len, uint8_t* data);

		        /** Clears the external EEPROM by filling it with 0xFF

				*/
				void clearEEPROM();

		        /** Writes an array of data to the external EEPROM
		            @param startAdr is the start address to write to
					@param len is the total length of the data to be written
					@param data is the array to be written to the external RAM

					@return TRUE if the request succeeded, FALSE if it did not

				*/
				bool writeEEPROM(unsigned int startAdr, unsigned int len, const uint8_t* data);

		        /** Reads data from the external EEPROM into an array
		            @param address is the start address to read from
					@param len is the total length of the data to be read
					@param data is the array where data will be stored to

					@return TRUE if the request succeeded, FALSE if it did not

				*/
				bool readEEPROM(unsigned int startAdr, unsigned int len, uint8_t* data);		

		        /** Reads the UID from the external eeprom
		            @param data is the array to read the UID to

				*/
				void readEEPROMUID(uint8_t* data);

		        /** Writes an array of data to the temp buffer (circular buffer)
		            @param startAdr is the start address to write to
					@param len is the total length of the data to be written
					@param data is the array to be written to the external RAM

				*/
				void writeTmpBuffer(uint32_t startAdr, uint32_t len, const uint8_t* data);

				
		        /** Reads data from the temp buffer into an array
		            @param address is the start address to read from
					@param len is the total length of the data to be read
					@param data is the array where data will be stored to

				*/
				void readTmpBuffer(uint32_t startAdr, uint32_t len, uint8_t* data);
				
		        /** Sends a CAN frame on the specified bus. Used by the command handler
		            @param frameFormat determines if the frame is a Standard (CANStandard) or an extended (CANExtended) frame
					@param frameType determines if the frame is a Data frame (CANData) or a Remote frame (CANRemote) frame

					@return TRUE if the frame was sent within the required timeout, FALSE if it wasnt.

				*/
				bool sendCANFrame(uint32_t msgID, uint8_t *payload, uint8_t len, uint8_t bus, CANFormat frameFormat = CANStandard, CANType frameType = CANData, uint32_t timeout = 10);

				bool DHCPEthernetConnect();
				
				bool manualEthernetConnect(const char *ip, const char *mask, const char *gateway);
				
				void ethernetDisconnect();
				
				char* getEthernetMACAddress();
				
				char* getEthernetIPAddress();
				
				char* getEthernetGateway();
				
				char* getEthernetNetworkMask();

		        /** Retrieves which button was pressed on the keypad. It is blocking.
		            @param waitTime is how long to wait after the button has been pressed

		            @return the button that was pressed (1 for Start, 2 for Down, 3 for Up and 4 for Back).

				*/
				
				
				bool startUDSSession(uint8_t interface);//1 for CAN1, 2 for CAN2

		        /** Opens a file and displays its contents in the screen
		            @param filename is the file to be opened

		            @return true if the file was displayed, false if it wasnt

				*/
				bool displayFile(const char *filename);
				
				void enableCANBridge(uint8_t interface);//0 to disable both, 1 for 1 only, 2 for 2 only, 3 for both
				
				bool startTP20Session(uint8_t interface);
				
				bool startLog();//uint8_t interfaces);

		        /** Sets the padding byte for CAN transmissions if using full frame.
					@param interfaceNo is the interface to set the byte for (1 or 2)
		            @param pByte is the value of the padding byte


				*/
				void setCANPaddingByte(uint8_t interfaceNo, uint8_t pByte);
				
				bool dumpCANUDSTransfer(uint32_t targetID, uint8_t bus, uint8_t dumpFormat = 0, uint32_t timeout = 2000);//dumps the first transfer it sees. dumpformat 0 is binary, 1 is txt to copy to array.
				
				/** Generates a header file with an array ready to copy&paste containing the binary payload passed to it
				 
				    @param const char *source is a char array containing the name of the binary file that contains the payload
					@param const char *dest is a char array containing the name of the destination file
				    @param uint32_t size is the size of the source file in bytes
					@return //0 if it failed, 1 if it succeeds
				 */		

				bool generatePayloadFromBin(const char *source, const char *dest, uint32_t size);

				bool generatePayloadFromArray(uint8_t *source, const char *dest, uint32_t size);
				

				//Menus and interactive stuff
				
				/** Shows the files (not the directories!) existing in a target directory, and returns the chosen one. Use the + and - to go up, start to select a file, and back to go back.
				 
				    @param char *lookupDir is the directory where the files should be listed from. If an option is chosen, it will be added here
						@return //true if a file was chosen, false if no file was chosen.
				 */					
				
				bool getFileName(char *lookupDir);

				size_t getFileSize(const char *filename);

				size_t readFile(const char *filename, uint8_t *dst, size_t offset, size_t length);

				void removeFileNoninteractive(const char *filename);

				size_t writeFile(const char *filename, uint8_t *data, size_t offset, size_t length);


				void CANConfigMenu(uint8_t busno);//Graphic menu to configure CAN parameters
				
				void ConfigMenu();//a menu just to enable and disable stuff on interfaces
				
				void CANBridgeMenu();//Shows the status of the CAN bridge, and allows to enable or disable it
				
				void mainMenu();//just the main menu, what did you expect
				
				void passthroughMenu();//Menu for passthrough options

				void loggingMenu();//Menu for log handling and other wonders of logging
				
				void analysisMenu();//menu for analysis features, such as protocol discovery, statistical analysis, etc...
				
				void diagMenu();//For everything related to diagnostics (UDS, TP, etc...)
				
				bool getDecValue(const char *Message, uint64_t *currentValue, uint64_t minValue, uint64_t maxValue, uint32_t step);//retrieves a decimal value using the screen and the keyboard
				
				bool getHexValue(const char *Message, uint64_t *currentValue, uint64_t minValue, uint64_t maxValue, uint32_t step);//retrieves a decimal value using the screen and the keyboard

				/** Shows a dialog on OLED to confirm file deletion. If ok button is pressed, file is deleted
				 
				    @param char *fileName is the absolute path to the file to be deleted (without the /sd part)
				 */			

				bool deleteFile(char *fileName);

				bool formatSD();
				

				void KWP2KCANDiagMenu(KWP2KCANHandler *kwp, uint8_t interfaceno);

				void KWP2KCANDataMenu(KWP2KCANHandler *kwp);

				void KWP2KCANDTCMenu(KWP2KCANHandler *kwp);

				void KWP2KCANTransferMenu(KWP2KCANHandler *kwp);

				void KWP2KMemoryMenu(KWP2KCANHandler *kwp);

				void KWP2KSecurityHijackMenu();

				void ScanKWP2KIDs();

				uint16_t KWP2KSecurityHijack(uint32_t ownID, uint32_t rID, uint8_t level);

				void doOLEDKWP2K(CAN *canbus, uint8_t interfaceNo = 1, bool isInSession = false);


				void TP20TransferMenu(KWP2KTP20Handler *tp20);

				void TP20SecAccessMenu(KWP2KTP20Handler *tp20);



				/**UDS menus***/
				
				void UDSCANDiagMenu(UDSCANHandler *uds, uint8_t interfaceno);
				
				uint32_t detectCANSpeed(uint8_t busno);

				void UDSCANDataMenu(UDSCANHandler *uds);
				
				void UDSCANDTCMenu(UDSCANHandler *uds);
				
				void UDSCANCommsControlMenu(UDSCANHandler *uds);
				
				void UDSCANMemoryMenu(UDSCANHandler *uds);
				
				void UDSCANTransferMenu(UDSCANHandler *uds);

				void UDSCANReconMenu();
				
				void UDSCANSecAccessMenu(UDSCANHandler *uds);

				void ScanUDSIDs();//will query a range of CAN IDs to see if they support UDS
				
				void ScanActiveUDSIDs();
				
				void ScanActiveKWP2KIDs();

				void UDSSecurityHijackMenu();
				
				void UDSSecurityHammerMenu();

				void TP20SecurityHijackMenu();

				uint8_t UDSSecurityHijack(uint32_t ownID, uint32_t rID, uint8_t level, uint8_t sessionType);//hijacks a security access between CAN1 and CAN2. returns the lvl we just hijacked
				
				uint16_t TP20SecurityHijack(uint32_t ownID, uint32_t rID, uint8_t level);//hijacks a security access between CAN1 and CAN2. returns the lvl we just hijacked on lower nibble, TP counter on upper nibble

				void displayError(uint8_t* errorString);
				
				uint32_t grabASCIIValue();//used by mitm

				void MITMMode(char *filename, CANFormat format =  CANStandard);

				void CANMITMMenu();


				// MISC
				GPIOHandler* getGPIOHandler();


				// MITM
				CAN_MITM* getNewMITM();

				bool addMITMRule(const char *rule);

				bool startMITM();


				// Ethernet
				void setCommandQueue(Mail<EthernetMessage, 16> *commQ);

				void setEthernetManager(EthernetManager *ethernet_manager);

				EthernetManager* getEthernetManager();

				void ethernetLoop();


				// Settings
				void setCanbadgerSettings(CanbadgerSettings *canbadger_settings);

				CanbadgerSettings* getCanbadgerSettings();



				bool checkForDisplay();

				bool listAllFiles(char *lookupDir, vector<std::string>* filenames);

				bool doesDirExist(const char *dirName);

				FileHandler* getFileHandler();

				Timer* getTimer();

				CAN* getCANClient(uint8_t interface);

				void setUDSHandler(UDSCANHandler *udsh);

				UDSCANHandler* getUDSHandler();

				bool chooseStartupMode();//true for ethernet, false for standalone

				void socketCANMenu();

				void CANReconMenu();

				void KWP2KSecurityHammerMenu();

				void KWP2KCANReconMenu();

				void TP20ReconMenu();

				void TP20DataMenu(KWP2KTP20Handler *tp20);

				void TP20DiagMenu(KWP2KTP20Handler *tp20);

				void findTP20Chans();

				void scanTP20Chan();

				void TP20SecurityHammerMenu();

				void TP20DTCMenu(KWP2KTP20Handler *tp20);

				void TP20MemoryMenu(KWP2KTP20Handler *tp20);

				uint32_t generalCounter1;//four counters for general purpose. Used as well to pass CAN IDs and stuff
				uint32_t generalCounter2;
				uint32_t generalCounter3;
				uint32_t generalCounter4;
				
				uint32_t localID;//used for CAN
				uint32_t remoteID;//used for CAN

				uint8_t currentDiagSession;//used to know the kind of diag session that is currently being held
				bool isSDInserted;
				uint8_t tmpBuffer[4096];//4kb of buffer, thats a lot
				Mail<EthernetMessage, 16> *commandQueue;
				EthernetManager *ethernet_manager;
		private:
				void doCANBridge(void);
				void checkSPISpeed(uint8_t memType);
				void doOLEDTP20(CAN *canbus, uint8_t interfaceNo, bool isInSession = false, uint8_t tpCounter = 0);
				void doOLEDUDS(CAN *canbus, uint8_t interfaceno, bool isInSession=false);
				bool writeToLog(char *stuff);
				uint8_t CAN1PaddingByte;//used to fill a CAN frame
				uint8_t CAN2PaddingByte;//used to fill a CAN frame
				CanbadgerSettings *canbadger_settings;
				UDSCANHandler *uds_handler = NULL;
				CAN_MITM *persistent_mitm = NULL;
};

#endif
