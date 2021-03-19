/*
* CANBadger File Handler
* Copyright (c) 2019 Javier Vazquez
* Copyright (c) 2021 Noelscher Consulting GmbH
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

/*File handler for the CANBadger*/


#ifndef __FILEHANDLER_H__
#define __FILEHANDLER_H__

#include "mbed.h"
#include "SDFileSystem.h"
#include <string>
#include "stdio.h"
#include <vector>
#include "RTOS_SPI.h"
#include "rtos.h"
#include "conversions.h"

/*SD PINS*/
#define SD_MOSI P0_9
#define SD_MISO P0_8
#define SD_SCK  P0_7
#define SD_SS   P0_6



class FileHandler
{
	public:

		FileHandler();//Constructor and Destructor
		~FileHandler();

		bool checkSD();

		bool doesDirExist(const char *dirName);

		bool makeFolder(const char *folderName);

		bool doesFileExist(const char *filename);

		void removeFileNoninteractive(const char *filename);

		bool deleteFile(const char *fileName);

		bool isFileOpen(uint8_t fileNo);

		bool file_copy(const char *src, const char *dst);

		uint32_t getFilePosition();//returns the current position of the file. Will return 0xFFFFFFFF if error

		bool lseekFile(uint32_t offset, uint8_t op);

        /** Retrieves the size of a file
            @param *filename is the absolute path to the file in the SD

			@return The size of the file

         */
		uint32_t getFileSize(const char *filename);

		bool openFile(const char *filename, uint16_t mode, uint8_t fileNo=1);

		bool closeFile(uint8_t fileNo=1);

		size_t readFile(const char *filename, uint8_t *dst, size_t offset, size_t length);

		size_t writeFile(const char *filename, uint8_t *data, size_t offset, size_t length);

		bool listAllFiles(char *lookupDir, vector<std::string>* filenames);

		bool formatSD();

		uint32_t read(char *whereTo, uint32_t howMuch, uint8_t fileNo=1);

		bool write(char *whereFrom, uint32_t howMuch, uint8_t fileNo=1);

        /** Provides a sequential filename for a base filename.
         * For example, if you input RAW_ and BIN, it will return RAW_1.BIN if that file didnt exist.
         * Limit is 65535.
            @param fileName is the file path and base name. The total filename will also be returned here.
            @param fileExtension is the extension of the file

            @return true if an available sequence was found, false if it wasnt.

		*/
		bool getSequencialFileName(char *fileName, char *fileExtension);




	private:

		RTOS_SPI* sdcard;
		SDFileSystem* sd;
		FileHandle* file;
		FileHandle* file2;
		bool isFile1Open;
		bool isFile2Open;
		uint32_t filePointer;//used to know the current position of the file

};
#endif
