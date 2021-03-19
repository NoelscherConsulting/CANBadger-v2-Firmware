/*
* K-LINE (ISO 9141-2) LIBRARY
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


#include "mbed.h"
#include "kline.h"

#define SBIT_WordLenght    0x00u
#define SBIT_DLAB          0x07u
#define SBIT_FIFO          0x00u
#define SBIT_RxFIFO        0x01u
#define SBIT_TxFIFO        0x02u

#define SBIT_RDR           0x00u
#define SBIT_THRE 				 0x05u


KLINEHandler::KLINEHandler(Serial *kline, uint8_t interfaceNo)
{
	_kline=kline;
	byteDelay=KLINE_DEFAULT_BYTE_DELAY;
	readTimeout= (KLINE_DEFAULT_READ_TIMEOUT * 10);
	speed=KLINE_DEFAULT_SPEED;
	byteTimeout = (KLINE_DEFAULT_BYTE_READ_TIMEOUT * 10);
	_kline->baud(speed);
	interface=interfaceNo;
}

KLINEHandler::~KLINEHandler()
{
	
}
	
void KLINEHandler::setTransmissionParameters(uint32_t doByteDelay, uint32_t readTimeaut, uint32_t bTimeout)
{
	byteDelay=doByteDelay;
	readTimeout= (readTimeaut * 10);
	byteTimeout = (bTimeout * 10);
}

void KLINEHandler::setBaudrate(uint32_t baudrate)
{
	speed=baudrate;
	_kline->baud(speed);
}

uint16_t KLINEHandler::getByte()
{
	uint32_t timeout = 0;
	while(!_kline->readable())
	{
		wait(0.0001);
		timeout++;
		if(timeout == (KLINE_DEFAULT_READ_TIMEOUT * 10))//if timeout
		{
			return 0xFF00;
		}
	}
	return _kline->getc();
}

bool KLINEHandler::sendByte(uint8_t toSend)
{
	uint32_t timeout = 0;
	while(_kline->readable())//first we empty the receive buffer. User should had checked if there was anything in it anyway
	{
		_kline->getc();
	}
	_kline->putc(toSend);
	while(!_kline->readable())
	{
		wait(0.0001);
		timeout++;
		if(timeout == (KLINE_DEFAULT_READ_TIMEOUT * 10))//if timeout
		{
			return false;
		}
	}	
	if(_kline->getc() != toSend)
	{
		return false;
	}
	return true;
}
	
void KLINEHandler::fastInit(uint8_t *initSequence, uint8_t len, bool doCRC)
{
	if(interface == 1)
	{
		LPC_PINCON->PINSEL4 &= ~ ((1<<1) | (1<<0));//set to GPIO function. right side is bit number. when you write a 1, you set it to 0
		LPC_GPIO2->FIODIR |= (1<<0);//set as output
		LPC_GPIO2->FIOSET |= (1<<0);//set high
		wait(0.1);
		LPC_GPIO2->FIOCLR |= (1<<0);//set low. Start bit
		wait(0.025);	//25ms down
		LPC_GPIO2->FIOSET |= (1<<0);//set high
		wait(0.025);//25ms high
		//after done with the fast init, restore uart	
		LPC_PINCON->PINSEL4 &= ~0x0000000F;//reset pins P2.0 and P1.0
		LPC_PINCON->PINSEL4 |= 0x0000000A;//enable TX1 and RX1
		LPC_UART1->FCR = (1<<SBIT_FIFO) | (1<<SBIT_RxFIFO) | (1<<SBIT_TxFIFO); // Enable FIFO and reset Rx/Tx FIFO buffers    
		LPC_UART1->LCR = (0x03<<SBIT_WordLenght) | (1<<SBIT_DLAB); // 8bit data, 1Stop bit, No parity
	}
	else
	{
		LPC_PINCON->PINSEL0 &= ~ ((1<<21) | (1<<20));//set to GPIO function. right side is bit number. when you write a 1, you set it to 0
		LPC_GPIO0->FIODIR |= (1<<10);//set as output
		LPC_GPIO0->FIOSET |= (1<<10);//set high
		wait(0.1);
		LPC_GPIO0->FIOCLR |= (1<<10);//set low. start bit
		wait(0.025);		
		LPC_GPIO0->FIOSET |= (1<<10);//set high
		wait(0.025);
		//after done with the init, restore uart		
		LPC_PINCON->PINSEL0 &= ~0x00F00000;//reset pins P0.10 and P0.11
		LPC_PINCON->PINSEL0 |= 0x000500000;//enable TX2 and RX2
		LPC_UART2->FCR = (1<<SBIT_FIFO) | (1<<SBIT_RxFIFO) | (1<<SBIT_TxFIFO); // Enable FIFO and reset Rx/Tx FIFO buffers    
		LPC_UART2->LCR = (0x03<<SBIT_WordLenght) | (1<<SBIT_DLAB); // 8bit data, 1Stop bit, No parity				
	}	
	_kline->baud(speed);//restore the baudrate
	write(initSequence, len, doCRC);//and now we send the sequence	
}


bool KLINEHandler::slowInit(uint8_t address)
{
	if(address > 0x7F)
	{
		return false; //address is 7bit
	}
	uint8_t p = address ^ (address >> 4 | address << 4);
	p = p ^ (p >> 2);
  p = p ^ (p >> 1);
	if(interface == 1)
	{
		LPC_PINCON->PINSEL4 &= ~ ((1<<1) | (1<<0));//set to GPIO function. right side is bit number. when you write a 1, you set it to 0
		LPC_GPIO2->FIODIR |= (1<<0);//set as output
		LPC_GPIO2->FIOSET |= (1<<0);//set high
		wait(0.1);
		LPC_GPIO2->FIOCLR |= (1<<0);//set low. Start bit
		wait(0.2);
		for (uint8_t a = 0; a < 7; a++)  //iterate through bit mask
		{
			 if (address & (1 << a)) // if bitwise AND resolves to true
			 {
				 LPC_GPIO2->FIOSET |= (1<<0);//set high
			 }
			 else
			 {
				 LPC_GPIO2->FIOCLR |= (1<<0);//set low
			 }
			 wait(0.2);
		}
		if((p & 1) != 1)//if even parity
		{
			LPC_GPIO2->FIOSET |= (1<<0);//set high
			wait(0.2);
		}
		else
		{
			LPC_GPIO2->FIOCLR |= (1<<0);//set low
			wait(0.2);
		}
		LPC_GPIO2->FIOSET |= (1<<0);//set high		
		wait(0.2);//stop bit	
		//after done with the bitbanging, restore uart	
		LPC_PINCON->PINSEL4 &= ~0x0000000F;//reset pins P2.0 and P1.0
		LPC_PINCON->PINSEL4 |= 0x0000000A;//enable TX1 and RX1
		LPC_UART1->FCR = (1<<SBIT_FIFO) | (1<<SBIT_RxFIFO) | (1<<SBIT_TxFIFO); // Enable FIFO and reset Rx/Tx FIFO buffers    
		LPC_UART1->LCR = (0x03<<SBIT_WordLenght) | (1<<SBIT_DLAB); // 8bit data, 1Stop bit, No parity
	}	
	else
	{
		LPC_PINCON->PINSEL0 &= ~ ((1<<21) | (1<<20));//set to GPIO function. right side is bit number. when you write a 1, you set it to 0
		LPC_GPIO0->FIODIR |= (1<<10);//set as output
		LPC_GPIO0->FIOSET |= (1<<10);//set high
		wait(0.1);
		LPC_GPIO0->FIOCLR |= (1<<10);//set low. start bit
		wait(0.2);
		for (uint8_t a = 0; a < 7; a++)  //iterate through bit mask
		{
			 if (address & (1 << a)) // if bitwise AND resolves to true
			 {
				 LPC_GPIO0->FIOSET |= (1<<10);//set high
			 }
			 else
			 {
				 LPC_GPIO0->FIOCLR |= (1<<10);//set low
			 }
			 wait(0.2);
		}
		if((p & 1) != 1)//if even parity
		{
			LPC_GPIO0->FIOSET |= (1<<10);//set high
			wait(0.2);
		}
		else
		{
			LPC_GPIO0->FIOCLR |= (1<<10);//set low
			wait(0.2);
		}
		LPC_GPIO0->FIOSET |= (1<<10);//set high. stop bit		
		wait(0.2);
		//after done with the bitbanging, restore uart		
		LPC_PINCON->PINSEL0 &= ~0x00F00000;//reset pins P0.10 and P0.11
		LPC_PINCON->PINSEL0 |= 0x000500000;//enable TX2 and RX2
		LPC_UART2->FCR = (1<<SBIT_FIFO) | (1<<SBIT_RxFIFO) | (1<<SBIT_TxFIFO); // Enable FIFO and reset Rx/Tx FIFO buffers    
		LPC_UART2->LCR = (0x03<<SBIT_WordLenght) | (1<<SBIT_DLAB); // 8bit data, 1Stop bit, No parity					
	}	
	_kline->baud(speed);//the speed is set to whatever the variable speed was set to
	uint32_t timeout = 0;//we will use this for timeout 
	while(!_kline->readable())
	{
		wait(0.0001);
		timeout++;
		if(timeout == 10000)//if one second has passed
		{
			return false;
		}
	}
	if(_kline->getc() != 0x55)//if we dont get the synchronization byte+
	{
		return false;
	}	
	return true;
}	
	
bool KLINEHandler::write(uint8_t *request, uint32_t len, bool doCRC)
{
  for(uint32_t i=0; i<len; i++)
  {
    _kline->putc(request[i]);
		uint32_t timeout = 0;//we will use this for timeout 
		while(!_kline->readable())//we should hear our own echo
		{
			wait(0.0001);
			timeout++;
			if(timeout == 10000)//if one second has passed and we couldnt send our msg
			{
				return false;
			}
		}
		if(_kline->getc() != request[i])
		{
			return false;
		}	
		if((i + 1) != len)
		{
			wait_ms(byteDelay);
		}
  }
	if(doCRC)
	{
		uint8_t crc=0;
		for(uint16_t i=0; i<len; i++)
		{
			crc=crc+request[i];
		}
		wait_ms(byteDelay);
		_kline->putc(crc);
		uint32_t timeout = 0;//we will use this for timeout 
		while(!_kline->readable())
		{
			wait(0.0001);
			timeout++;
			if(timeout == 10000)//if one second has passed
			{
				return false;
			}
		}
		if(_kline->getc() != crc)
		{
			return false;
		}		
	}
	return true;
}

uint32_t KLINEHandler::read(uint8_t *response, uint32_t len, bool checkCRC)
{
		uint32_t a = 0;
		uint32_t timeout = 0;//we will use this for timeout
		if(len != 0)//in an ideal case, we know the expected length
		{
			for(a = 0; a < len; a++)
			{
				timeout = 0;//we will use this for timeout
				while(!_kline->readable())
				{
					wait(0.0001);
					timeout++;
					if(timeout == readTimeout)//if we hit the timeout
					{
						return (0);//return zero
					}
				}
				response[a]=_kline->getc();
			}
			if(checkCRC)
			{
				timeout = 0;//we will use this for timeout
				while(!_kline->readable())
				{
					wait(0.0001);
					timeout++;
					if(timeout == readTimeout)//if we hit the timeout
					{
						return (a + 0xFF000000);//return what we have read, and MSB as FF to indicate that something went wrong
					}
				}
				uint8_t chksum=_kline->getc();	
				uint8_t crc=0;
				for(uint32_t i=0; i<len; i++)
				{
					crc=crc+response[i];
				}
				if(crc != chksum)
				{
					return (a + 0xFF000000);//return what we have read, and MSB as FF to indicate that something went wrong with checksum
				}
				return (a - 1);//because we already checked the checksum
			}
			return a;
		}
		else //but if we dont
		{
			timeout = 0;//we will use this for timeout
			while(!_kline->readable())//we wait a bit
			{
				wait(0.0001);
				timeout++;
				if(timeout == readTimeout)//if we hit the timeout
				{
					return 0;
				}
			}	
			//we are supposed to have something now
			timeout = 0;//need to reset it to get into the while loop
			while(timeout < byteTimeout)
			{
				response[a]=_kline->getc();
				a++;
				timeout = 0;
				while(!_kline->readable())//we wait a bit
				{
					wait(0.0001);
					timeout++;
					if(timeout == byteTimeout)//if we hit the timeout
					{
						break;//that must be it
					}
				}
			}
			if(checkCRC)
			{				
				uint8_t chksum=response[(a - 1)];	//last byte is the checksum
				uint8_t crc=0;
				for(uint32_t i=0; i<(a - 1); i++)//we need to remove one from a, which is the checksum byte itself
				{
					crc=crc+response[i];
				}
				if(crc != chksum)
				{
					return ((a - 1) + 0xFF000000);//return what we have read, and MSB as FE to indicate that something went wrong with checksum
				}
				return (a - 1);//again, we remove the checksum byte
			}	
			return a;
		}
}
			
			

void KLINEHandler::ftdiPassthrough(Buttons* buttons)
{
	bool klineStatus=true;
	bool ftdiStatus=true;
	bool currentKlineStatus;
	bool currentFtdiStatus;
	LPC_PINCON->PINSEL4 &= ~ ((1<<1) | (1<<0));//set to GPIO function. P2.0 (KLINE TX)
	LPC_PINCON->PINSEL4 &= ~ ((1<<3) | (1<<2));//P2.1 (KLINE RX)
	LPC_PINCON->PINSEL0 &= ~ ((1<<5) | (1<<4));//P0.2 (FTDI RX / MCU TX)
	LPC_PINCON->PINSEL0 &= ~ ((1<<7) | (1<<6));//P0.3 (FTDI TX / MCU RX)
	LPC_GPIO2->FIODIR |= (1<<0);//set P2.0 as output
	LPC_GPIO2->FIOSET |= (1<<0);//set P2.0 high
	LPC_GPIO0->FIODIR |= (1<<2);//set P0.2 as output
	LPC_GPIO2->FIOSET |= (1<<2);//set P0.2 high
	LPC_GPIO0->FIODIR &= ~((1<<3));//set P0.3 as input
	LPC_GPIO2->FIODIR &= ~((1<<1));//set P2.1 as input
	while(1)//!buttons->isButtonPressed(4))
	{
		currentFtdiStatus=((LPC_GPIO0->FIOPIN & (1 <<3))>> 3);
		currentKlineStatus=((LPC_GPIO2->FIOPIN & (1 <<1))>> 1);
		if(currentFtdiStatus == true && klineStatus == false)//if FTDI TX is high
		{
			LPC_GPIO2->FIOSET |= (1<<0);//set P2.0 high
			klineStatus=true;
		}
		else if(currentFtdiStatus == false && klineStatus == true)//else if FTDI TX is low
		{
			LPC_GPIO2->FIOCLR |= (1<<0);//set P2.0 low
			klineStatus=false;
		}
		if(currentKlineStatus == true && ftdiStatus == false)//if KLINE TX is high
		{
			LPC_GPIO0->FIOSET |= (1<<2);//set P0.2 high
			ftdiStatus=true;
		}
		else if(currentKlineStatus == false && ftdiStatus == true)//else if KLINE TX is low
		{
			LPC_GPIO0->FIOCLR |= (1<<2);//set P0.2 low
			ftdiStatus=false;
		}
	}
	LPC_GPIO2->FIOSET |= (1<<0);//set high
	wait(0.02);
	LPC_PINCON->PINSEL4 &= ~0x0000000F;//reset pins P2.0 and P1.0
	LPC_PINCON->PINSEL4 |= 0x0000000A;//enable TX1 and RX1
	LPC_UART1->FCR = (1<<SBIT_FIFO) | (1<<SBIT_RxFIFO) | (1<<SBIT_TxFIFO); // Enable FIFO and reset Rx/Tx FIFO buffers
	LPC_UART1->LCR = (0x03<<SBIT_WordLenght) | (1<<SBIT_DLAB); // 8bit data, 1Stop bit, No parity
}
