/*
* SSD1327 library for 128x128 OLED display
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

#include "SSD1327.h"


// font info
int font_w;
int font_h;
int font_bpc; //bytes per char width
uint8_t *font_s;

unsigned char currR, currC;

unsigned char grayH;
unsigned char grayL;


bool SSD1327::send(char mode, const char data[], size_t len)
    {
        for (size_t c = 0; c < len; c++) {
            const char snd[2]= { mode, data[c] };
            if (i2c->write(address, snd, sizeof(snd)) != 0)
                return false;
        }

        return true;
    }

/*bool SSD1327::sendf(char mode, char data[], size_t len)
    {
        for (size_t c = 0; c < len; c++) {
            const char snd[2]= { mode, data[c] };
            if (i2c->write(address, snd, sizeof(snd)) != 0)
                return false;
        }

        return true;
    } */

bool SSD1327::sendx(char mode, char *data, uint32_t len)
    {
        char snd[52];
        snd[0]=mode;
        for (uint32_t c=0;c<len;c++) snd[c+1]=data[c];
        if (i2c->write(address, snd, len+1) != 0)
                return false;
        return true;
    }

void SSD1327::tribyte_cmd(uint8_t a, uint8_t b, uint8_t c)
    {
        char data[] = { a, b, c };
        send(cmd_mode, data, sizeof(data));

    }

void SSD1327::set_column_address(uint8_t start, uint8_t end)
    {
        tribyte_cmd(0x15, start, end);
    }

uint8_t SSD1327::getCurrentRow()
{
	return currR;
}
		
uint8_t SSD1327::getCurrentColumn()
{
	return currC;
}


void SSD1327::set_row_address(uint8_t start, uint8_t end)
    {
        tribyte_cmd(0x75, start, end);
    }
    
void SSD1327::set_gray_level(unsigned char level)
    {
        grayL =  level & 0x0F;
        grayH =  grayL<<4;
    }
    
void SSD1327::set_text_rc(unsigned char row, unsigned char column)
{
  currR=row;
  currC=column;
	set_column_address(column*font_w/2,column*font_w/2+font_w/2-1);
  set_row_address(0x00+(row*font_h),0x00+(row*font_h+(font_h-1)));
}    

void SSD1327::set_rc(unsigned char row, unsigned char column)
{
  if (row>PIXELS) row=PIXELS;
  if (column>PIXELS) column=PIXELS;
  currR=row;
  currC=column/2;
  set_column_address(8+column/2,8+column/2+font_w/2-1);
  set_row_address(row,row+(font_h-1));
}    

void SSD1327::putc(unsigned char C)
{
  uint8_t line;
  uint8_t *pchar;
  char cc=0;
  char out[50];
  if(C == '\n')//support for newline
  {
	  if(getCurrentColumn() != 0)
	  {
		  line=(getCurrentRow() + 1);
	  }
	  else
	  {
		  line=(getCurrentRow());
	  }
	  if(line == 16)
	  {
		  line=0;
	  }
	  clearLine(line);
	  set_text_rc(line,0);
	  return;
  }
  if(C < 32 || C > 127) //Ignore non-printable ASCII characters. This can be modified for multilingual font.
  {
      C=' '; //Space
  }

  pchar=font_s+((C-32)*font_h*font_bpc);

  for (uint32_t i = 0; i < font_h; i++)
  {
	  char outcc=0;
      for (char j=0; j<font_bpc; j++)
      {
          line=pchar[cc++];
          for (char k=0; k<4;k++)
          {
            char c=0x00;
            char bit1=line & 0x80; line<<=1;
            char bit2=line & 0x80; line<<=1;
            c|=(bit1)?grayH:0x00;
            c|=(bit2)?grayL:0x00;
            out[outcc++]=c;
          }
      }
      sendx(data_mode,out,font_bpc*4);
    }
    set_text_rc(currR,currC+1);
}

void SSD1327::puts(const char *str)
{
    unsigned char i=0;
    while(str[i])
    {
        putc(str[i]);
        i++;
    }
}

void SSD1327::clearLine(uint8_t lineNumber)
{
	set_text_rc(lineNumber,0);
	for (uint8_t a=0; a<16;a++)
	{
		putc(' ');
	}
}

void SSD1327::set_font(const uint8_t *Font, int width, int height) {
  font_s= (uint8_t *) Font;
  font_h=height;
  font_w=width;
  font_bpc=width/8;
}

void SSD1327::displayMessage(const char *message, bool nextLine, bool append, bool addSpace, bool doClearScreen, uint8_t row, uint8_t column)
{
	if(doClearScreen)
	{
		clearScreen();
	}
	char tmp=0;
	char space = ' ';
	uint16_t cnt=column;//to count the characters in the line
	uint8_t line=row;//to track the current line number
	if(nextLine == true)
	{
		if(getCurrentColumn() != 0)
		{
			line=(getCurrentRow() + 1);
		}
		else
		{
			line=(getCurrentRow());
		}
		if(line == 16)
		{
			line=0;
		}
		clearLine(line);
	}
	else if(append == true)
	{
		cnt = getCurrentColumn();
		line= getCurrentRow();
	}
	uint32_t currentPos=0;
	set_text_rc(line,cnt);
	if(addSpace == true)
	{
		putc(space);
	}
	while(message[currentPos])//we are looking for the first space, to make sure that the word fits in a single line, and for the end of the message
	{
		tmp=message[currentPos];
		if(tmp == '\n')//if we got a newline
		{
			line++;
			if(line > 15)
			{
				return; //need to add scrolling feature, but that will be in the future. now we just return
			}
			set_text_rc(line,0);
			cnt=0;
		}
		else
		{
			putc(tmp);
			if(cnt == 15)
			{
				line++;
				if(line > 15)
				{
					return; //need to add scrolling feature, but that will be in the future. now we just return
				}
				set_text_rc(line,0);
				cnt=0;
				if(message[(currentPos + 1)] == '\n')//if the next char is newline
				{
					currentPos++;//skip it so we dont add two lines
				}
			}
			else
			{
				cnt++;
			}
		}
		currentPos++;
	}
}

