/*
* Ser23LC1024 library
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

#ifndef __SER23LC1024_H__
#define __SER23LC1024_H__

#include "mbed.h"
#include "RTOS_SPI.h"


/*SPI RAM stuff*/
#define RAM_CMD_READ    0x03
#define RAM_CMD_WRITE   0x02
#define RAM_CMD_RDMR    0x05
#define RAM_CMD_WRMR    0x01


/**
A class to read and write Ser23LC1024 serial SPI RAM devices from Microchip.
*/
class Ser23LC1024
{
    public:
        /**
            create the handler class
            @param spi the SPI port where the eeprom is connected. Must be set to format(8,3), and with a speed matching the one of your device (up to 20MHz should work)
            @param enable the pin name for the port where /CS is connected
            @param hold then pin name for the port where /HOLD is connected
            
        */
        Ser23LC1024(RTOS_SPI *spi, PinName enable, PinName hold);
        
        /**
            destroys the handler, and frees the /CS and /HOLD pin        
        */
        ~Ser23LC1024();
        
        /**
            read a part of the RAM memory. The buffer will be allocated here, and must be freed by the user
            @param startAdr the adress where to start reading. Doesn't need to match a page boundary
            @param len the number of bytes to read (must not exceed the end of memory)
            @return the number of bytes read
        */
        bool read(uint32_t startAdr, uint32_t len, uint8_t* data);
        
        /**
            writes the given buffer into the memory. This function handles dividing the write into 
            pages, and waites until the phyiscal write has finished
            @param startAdr the adress where to start writing. Doesn't need to match a page boundary
            @param len the number of bytes to read (must not exceed the end of memory)
            @return true if data was read, false if it was not
        */
        bool write(uint32_t startAdr, uint32_t len, const uint8_t* data);
        /**
            fills the RAM with 0xFF
        */
        void clearRAM();
				
				bool isRAMBusy();
				

				void clearPage(uint32_t pageNum);
        
    private:
				void writePage(uint32_t startAdr, uint32_t len, const uint8_t* data);
 
        RTOS_SPI* _spi;
        DigitalOut* _enable;
				DigitalOut* _hold;
				int _size,_pageSize;
				bool isBusy;
        
};



#endif
