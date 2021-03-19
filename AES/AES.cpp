/* AES Cipher Library
 * Copyright (c) 2016 Neil Thiessen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "AES.h"

const char AES::m_Sbox[256] = {
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5,
    0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
    0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0,
    0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
    0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC,
    0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A,
    0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
    0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0,
    0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
    0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B,
    0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85,
    0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
    0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5,
    0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
    0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17,
    0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
    0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88,
    0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
    0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C,
    0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
    0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9,
    0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6,
    0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
    0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E,
    0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
    0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94,
    0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68,
    0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16
};

const char AES::m_InvSbox[256] = {
    0x52, 0x09, 0x6A, 0xD5, 0x30, 0x36, 0xA5, 0x38,
    0xBF, 0x40, 0xA3, 0x9E, 0x81, 0xF3, 0xD7, 0xFB,
    0x7C, 0xE3, 0x39, 0x82, 0x9B, 0x2F, 0xFF, 0x87,
    0x34, 0x8E, 0x43, 0x44, 0xC4, 0xDE, 0xE9, 0xCB,
    0x54, 0x7B, 0x94, 0x32, 0xA6, 0xC2, 0x23, 0x3D,
    0xEE, 0x4C, 0x95, 0x0B, 0x42, 0xFA, 0xC3, 0x4E,
    0x08, 0x2E, 0xA1, 0x66, 0x28, 0xD9, 0x24, 0xB2,
    0x76, 0x5B, 0xA2, 0x49, 0x6D, 0x8B, 0xD1, 0x25,
    0x72, 0xF8, 0xF6, 0x64, 0x86, 0x68, 0x98, 0x16,
    0xD4, 0xA4, 0x5C, 0xCC, 0x5D, 0x65, 0xB6, 0x92,
    0x6C, 0x70, 0x48, 0x50, 0xFD, 0xED, 0xB9, 0xDA,
    0x5E, 0x15, 0x46, 0x57, 0xA7, 0x8D, 0x9D, 0x84,
    0x90, 0xD8, 0xAB, 0x00, 0x8C, 0xBC, 0xD3, 0x0A,
    0xF7, 0xE4, 0x58, 0x05, 0xB8, 0xB3, 0x45, 0x06,
    0xD0, 0x2C, 0x1E, 0x8F, 0xCA, 0x3F, 0x0F, 0x02,
    0xC1, 0xAF, 0xBD, 0x03, 0x01, 0x13, 0x8A, 0x6B,
    0x3A, 0x91, 0x11, 0x41, 0x4F, 0x67, 0xDC, 0xEA,
    0x97, 0xF2, 0xCF, 0xCE, 0xF0, 0xB4, 0xE6, 0x73,
    0x96, 0xAC, 0x74, 0x22, 0xE7, 0xAD, 0x35, 0x85,
    0xE2, 0xF9, 0x37, 0xE8, 0x1C, 0x75, 0xDF, 0x6E,
    0x47, 0xF1, 0x1A, 0x71, 0x1D, 0x29, 0xC5, 0x89,
    0x6F, 0xB7, 0x62, 0x0E, 0xAA, 0x18, 0xBE, 0x1B,
    0xFC, 0x56, 0x3E, 0x4B, 0xC6, 0xD2, 0x79, 0x20,
    0x9A, 0xDB, 0xC0, 0xFE, 0x78, 0xCD, 0x5A, 0xF4,
    0x1F, 0xDD, 0xA8, 0x33, 0x88, 0x07, 0xC7, 0x31,
    0xB1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xEC, 0x5F,
    0x60, 0x51, 0x7F, 0xA9, 0x19, 0xB5, 0x4A, 0x0D,
    0x2D, 0xE5, 0x7A, 0x9F, 0x93, 0xC9, 0x9C, 0xEF,
    0xA0, 0xE0, 0x3B, 0x4D, 0xAE, 0x2A, 0xF5, 0xB0,
    0xC8, 0xEB, 0xBB, 0x3C, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2B, 0x04, 0x7E, 0xBA, 0x77, 0xD6, 0x26,
    0xE1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0C, 0x7D
};

const unsigned int AES::m_Rcon[10] = {
    0x01000000, 0x02000000, 0x04000000, 0x08000000,
    0x10000000, 0x20000000, 0x40000000, 0x80000000,
    0x1B000000, 0x36000000
};

AES::AES()
{
    //Initialize the member variables to default values
    m_CipherMode = MODE_ECB;
    m_Rounds = 0;
    clear();
}

AES::AES(const char* key, KeySize keySize, CipherMode mode, const char* iv)
{
    //Set up the cipher with the specified parameters
    setup(key, keySize, mode, iv);
}

AES::~AES()
{
    //Erase any sensitive information
    clear();
}

void AES::setup(const char* key, KeySize keySize, CipherMode mode, const char* iv)
{
    //Set the cipher mode
    m_CipherMode = mode;

    //Determine the number of rounds from the key size
    m_Rounds = (int)keySize + 6;

    //Check if the key pointer is NULL
    if (key == NULL) {
        //Set a blank key
        memset(m_Key, 0, sizeof(m_Key));
    } else {
        //Expand the key
        expandKey(key, keySize);
    }

    //Check if the initialization vector pointer is NULL
    if (iv == NULL) {
        //Set a blank initialization vector
        memset(m_CarryVector, 0, 16);
    } else {
        //Copy the initialization vector to the carry vector
        memcpy(m_CarryVector, iv, 16);
    }
}

void AES::encrypt(void* data, size_t length)
{
    //Encrypt the specified data in-place
    encrypt(data, (char*)data, length);
}

void AES::encrypt(const void* src, char* dest, size_t length)
{
    //Convert the source pointer for byte access
    const char* srcBytes = (const char*)src;

    //Check if the length is less than 1 block
    if (length > 0 && length < 16) {
        //Copy the partial source block to the state array
        memcpy(m_State, srcBytes, length);

        //Pad the state array with zeroes
        memset(m_State + length, 0, 16 - length);

        //Perform CBC pre-processing if necessary
        if (m_CipherMode == MODE_CBC) {
            //XOR the state array with the carry vector
            for (int i = 0; i < 16; i++) {
                m_State[i] = m_State[i] ^ m_CarryVector[i];
            }
        }

        //Encrypt the state array
        aesEncrypt();

        //Perform CBC post-processing if necessary
        if (m_CipherMode == MODE_CBC) {
            //Save the state array as the next carry vector
            memcpy(m_CarryVector, m_State, 16);
        }

        //Copy the state array to the destination block
        memcpy(dest, m_State, 16);
        return;
    }

    //Encrypt all of the data
    while (length > 0) {
        //Copy the next source block to the state array
        memcpy(m_State, srcBytes, 16);
        srcBytes += 16;
        length -= 16;

        //Perform CBC pre-processing if necessary
        if (m_CipherMode == MODE_CBC) {
            //XOR the state array with the carry vector
            for (int i = 0; i < 16; i++) {
                m_State[i] = m_State[i] ^ m_CarryVector[i];
            }
        }

        //Encrypt the state array
        aesEncrypt();

        //Perform CBC post-processing if necessary
        if (m_CipherMode == MODE_CBC) {
            //Save the state array as the next carry vector
            memcpy(m_CarryVector, m_State, 16);
        }

        //Perform ciphertext stealing if the next block is a partial block
        if (length > 0 && length < 16) {
            //Copy the last partial source block to a temporary buffer (in case of in-place encryption)
            char temp[16];
            memcpy(temp, srcBytes, length);

            //Copy the leading bytes of the state array to the last partial destination block
            memcpy(dest + 16, m_State, length);

            //Copy the temporary buffer to the state array
            memcpy(m_State, temp, length);

            //Perform CBC processing if necessary
            if (m_CipherMode == MODE_CBC) {
                //Pad the state array with zeroes
                memset(m_State + length, 0, 16 - length);

                //XOR the state array with the carry vector
                for (int i = 0; i < 16; i++) {
                    m_State[i] = m_State[i] ^ m_CarryVector[i];
                }
            }

            //Encrypt the state array
            aesEncrypt();
            length = 0;
        }

        //Copy the state array to the destination block
        memcpy(dest, m_State, 16);
        dest += 16;
    }
}

void AES::decrypt(void* data, size_t length)
{
    //Decrypt the specified data in-place
    decrypt((const char*)data, data, length);
}

void AES::decrypt(const char* src, void* dest, size_t length)
{
    //Convert the destination pointer for byte access
    char* destBytes = (char*)dest;

    //Check if the length is less than 1 block
    if (length > 0 && length < 16) {
        //Copy the complete source block to the state array
        memcpy(m_State, src, 16);

        //Decrypt the state array
        aesDecrypt();

        //Perform CBC processing if necessary
        if (m_CipherMode == MODE_CBC) {
            //XOR the state array with the carry vector
            for (int i = 0; i < 16; i++) {
                m_State[i] = m_State[i] ^ m_CarryVector[i];
            }

            //Save the source block as the next carry vector
            memcpy(m_CarryVector, src, 16);
        }

        //Copy the leading bytes of the state array to the destination block
        memcpy(destBytes, m_State, length);
        return;
    }

    //Encrypt all of the data
    while (length > 0) {
        //Copy the next source block to the state array
        memcpy(m_State, src, 16);
        src += 16;
        length -= 16;

        //Decrypt the state array
        aesDecrypt();

        //Reverse ciphertext stealing if the next block is a partial block
        if (length > 0 && length < 16) {
            //Perform CBC processing if necessary
            if (m_CipherMode == MODE_CBC) {
                //XOR the state array with the last partial source block
                for (uint32_t i = 0; i < length; i++) {
                    m_State[i] = m_State[i] ^ src[i];
                }
            }

            //Copy the last partial source block to a temporary buffer (in case of in-place decryption)
            char temp[16];
            memcpy(temp, src, length);

            //Copy the leading bytes of the state array to the last partial destination block
            memcpy(destBytes + 16, m_State, length);

            //Copy the temporary buffer to the state array
            memcpy(m_State, temp, length);

            //Decrypt the state array
            aesDecrypt();
            length = 0;
        }

        //Perform CBC processing if necessary
        if (m_CipherMode == MODE_CBC) {
            //XOR the state array with the carry vector
            for (int i = 0; i < 16; i++) {
                m_State[i] = m_State[i] ^ m_CarryVector[i];
            }

            //Save the source block as the next carry vector
            memcpy(m_CarryVector, src - 16, 16);
        }

        //Copy the state array to the destination block
        memcpy(destBytes, m_State, 16);
        destBytes += 16;
    }
}

void AES::clear()
{
    //Erase the key, state array, and carry vector
    memset(m_Key, 0, sizeof(m_Key));
    memset(m_State, 0, sizeof(m_State));
    memset(m_CarryVector, 0, sizeof(m_CarryVector));
}

void AES::aesEncrypt()
{
    addRoundKey(0);
    for (int r = 1; r < m_Rounds; r++) {
        subBytes();
        shiftRows();
        mixColumns();
        addRoundKey(r);
    }
    subBytes();
    shiftRows();
    addRoundKey(m_Rounds);
}

void AES::aesDecrypt()
{
    addRoundKey(m_Rounds);
    for (int r = m_Rounds - 1; r > 0; r--) {
        invShiftRows();
        invSubBytes();
        addRoundKey(r);
        invMixColumns();
    }
    invShiftRows();
    invSubBytes();
    addRoundKey(0);
}

void AES::expandKey(const char* key, int nk)
{
    unsigned int temp;
    int i = 0;

    while(i < nk) {
        m_Key[i] = (key[4*i] << 24) + (key[4*i+1] << 16) + (key[4*i+2] << 8) + key[4*i+3];
        i++;
    }
    i = nk;
    while(i < 4*(m_Rounds+1)) {
        temp = m_Key[i-1];
        if(i % nk == 0)
            temp = subWord(rotWord(temp)) ^ m_Rcon[i/nk-1];
        else if(nk > 6 && i % nk == 4)
            temp = subWord(temp);
        m_Key[i] = m_Key[i-nk] ^ temp;
        i++;
    }
}

unsigned int AES::rotWord(unsigned int w)
{
    return (w << 8) + (w >> 24);
}

unsigned int AES::invRotWord(unsigned int w)
{
    return (w >> 8) + (w << 24);
}

unsigned int AES::subWord(unsigned int w)
{
    unsigned int out = 0;
    for(int i = 0; i < 4; ++i) {
        char temp = (w & 0xFF);
        out |= (m_Sbox[temp] << (8*i));
        w = (w >> 8);
    }
    return out;
}

void AES::subBytes()
{
    for(int i = 0; i < 16; ++i)
        m_State[i] = m_Sbox[m_State[i]];
}

void AES::invSubBytes()
{
    for(int i = 0; i < 16; ++i)
        m_State[i] = m_InvSbox[m_State[i]];
}

void AES::shiftRows()
{
    for(int r = 0; r < 4; ++r) {
        unsigned int temp = (m_State[r] << 24) + (m_State[r+4] << 16) + (m_State[r+8] << 8) + m_State[r+12];
        int i = r;
        while(i > 0) {
            temp = rotWord(temp);
            --i;
        }
        m_State[r] = temp >> 24;
        m_State[r+4] = temp >> 16;
        m_State[r+8] = temp >> 8;
        m_State[r+12] = temp;
    }
}

void AES::invShiftRows()
{
    for(int r = 0; r < 4; ++r) {
        unsigned int temp = (m_State[r] << 24) + (m_State[r+4] << 16) + (m_State[r+8] << 8) + m_State[r+12];
        int i = r;
        while(i > 0) {
            temp = invRotWord(temp);
            --i;
        }
        m_State[r] = temp >> 24;
        m_State[r+4] = temp >> 16;
        m_State[r+8] = temp >> 8;
        m_State[r+12] = temp;
    }
}

char AES::gmul(char a, char b)
{
    char p = 0;
    char counter;
    char carry;
    for (counter = 0; counter < 8; counter++) {
        if (b & 1)
            p ^= a;
        carry = (a & 0x80);
        a <<= 1;
        if (carry)
            a ^= 0x001B;
        b >>= 1;
    }
    return p;
}

void AES::mul(char* r)
{
    char tmp[4] = {};
    memcpy(tmp, r, 4);
    r[0] = gmul(tmp[0],2) ^ gmul(tmp[1],3) ^ tmp[2] ^ tmp[3];
    r[1] = tmp[0] ^ gmul(tmp[1],2) ^ gmul(tmp[2],3) ^ tmp[3];
    r[2] = tmp[0] ^ tmp[1] ^ gmul(tmp[2],2) ^ gmul(tmp[3],3);
    r[3] = gmul(tmp[0],3) ^ tmp[1] ^ tmp[2] ^ gmul(tmp[3],2);
}

void AES::invMul(char* r)
{
    char tmp[4] = {};
    memcpy(tmp, r, 4);
    r[0] = gmul(tmp[0],0x0e) ^ gmul(tmp[1],0x0b) ^ gmul(tmp[2],0x0d) ^ gmul(tmp[3],9);
    r[1] = gmul(tmp[0],9) ^ gmul(tmp[1],0x0e) ^ gmul(tmp[2],0x0b) ^ gmul(tmp[3],0x0d);
    r[2] = gmul(tmp[0],0x0d) ^ gmul(tmp[1],9) ^ gmul(tmp[2],0x0e) ^ gmul(tmp[3],0x0b);
    r[3] = gmul(tmp[0],0x0b) ^ gmul(tmp[1],0x0d) ^ gmul(tmp[2],9) ^ gmul(tmp[3],0x0e);
}

void AES::mixColumns()
{
    for(int c = 0; c < 4; ++c)
        mul(&m_State[4*c]);
}

void AES::invMixColumns()
{
    for(int c = 0; c < 4; ++c)
        invMul(&m_State[4*c]);
}

void AES::addRoundKey(int round)
{
    for(int c = 0; c < 4; ++c) {
        unsigned int temp = (m_State[4*c] << 24) + (m_State[4*c+1] << 16) + (m_State[4*c+2] << 8) + m_State[4*c+3];
        temp ^= m_Key[round*4+c];
        m_State[4*c] = temp >> 24;
        m_State[4*c+1] = temp >> 16;
        m_State[4*c+2] = temp >> 8;
        m_State[4*c+3] = temp;
    }
}
