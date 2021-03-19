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

#include "Ser23LC1024.h"


#define HIGH(x) ((x&0xff0000)>>16)
#define MIDDLE(x) ((x&0x00ff00)>>8)
#define LOW(x) (x&0x0000ff)

Ser23LC1024::Ser23LC1024(RTOS_SPI *spi, PinName enable, PinName hold) 
{
    _spi=spi;
    _enable=new DigitalOut(enable);
		_hold=new DigitalOut(hold);
		_size=131072;
    _pageSize=32;
    _enable->write(1);
		_hold->write(1);
		isBusy= false;
}

Ser23LC1024::~Ser23LC1024() 
{
	delete _enable;
	delete _hold;
}

bool Ser23LC1024::isRAMBusy()
{
	return isBusy;
}

bool Ser23LC1024::read(uint32_t startAdr, uint32_t len, uint8_t* data) 
{
    // assertion
    if (startAdr+len>0x1F3FF)
		{
        return 0;
		}
		isBusy = true;
    _enable->write(0);
    // send address
    _spi->write(RAM_CMD_READ);
    _spi->write(HIGH(startAdr));
		_spi->write(MIDDLE(startAdr));
		_spi->write(LOW(startAdr));
    // read data into buffer
    for (uint32_t i=0;i<len;i++) 
		{
        data[i]=_spi->write(0);
    }
    _enable->write(1);
		isBusy = false;
    return 1;
}

bool Ser23LC1024::write(uint32_t startAdr, uint32_t len, const uint8_t* data) 
{
    if (startAdr+len>0x1F3FF)
        return false;

    uint32_t ofs=0;
    while (ofs<len) 
		{
        // calculate amount of data to write into current page
        int pageLen=_pageSize-((startAdr+ofs)%_pageSize);
        if (ofs+pageLen>len)
				{
            pageLen=len-ofs;
				}
        // write single page
        writePage(startAdr+ofs,pageLen,data+ofs);
        // and switch to next page
        ofs+=pageLen;
    }
    return true;
}

void Ser23LC1024::writePage(uint32_t startAdr, uint32_t len, const uint8_t* data)
{

	isBusy = true;
    _enable->write(0);
    _spi->write(RAM_CMD_WRITE);
    _spi->write(HIGH(startAdr));
	_spi->write(MIDDLE(startAdr));
	_spi->write(LOW(startAdr));

	// do data write
    for (uint32_t i=0;i<len;i++)
	{
        _spi->write(data[i]);
    }
    // disable to start physical write
    _enable->write(1);   
	isBusy = false;
    return;
}

void Ser23LC1024::clearPage(uint32_t pageNum) 
{
		isBusy = true;
    _enable->write(0);
    _spi->write(RAM_CMD_WRITE);
    _spi->write(HIGH(_pageSize*pageNum));
		_spi->write(MIDDLE(_pageSize*pageNum));
		_spi->write(LOW(_pageSize*pageNum));
	// do real write
    for (int i=0;i<32;i++) 
		{
        _spi->write(0xFF);
    }
    // disable to start physical write
    _enable->write(1);
		isBusy = false;
    return;
}

void Ser23LC1024::clearRAM() 
{
	uint32_t pages= 4096;//total number of pages
	uint32_t pageNo=0;
	while (pageNo<pages) 
	{
        // clear single page
        clearPage(pageNo);
        // and switch to next page
        pageNo++;
  }

}



