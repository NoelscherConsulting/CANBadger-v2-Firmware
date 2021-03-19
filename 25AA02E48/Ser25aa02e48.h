/*
* Ser25AA02E48 library
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

#ifndef __SER25AA02E48_H__
#define __SER25AA02E48_H__

#include "mbed.h"
#include <stdio.h>
#include "RTOS_SPI.h"

/*SPI EEPROM stuff*/
#define ROM_CMD_READ    0x03
#define ROM_CMD_WRITE   0x02
#define ROM_CMD_RDSR    0x05
#define ROM_CMD_WRSR    0x01
#define ROM_CMD_WRDI    0x04
#define ROM_CMD_WREN    0x06

/**
A class to read and write Ser25AA02E48 serial SPI eeprom devices from Microchip.
*/
class Ser25AA02E48
{
    public:
        /**
            create the handler class
            @param spi the SPI port where the eeprom is connected. Must be set to format(8,3), and with a speed matching the one of your device (up to 5MHz should work)
            @param enable the pin name for the port where /CS is connected
            @param bytes the size of you eeprom in bytes (NOT bits, eg. a 25LC010 has 128 bytes)
            
        */
        Ser25AA02E48(RTOS_SPI *spi, PinName enable, PinName hold);
        
        /**
            destroys the handler, and frees the /CS and /HOLD pin        
        */
        ~Ser25AA02E48();
        
        /**
            read a part of the eeproms memory. The buffer will be allocated here, and must be freed by the user
            @param startAdr the address where to start reading. Doesn't need to match a page boundary
            @param len the number of bytes to read (must not exceed the end of memory)
            @return NULL if the addresses are out of range, the pointer to the data otherwise
        */
        bool read(unsigned int startAdr, unsigned int len, uint8_t* data);

        /**
            read the UID of the EEPROM. response will be returned as an array pointer
        */
        void read_UID(uint8_t* ret);
        
        /**
            writes the give buffer into the memory. This function handles dividing the write into 
            pages, and waits until the physical write has finished
            @param startAdr the address where to start writing. Doesn't need to match a page boundary
            @param len the number of bytes to read (must not exceed the end of memory)
            @return false if the addresses are out of range
        */
        bool write(unsigned int startAdr, unsigned int len, const uint8_t* data);
        
        /**
            fills the given page with 0xFF
            @param pageNum the page number to clear
            @return if the pageNum is out of range        
        */
        bool clearPage(unsigned int pageNum);
        
        /**
            fills the while eeprom with 0xFF
        */
        void clearMem();
    private:
        bool writePage(unsigned int startAdr, unsigned int len, const uint8_t* data);
        int readStatus();
        void waitForWrite();
        void enableWrite();
        

        RTOS_SPI* _spi;
        DigitalOut* _enable;
				DigitalOut* _hold;
        int _size,_pageSize;
        
};



#endif
