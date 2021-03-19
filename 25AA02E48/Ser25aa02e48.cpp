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

#include "Ser25AA02E48.h"
#include "wait_api.h"

#define HIGH(x) ((x&0xff00)>>8)
#define LOW(x) (x&0xff)

Ser25AA02E48::Ser25AA02E48(RTOS_SPI *spi, PinName enable, PinName hold) 
{
    _spi=spi;
    _enable=new DigitalOut(enable);
	_hold=new DigitalOut(hold);
    _size=256;
    _pageSize=16;
    _enable->write(1);
	_hold->write(1);
}

Ser25AA02E48::~Ser25AA02E48() 
{
	delete _enable;
	delete _hold;
}

void Ser25AA02E48::read_UID(uint8_t* ret) 
{
    _enable->write(0);
    wait_us(1);
    // send address
    _spi->write(0x03);
    _spi->write(LOW(0xFA));
    // read data into buffer
    for (int i=0;i<6;i++) 
		{
        ret[i]=_spi->write(0);
    }
    wait_us(1);
    _enable->write(1);
}

bool Ser25AA02E48::read(unsigned int startAdr, unsigned int len, uint8_t* data) 
{
    // assertion
    if (startAdr+len>0xF9)
        return false;
    _enable->write(0);
    wait_us(1);
    // send address
    _spi->write(0x03);
    _spi->write(LOW(startAdr));
    // read data into buffer
    for (uint32_t i=0;i<len;i++)
	{
        data[i]=_spi->write(0);
    }
    wait_us(1);
    _enable->write(1);
    return true;
}

bool Ser25AA02E48::write(unsigned int startAdr, unsigned int len, const uint8_t* data) 
{
    if (startAdr+len>0xF9)
        return false;

    uint32_t ofs=0;
    while (ofs<len) {
        // calculate amount of data to write into current page
        int pageLen=_pageSize-((startAdr+ofs)%_pageSize);
        if (ofs+pageLen>len)
            pageLen=len-ofs;
        // write single page
        bool b=writePage(startAdr+ofs,pageLen,data+ofs);
        if (!b)
            return false;
        // and switch to next page
        ofs+=pageLen;
    }
    return true;
}

bool Ser25AA02E48::writePage(unsigned int startAdr, unsigned int len, const uint8_t* data) 
{
    enableWrite();

    _enable->write(0);
    wait_us(1);

    _spi->write(0x02);
    _spi->write(LOW(startAdr));

	// do data write
    for (uint32_t i=0;i<len;i++)
	{
        _spi->write(data[i]);
    }
    wait_us(1);
    // disable to start physical write
    _enable->write(1);
    
    waitForWrite();

    return true;
}

bool Ser25AA02E48::clearPage(unsigned int pageNum) 
{
    enableWrite();
    uint8_t s[_pageSize]={0xFF};
	memset(s,0xFF,_pageSize);
    return writePage(_pageSize*pageNum,_pageSize,s);
}

void Ser25AA02E48::clearMem() 
{
    enableWrite();
    for (int i=0;i<_size/_pageSize;i++) 
		{
        if (!clearPage(i))
				{
                break;
				}
    }
}

int Ser25AA02E48::readStatus() 
{
    _enable->write(0);
    wait_us(1);
    _spi->write(ROM_CMD_RDSR);
    int status=_spi->write(0x00);
    wait_us(1);
    _enable->write(1);
    return status;
}

void Ser25AA02E48::waitForWrite() 
{
    while (true) 
		{
        if (0==(readStatus()&1))
            break;
        wait_us(10);
    }
}

void Ser25AA02E48::enableWrite()
{
    _enable->write(0);
    wait_us(1);
    _spi->write(ROM_CMD_WREN);
    wait_us(1);
    _enable->write(1);
}
