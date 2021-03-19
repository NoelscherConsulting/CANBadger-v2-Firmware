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

#ifndef AES_H
#define AES_H

#include "mbed.h"

/** AES class.
 *  Used for encrypting/decrypting data using the AES block cipher.
 *
 * Example:
 * @code
 * #include "mbed.h"
 * #include "AES.h"
 *
 * char message[] = {
 *     "Hello World!"
 * };
 *
 * const char key[32] = {
 *     0x60, 0x3D, 0xEB, 0x10, 0x15, 0xCA, 0x71, 0xBE,
 *     0x2B, 0x73, 0xAE, 0xF0, 0x85, 0x7D, 0x77, 0x81,
 *     0x1F, 0x35, 0x2C, 0x07, 0x3B, 0x61, 0x08, 0xD7,
 *     0x2D, 0x98, 0x10, 0xA3, 0x09, 0x14, 0xDF, 0xF4
 * };
 *
 * const char iv[16] = {
 *     0x74, 0x11, 0xF0, 0x45, 0xD6, 0xA4, 0x3F, 0x69,
 *     0x18, 0xC6, 0x75, 0x42, 0xDF, 0x4C, 0xA7, 0x84
 * };
 *
 * void printData(const void* data, size_t length)
 * {
 *     const char* dataBytes = (const char*)data;
 *     for (size_t i = 0; i < length; i++) {
 *         if ((i % 8) == 0)
 *             printf("\n\t");
 *         printf("0x%02X, ", dataBytes[i]);
 *     }
 *     printf("\n");
 * }
 *
 * int main()
 * {
 *     AES aes;
 *
 *     //Print the original message
 *     printf("Original message: \"%s\"", message);
 *     printData(message, sizeof(message));
 *
 *     //Encrypt the message in-place
 *     aes.setup(key, AES::KEY_256, AES::MODE_CBC, iv);
 *     aes.encrypt(message, sizeof(message));
 *     aes.clear();
 *
 *     //Print the encrypted message
 *     printf("Encrypted message:");
 *     printData(message, sizeof(message));
 *
 *     //Decrypt the message in-place
 *     aes.setup(key, AES::KEY_256, AES::MODE_CBC, iv);
 *     aes.decrypt(message, sizeof(message));
 *     aes.clear();
 *
 *     //Print the decrypted message
 *     printf("Decrypted message: \"%s\"", message);
 *     printData(message, sizeof(message));
 * }
 * @endcode
 */
class AES
{
public:
    /** Represents the different AES key sizes
     */
    enum KeySize {
        KEY_128 = 4,    /**< 128-bit AES key */
        KEY_192 = 6,    /**< 192-bit AES key */
        KEY_256 = 8     /**< 256-bit AES key */
    };

    /** Represents the different cipher modes
     */
    enum CipherMode {
        MODE_ECB,   /**< Electronic codebook */
        MODE_CBC    /**< Cipher block chaining */
    };

    /** Create a blank AES object
     */
    AES();

    /** Create an AES object with the specified parameters
     *
     * @param key Pointer to the AES key.
     * @param keySize The AES key size as a KeySize enum.
     * @param mode The cipher mode as a CipherMode enum (MODE_ECB by default).
     * @param iv Pointer to the 16B initialization vector (NULL by default).
     */
    AES(const char* key, KeySize keySize, CipherMode mode = MODE_ECB, const char* iv = NULL);

    /** Destructor
     */
    ~AES();

    /** Set up this AES object for encryption/decryption with the specified parameters
     *
     * @param key Pointer to the AES key.
     * @param keySize The AES key size as a KeySize enum.
     * @param mode The cipher mode as a CipherMode enum (MODE_ECB by default).
     * @param iv Pointer to the 16B initialization vector (NULL by default).
     */
    void setup(const char* key, KeySize keySize, CipherMode mode = MODE_ECB, const char* iv = NULL);

    /** Encrypt the specified data in-place, using CTS or zero-padding if necessary
     *
     * @param data Pointer to the data to encrypt (minimum 16B for output).
     * @param length The length of the data to encrypt in bytes.
     */
    void encrypt(void* data, size_t length);

    /** Encrypt the specified data, using CTS or zero-padding if necessary
     *
     * @param src Pointer to the data to encrypt.
     * @param dest Pointer to an array in which to store the encrypted data (minimum 16B).
     * @param length The length of the data to encrypt in bytes.
     */
    void encrypt(const void* src, char* dest, size_t length);

    /** Decrypt the specified data in-place, and remove the padding if necessary
     *
     * @param data Pointer to the data to decrypt (minimum 16B).
     * @param length The length of the decrypted data without padding in bytes.
     */
    void decrypt(void* data, size_t length);

    /** Decrypt the specified data, and remove the padding if necessary
     *
     * @param src Pointer to the data to decrypt (minimum 16B).
     * @param dest Pointer to an array in which to store the decrypted data.
     * @param length The length of the decrypted data without padding in bytes.
     */
    void decrypt(const char* src, void* dest, size_t length);

    /** Erase any sensitive information in this AES object
     */
    void clear();

private:
    //Member variables
    static const char m_Sbox[256];
    static const char m_InvSbox[256];
    static const unsigned int m_Rcon[10];
    AES::CipherMode m_CipherMode;
    int m_Rounds;
    unsigned int m_Key[60];
    char m_State[16];
    char m_CarryVector[16];

    //Internal methods
    void aesEncrypt();
    void aesDecrypt();
    void expandKey(const char* key, int nk);
    unsigned int rotWord(unsigned int w);
    unsigned int invRotWord(unsigned int w);
    unsigned int subWord(unsigned int w);
    void subBytes();
    void invSubBytes();
    void shiftRows();
    void invShiftRows();
    char gmul(char a, char b);
    void mul(char* r);
    void invMul(char* r);
    void mixColumns();
    void invMixColumns();
    void addRoundKey(int round);
};

#endif
