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


#include "fileHandler.h"

FileHandler::FileHandler()
{
	sdcard = new RTOS_SPI(SD_MOSI, SD_MISO, SD_SCK);
	sd = new SDFileSystem(sdcard, SD_SS, "sd");//we create the SDFileSystem
	isFile1Open = false;
	isFile2Open = false;
	filePointer = 0;
}

FileHandler::~FileHandler()
{
	delete sd;
	delete sdcard;
}

uint32_t FileHandler::getFilePosition()
{
	if(isFile1Open == false)
	{
		return 0xFFFFFFFF;
	}
	return filePointer;
}

bool FileHandler::checkSD()
{
    if(!doesDirExist("/"))//check if the SD is inserted and if we can communicate with it
    {
    	return false;
    }
	return true;
}

bool FileHandler::lseekFile(uint32_t offset, uint8_t op)
{
	if(isFile1Open == false)
	{
		return false;
	}
	file->lseek(offset, op);
	filePointer=offset;
	return true;
}

bool FileHandler::doesDirExist(const char *dirName)
{
	 DIR* dir;
	 char totalFilename[128]="/sd";
	 strcat(totalFilename, dirName);
	 dir=opendir(totalFilename);
	 if(dir)
	 {
		 closedir(dir);
		 return 1;
	 }
    return 0;
}

bool FileHandler::doesFileExist(const char *fileName)
{
	FileHandle* file123;
	file123 = sd->open(fileName, O_RDONLY);
	if (file123 == NULL)
    {
        return 0;
    }
    file123->close();
    return 1;
}

bool FileHandler::makeFolder(const char *folderName)
{
	char totalFilename[128]="/sd";
	strcat(totalFilename, folderName);
	if(mkdir(totalFilename,0777) != 0)
	{
		return false;
	}
	return true;
}

uint32_t FileHandler::getFileSize(const char *fileName)
{
	char totalFilename[128]="/sd";
	strcat(totalFilename, fileName);
	FILE *fpsrc = fopen(totalFilename, "r");   // src file
	fseek(fpsrc, 0, SEEK_END); // seek to end of file
	uint32_t fileSize = ftell(fpsrc);       // get current file pointer
	fclose(fpsrc);
	return fileSize;
}

bool FileHandler::file_copy(const char *src, const char *dst)
{
    int retval = 0;
    int ch;
	char totalFilename[128]="/sd";
	strcat(totalFilename, src);
	char totalFilename2[128]="/sd";
	strcat(totalFilename2, dst);

    FILE *fpsrc = fopen(totalFilename, "r");   // src file
    FILE *fpdst = fopen(totalFilename2, "w");   // dest file

    while (1)
    {                  // Copy src to dest
        ch = fgetc(fpsrc);       // until src EOF read.
        if (ch == EOF) break;
        fputc(ch, fpdst);
    }
    fclose(fpsrc);
    fclose(fpdst);

    fpdst = fopen(dst, "r");     // Reopen dest to insure
    if (fpdst == NULL)
    {          // that it was created.
        return false;           // Return error.
    }
    else
    {
        fclose(fpdst);
    }
    return retval;
}

uint32_t FileHandler::read(char *whereTo, uint32_t howMuch, uint8_t fileNo)
{
	if(fileNo == 1)
	{
		if(isFile1Open == false)
		{
			return 0;
		}
		uint32_t toReturn = file->read((char*)whereTo, howMuch);
		filePointer= (filePointer + toReturn);//update the file pointer
		return toReturn;
	}
	else
	{
		if(isFile2Open == false)
		{
			return 0;
		}
		return file2->read((char*)whereTo, howMuch);
	}
}


bool FileHandler::write(char *whereFrom, uint32_t howMuch, uint8_t fileNo)
{
	if(fileNo == 1)
	{
		if(isFile1Open == false)
		{
			return false;
		}
		file->write((char*)whereFrom,howMuch);
		if(file->fsync() == 0)//sync to make sure the data is in the file
		{
			filePointer = (filePointer + howMuch);
			return true;
		}
	}
	else
	{
		if(isFile2Open == false)
		{
			return false;
		}
		file2->write((char*)whereFrom,howMuch);
		if(file2->fsync() == 0)//sync to make sure the data is in the file
		{
			return true;
		}
	}
	return false;
}