void SSD1327::initScreen()
{
	set_font(default_font,8,8);
	clearScreen();
}

uint8_t SSD1327::showOLEDMenu(const char *header, const char** items, uint8_t numberOfItems, Buttons* buttons)
{

	//make the menu take more than 15 items
	clearScreen();
	char space= ' ';
	char arrow= '>';
	uint8_t chosenOption=1;
	bool scrollScreen=true;//we use this to refresh the shown options on the screen
	double pages = ((double)numberOfItems / 15.0) + 1.0;//check how many pages do we have with file names
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
			clearScreen();
			displayMessage(header);
			uint8_t itemPointer=((currentPage - 1)*15);//used to track the item displayed on the top
			for(uint8_t a=0; a< 15; a++)
			{
				if((a + itemPointer == numberOfItems))
				{
					break;
				}
				displayMessage((const char*)" ",1);
				displayMessage(items[(a + itemPointer)],0,1);
			}
			set_text_rc(chosenOption,0);//we set the cursor on current highlight
			putc(arrow);
			scrollScreen = false;
		}
		uint8_t a = buttons->getButtonPressed(0.15);//check which button was pressed
		if(a == 1)
		{
			numOfPages=(15 * (currentPage - 1) + getCurrentRow());//we just reuse this var to get the current file name
			clearScreen();
			return numOfPages;//returns the current line, which is the chosen option
		}
		else if (a == 4)//return button pressed
		{
			clearScreen();
			return 0;
		}
		else if (a == 2)//One option down, the "+" button
		{
			uint16_t b = getCurrentRow();
			set_text_rc(b,0);
			putc(space);
			if(b == numberOfItems && numberOfItems < 15)//in order to not overwrite the header. This is the case where we have less than 15 files, so they fit in the screen
			{
				b = 1;
				set_text_rc(b,0);
				putc(arrow);
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
			else if(b + ((currentPage - 1) * 15) == numberOfItems)//if this is the last file
			{
				currentPage = 1;
				scrollScreen = true;//and refresh the entire screen
				chosenOption = 1;
			}
			else
			{
				set_text_rc((b + 1),0);
				putc(arrow);
			}

		}
		else if (a == 3)//One option up, the "-" button
		{
			uint16_t b = getCurrentRow();
			set_text_rc(b,0);
			putc(space);
			if(b == 1 && numberOfItems <= 15)//in order to not overwrite the header. This is the case where we have less than 15 files, so they fit in the screen
			{
				b = numberOfItems;
				set_text_rc(b,0);
				putc(arrow);
			}
			else if(b == 1 && numberOfItems > 15)//if we have hit the top and there is more than one screen
			{
				if(currentPage == 1)//if we are on the first page
				{
					currentPage = numOfPages;//set the page pointer to the last page
					chosenOption = numberOfItems % 15;//get the remainder to set the pointer there
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
				set_text_rc((b - 1),0);
				putc(arrow);
			}
		}
	}
}



void SSD1327::clearScreen()
{
	char empty =' ';
	for (uint8_t a=0; a<16; a++)
	{
		set_text_rc(a,0);    
		for (uint8_t j=0;j<16;j++) putc(empty);
	}
	set_text_rc(0,0);
}


SSD1327::~SSD1327()
{
	delete i2c;
}

SSD1327::SSD1327()
{
	i2c= new I2C(LCD_SDA, LCD_SCL);
	// Init gray level for text. Default:Brightest White
        grayH= 0xF0;
        grayL= 0x0F;
		i2c->frequency(1000000);
        static const char init[] = { 0xAE, 0x15, 0x00, 0x7F, 0x75, 0x00,
                                     0x7F, 0x81, 0x80, 0xA0, 0x51, 0xA1,
                                     0x00, 0xA2, 0x00, 0xA4, 0xA8, 0x7F,
                                     0xB1, 0xF1, 0xB3, 0x00, 0xAB, 0x01,
                                     0xB6, 0x0F, 0xBE, 0x0F, 0xBC, 0x08,
                                     0xD5, 0x62, 0xFD, 0x12};

        if (not send(cmd_mode, init, sizeof(init))) {
//            pc.printf("SSD1327 init failed.\n");
        }

        wait_ms(100);

        static const char init2[] = { 0xA0, 0x42 };

        if (not send(cmd_mode, init2, sizeof(init2))) {
//            return false;
        }
        static const char init3[] = {0xAF};
				send(cmd_mode,init3,1);
				wait_ms(300);
        clearScreen();
    }

// EOF
