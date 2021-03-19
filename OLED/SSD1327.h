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

#ifndef SSD1327_H
#define SSD1327_H

#include "mbed.h"
#include "defaultfont.h"
#include "buttons.h"

/*LCD stuff*/
#define LCD_SDA P0_27 //pins used for the LCD
#define LCD_SCL P0_28

class SSD1327
{



public:
    /* Create SSD1327 object
    */
    SSD1327();

    ~SSD1327();

    const static uint8_t address   = 0x3c << 1;
    const static uint8_t cmd_mode  = 0x00;
    const static uint8_t data_mode = 0x40;

    const static uint8_t PIXELS         = 128;
    const static uint8_t PIXEL_PER_CHAR = 8; // if default font used

    const static uint8_t ROWS = PIXELS / PIXEL_PER_CHAR;
    const static uint8_t COLS = PIXELS / PIXEL_PER_CHAR;

	void initScreen();
    
    /** Set Gray Level 
    * @param level - new gray level  (0..0xF)
    */
    void set_gray_level(unsigned char level);
    
    /** Set text font
    * @param Font - font array
    * @param width - font width (only 8, 16, 24 or 32)
    * @param height - font height
    */
    void set_font(const uint8_t *Font, int width, int height);
    
    /** Set new Row and Column
    * @param row - new row (0..95)
    * @param column - new column (0..95)
    */
    void set_rc(unsigned char row, unsigned char column);
    
    /** Set new text Row and Column
    * Used after set_font
    * @param row - new row (0..96/font_height-1)
    * @param column - new column (0..96/font_width-1)
    */
    void set_text_rc(unsigned char row, unsigned char column);
    
    /** Put char into display using current font
    * @param C - printed char
    */
    void putc(unsigned char C);

    /** Put string into display using current font
    * @param str - printed string
    */
    void puts(const char *str);
		
	uint8_t getCurrentRow();

	uint8_t getCurrentColumn();

	uint8_t showOLEDMenu(const char *header, const char** items, uint8_t numberOfItems, Buttons* buttons);
    
    /** Clear display
    */
    void clearScreen();

		/** Displays a message on the OLED screen

		    @param const char *message is the message to be displayed
			@param nextline displays it in the next available line. If the last line was the bottom, it will display it on the top line
		    @param append will append the message to the existing one
			@param addSpace will add a space at the beginning of the message if set to true. useful for using cursors in menus.
			@param doClearScreen will clear the screen before showing the message
			@param row specifies on which row to start
			@param column specifies on which column to start
			@return //0 if the back button was pressed, otherwise the number of the option that was chosen
		 */

	void displayMessage(const char *message, bool nextLine= 0, bool append = 0, bool addSpace = 0, bool doClearScreen = 0, uint8_t row = 0, uint8_t column = 0);

	void clearLine(uint8_t lineNumber);



    
    // Service low-level functions

    bool send(char mode, const char data[], size_t len);

    void tribyte_cmd(uint8_t a, uint8_t b, uint8_t c);
    
    void set_column_address(uint8_t start, uint8_t end);

    void set_row_address(uint8_t start, uint8_t end);
    
    //bool sendf(char mode, char data[], size_t len);
    
    bool sendx(char mode, char *data, uint32_t len);
    
private:

    I2C* i2c;

    
};

#endif // SSD1327_H

// EOF
