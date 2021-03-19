/*
* Conversions library
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


#include "conversions.h"


Conversions::Conversions() 
{

}

Conversions::~Conversions() 
{

}


uint32_t Conversions::getUInt32FromData(uint32_t offset, char *data)
{
		return (data[offset+3] << 24) | (data[offset+2] << 16) | (data[offset+1] << 8) | (data[offset+0]);	
}

void Conversions::itoa(uint64_t i, char *s, uint8_t digits)
{
	uint64_t sum = i;
	uint8_t e = 0;
	char digit;

	while (sum && (e < digits))
	{
		digit = sum % 10;
		if (digit < 0xA)
			s[e++] = '0' + digit;
		else
			s[e++] = 'A' + digit - 0xA;
		sum /= 10;
	}
	if (e == digits && sum)
	{
		return;
	}
	s[e] = '\0';
	strrev(s);
	return;
}
	
uint8_t Conversions::getHexNumberLength(uint64_t number)
{
	uint8_t howLong=0;
	if(number <= 0xFF)
	{
		howLong=2;
	}
	else if(number > 0xFF && number <= 0xFFFF)
	{
		howLong=4;
	}
	else if(number > 0xFFFF && number <= 0xFFFFFF)
	{
		howLong=6;
	}
	else if(number > 0xFFFFFF && number <= 0xFFFFFFFF)
	{
		howLong=8;
	}
	else if(number > 0xFFFFFFFF && number <= 0xFFFFFFFFFF)
	{
		howLong=10;
	}
	else if(number > 0xFFFFFFFFFF && number <= 0xFFFFFFFFFFFF)
	{
		howLong=12;
	}	
	else if(number > 0xFFFFFFFFFFFF && number <= 0xFFFFFFFFFFFFFF)
	{
		howLong=14;
	}
	else if(number > 0xFFFFFFFFFFFFFF && number <= 0xFFFFFFFFFFFFFFFF)
	{
		howLong=16;
	}	
	return howLong;
}

void Conversions::strrev(char *str)
{
	int i;
	int j;
	unsigned char a;
	unsigned len = strlen((const char *)str);
	for (i = 0, j = len - 1; i < j; i++, j--)
	{
		a = str[i];
		str[i] = str[j];
		str[j] = a;
	}
}


void Conversions::itox(uint64_t i, char *s, uint8_t digits)
{
		unsigned char n;
		s += digits;
		*s = '\0';
		for (n = digits; n != 0; --n)
		{
			*--s = "0123456789ABCDEF"[i & 0x0F];
			i >>= 4;
		}	
	
}

uint32_t Conversions::getArrayLength(char *input)
{
		uint32_t count=0;
		while(input[count] != 0)
		{
			count++;
		}
		return count;	
}

double Conversions::grabFloatValue(char *s, uint8_t position, char separator)
{
	uint32_t cnt=0; //used as the counter for the array to convert to float
	char got[12]={0};//array to store the float value to convert
	uint32_t cnt2=0;//counter for the provided line array
	uint8_t cnt3=1;//counter for the field
	while((s[cnt2] == '*' || s[cnt2] == 0xA || s[cnt2] == 0xD) && s[cnt2] != 0x0)//discard the beginning of the string
	{
		cnt2++;
	}
	if(s[cnt2] == 0x00)
	{
		return 0xFFFFFFFF;//end of string
	}
	while(cnt3 != position)
	{
			char a = s[cnt2];
			if(a == ',')
			{
					cnt3++;
			}
			if(s[cnt2] == 0x00)
			{
				return 0xFFFFFFFF;//end of string
			}
			cnt2++;
	}
	while(s[cnt2] != separator && s[cnt2] != 0xA && s[cnt2] != 0xD)
	{
		got[cnt]=s[cnt2];
		cnt++;
		cnt2++;
	}
	return strtod (got, NULL);	
}

uint32_t Conversions::grabHexValue(char *s, uint8_t position, char separator)
{
	uint32_t cnt=0; //used as the counter for the array to convert to float
	char got[12]={0};//array to store the float value to convert
	uint32_t cnt2=0;//counter for the provided line array
	uint8_t cnt3=1;//counter for the field
	while((s[cnt2] == '*' || s[cnt2] == 0xA || s[cnt2] == 0xD) && s[cnt2] != 0x0)//discard the beginning of the string
	{
		cnt2++;
	}
	if(s[cnt2] == 0x00)
	{
		return 0xFFFFFFFF;//end of string
	}
	while(cnt3 != position)
	{
			char a = s[cnt2];
			if(a == ',')
			{
					cnt3++;
			}
			if(s[cnt2] == 0x00)
			{
				return 0xFFFFFFFF;//end of string
			}
			cnt2++;
	}
	while(s[cnt2] != separator && s[cnt2] != 0xA && s[cnt2] != 0xD)
	{
		got[cnt]=s[cnt2];
		cnt++;
		cnt2++;
	}
	return strtol(got,NULL,0);		
}

bool Conversions::lineToRawFrame(char *s, uint32_t *tim, uint32_t *tID, uint8_t *tlen, uint8_t *tdata)
{

	uint32_t tmpval = grabHexValue(s, 1, ',');
	if (tmpval == 0xFFFFFFFF)//EOF
	{
		return 0;
	}
	*tim=tmpval;
	*tID=grabHexValue(s, 2, ',');
	*tlen = grabHexValue(s, 3, ',');
	for (uint8_t ccnt = 0; ccnt< *tlen; ccnt++)
	{
		tdata[ccnt]=grabHexValue(s, (ccnt + 4), ',');
	}
	return 1;	
}

uint32_t Conversions::inputToHex(USBSerial *input)//rewrite it to be both hex and dec
{
  char param[17]={0};
  uint32_t xx=0;
  uint8_t cnt=0;
  char inc=0xFF;
  while(!input->readable());//wait for incoming
  wait(0.1);//give time for all chars to arrive
  while(input->readable() && cnt < 16)
  {
    inc=input->getc();
    if(inc != 0xA && inc != 0xD)//to sanitize input
    {
		param[cnt]=inc;
		cnt++;
    }
    else
    {
    	break;
    }
    //device.printf(reinterpret_cast<char*>(&param[cnt]));
  }
  if(cnt == 0)
  {
    while(input->readable())
    {
        uint8_t trash=input->getc();
        wait(0.001);//wait for next char
		}
	  return 0; //you should not be using 0 anywhere
  }

  sscanf(param, "%x", &xx);
  while(input->readable())
  {
		uint8_t trash=input->getc();
    wait(0.001);//wait for next char
	}
	return xx;	
}

uint32_t Conversions::inputToDec(USBSerial *input)
{
  char param[17]={0};
  uint32_t xx=0;
  uint8_t cnt=0;
  char inc=0xFF;
  while(!input->readable());//wait for incoming
  wait(0.1);//give time for all chars to arrive
  while(input->readable() && cnt < 16)
  {
    inc=input->getc();
		if(inc >=0x30 && inc <= 0x39)
		{
			param[cnt]=inc;
			cnt++;
		}
		else
		{
			break;
		}
  }
  if(cnt == 0)
  {
    while(input->readable())
    {
        uint8_t trash=input->getc();
        wait(0.001);//wait for next char
		}
	  return 0; //you should not be using 0 anywhere
  }
  sscanf(param, "%d", &xx);
  while(input->readable())
  {
		uint8_t trash=input->getc();
    wait(0.001);//wait for next char
	}
	return xx;	
}


uint32_t Conversions::arrayToHex(char *input, uint8_t len)
{
  char param[(len + 1)];
  memset(param,0,(len + 1));
  uint32_t xx=0;
  for (uint8_t cnt=0;cnt<len;cnt++)
  {
    param[cnt]=input[cnt];
    //device.printf(reinterpret_cast<char*>(&param[cnt]));
  }
  sscanf(param, "%x", &xx);
  return xx;
}

bool Conversions::getBit(uint32_t b, uint8_t bitNumber) //checks if a bit is set in a byte
{
	if((b & (1 << bitNumber)))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void Conversions::setBit(uint32_t *whereTo, uint8_t value, uint8_t bitNumber)
{
		if(value == 0)
		{
			*whereTo &= ~(1UL << bitNumber);
		}
		else
		{
			*whereTo |= 1UL << bitNumber;
		}
}