size_t FileHandler::readFile(const char *filename, uint8_t *dst, size_t offset, size_t length)
{

	FileHandle *fp = sd->open(filename, O_RDONLY);
	if(fp == NULL)
	{
		// file not existing
		return 0;
	}
	ssize_t read_res = fp->read(dst + offset, length);
	fp->close();
	if(read_res == -1)
	{
		return 0;
	}
	return read_res;
}

void FileHandler::removeFileNoninteractive(const char *fname)
{
	if(doesFileExist(fname) == 1){
		sd->remove(fname);
	}
}

size_t FileHandler::writeFile(const char *filename, uint8_t *data, size_t offset, size_t length)
{
	FileHandle *fp = sd->open(filename, O_WRONLY + O_CREAT);
	ssize_t write_res = fp->write(data + offset, length);
	fp->close();
	if(write_res == -1){
		return 0;
	}
	return write_res;
}


bool FileHandler::deleteFile(const char *fileName)
{
	char totalFilename[128]="/sd";
	strcat(totalFilename, fileName);
	if(remove(totalFilename) == 0)
	{
		return true;
	}
	return false;
}

bool FileHandler::formatSD()
{
	if(sd->format() == 0)
	{
		return true;
	}
	return false;
}


bool FileHandler::listAllFiles(char *lookupDir, vector<std::string>* filenames)
{
	DIR *dp;
	struct dirent *dirp;
	char fullDir[90]="/sd";
	strcat(fullDir, lookupDir);
	dp = opendir(fullDir);
	//read all directory and file names in current directory into filename vector
	while((dirp = readdir(dp)) != NULL)
	{
		filenames->push_back(string(dirp->d_name));
	}
	closedir(dp);
	return true;
}


bool FileHandler::openFile(const char *filename, uint16_t mode, uint8_t fileNo)
{
	if(fileNo == 1)
	{
		file = sd->open(filename, mode);//open source file
		if (file == NULL)//if we couldnt open the file
		{
			return false;
		}
		isFile1Open=true;
	}
	else
	{
		file2 = sd->open(filename, mode);//open source file
		if (file2 == NULL)//if we couldnt open the file
		{
			return false;
		}
		isFile2Open=true;
	}
	return true;
}

bool FileHandler::isFileOpen(uint8_t fileNo) {
	if(fileNo == 1) {
		return isFile1Open;
	} else if(fileNo ==2) {
		return isFile2Open;
	} else {
		return false;
	}

}


bool FileHandler::closeFile(uint8_t fileNo)
{
	if(fileNo == 1)
	{
		if (isFile1Open == false)
		{
	 		return false;
		}
		file->close();
		isFile1Open = false;
	}
	else
	{
		if (isFile2Open == false)
		{
	 		return false;
		}
		file2->close();
		isFile2Open = false;
	}
	return true;
}

bool FileHandler::getSequencialFileName(char *fileName, char *fileExtension)
{
	uint16_t logNumber=1;//for sequential log writing
	bool itsDone=false;//to know when we found an available filename
	while(1)
	{
		char filename2[90];
		memset(filename2,0,90);
		strcat(filename2,fileName);
		char sequence[6];//if we need more than 5 digits for a log number, i think its time to cleanup the SD
		Conversions convert;
		convert.itox(logNumber,sequence,4);
		strcat(filename2,sequence);
		strcat(filename2,fileExtension);
		if(!doesFileExist(filename2))
		{
			strcat(fileName,sequence);
			strcat(fileName,fileExtension);
			return true;
		}
		else
		{
			logNumber++;
		}
		if(logNumber > 65535)
		{
			return false;//i think that 65535 logs are enough
		}
	}
}
