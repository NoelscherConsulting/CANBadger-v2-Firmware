/*
* KWP2000 SIDs HEADER FILE
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


//KWP2K SIDS
#define KWP_START_DIAG_SESSION 0x10
#define KWP_ECU_RESET 0x11
#define KWP_READ_FREEZE_FRAME_DATA 0x12
#define KWP_READ_DTC 0x13
#define KWP_CLEAR_DTC 0x14
#define KWP_READ_DTC_STATUS 0x17
#define KWP_READ_DTC_BY_STATUS 0x18
#define KWP_READ_ECU_ID 0x1A
#define KWP_STOP_DIAG_SESSION 0x20
#define KWP_READ_DATA_BY_LOCAL_ID 0x21
#define KWP_READ_DATA_BY_COMMON_ID 0x22
#define KWP_READ_MEMORY_BY_ADDRESS 0x23
#define KWP_STOP_REPEATED_DATA_TRANSMISSION 0x25
#define KWP_SET_DATA_RATES 0x26
#define KWP_SECURITY_ACCESS 0x27
#define KWP_DINAMICALLY_DEFINE_LOCAL_ID 0x2C
#define KWP_WRITE_DATA_BY_COMMON_ID 0x2E
#define KWP_IO_CONTROL_BY_COMMON_ID 0x2F
#define KWP_IO_CONTROL_BY_LOCAL_ID 0x30
#define KWP_START_ROUTINE_BY_LOCAL_ID 0x31
#define KWP_STOP_ROUTINE_BY_LOCAL_ID 0x32
#define KWP_REQUEST_ROUTINE_RESULTS_BY_LOCAL_ID 0x33
#define KWP_REQUEST_DOWNLOAD 0x34
#define KWP_REQUEST_UPLOAD 0x35
#define KWP_TRANSFER_DATA 0x36
#define KWP_REQUEST_TRANSFER_EXIT 0x37
#define KWP_START_ROUTINE_BY_ADDRESS 0x38
#define KWP_STOP_ROUTINE_BY_ADDRESS 0x39
#define KWP_REQUEST_ROUTINE_RESULTS_BY_ADDRESS 0x3A
#define KWP_WRITE_DATA_BY_LOCAL_ID 0x3B
#define KWP_WRITE_MEMORY_BY_ADDRESS 0x3D
#define KWP_TESTER_PRESENT 0x3E
#define KWP_RESPONSE_OFFSET 0x40
#define KWP_START_COMMUNICATIONS 0x81
#define KWP_STOP_COMMUNICATIONS 0x82
#define KWP_ACCESS_TIMING_PARAMETERS 0x83
#define KWP_START_PROGRAMMING_MODE 0x85

//KWP ERROR CODES
#define KWP_GENERAL_REJECT 0x10
#define KWP_SERVICE_NOT_SUPPORTED 0x11
#define KWP_SUBFUNCTION_NOT_SUPPORTED 0x12
#define KWP_BUSY_REPEAT_REQUEST 0x21
#define KWP_CONDITIONS_NOT_CORRECT 0x22 //ALSO MEANS REQUEST SEQUENCE ERROR
#define KWP_ROUTINE_NOT_COMPLETE 0x23
#define KWP_REQUEST_OUT_OF_RANGE 0x31
#define KWP_SECURITY_ACCESS_DENIED 0x33
#define KWP_INVALID_KEY 0x35
#define KWP_EXCEEDED_NUMBER_OF_ATTEMPTS 0x36
#define KWP_REQUIRED_TIME_DELAY_NOT_EXPIRED 0x37
#define KWP_DOWNLOAD_NOT_ACCEPTED 0x40
#define KWP_IMPROPER_DOWNLOAD_TYPE 0x41
#define KWP_CANNOT_DOWNLOAD_TO_SPECIFIED_ADDRESS 0x42
#define KWP_CANNOT_DOWNLOAD_NUMBER_OF_BYTES_REQUESTED 0x43
#define KWP_UPLOAD_NOT_ACCEPTED 0x50
#define KWP_IMPROPER_UPLOAD_TYPE 0x51
#define KWP_CANNOT_UPLOAD_FROM_SPECIFIED_ADDRESS 0x52
#define KWP_CANNOT_UPLOAD_NUMBER_OF_BYTES_REQUESTED 0x53
#define KWP_TRANSFER_SUSPENDED 0x71
#define KWP_TRANSFER_ABORTED 0x72
#define KWP_ILLEGAL_ADDRESS_IN_BLOCK_TRANSFER 0x74
#define KWP_ILLEGAL_BYTE_COUNT_IN_BLOCK_TRANSFER 0x75
#define KWP_ILLEGAL_BLOCK_TRANSFER_TYPE 0x76
#define KWP_BLOCK_TRANSFER_DATA_CHECKSUM_ERROR 0x77
#define KWP_RESPONSE_PENDING 0x78
#define KWP_INCORRECT_BYTE_COUNT_DURING_TRANSFER 0x79
#define KWP_SERVICE_NOT_SUPPORTED_IN_ACTIVE_DIAG_MODE 0x80


//Tester Present
#define KWP_TESTER_PRESENT_REPLY_REQUIRED 0x01
#define KWP_TESTER_PRESENT_NO_REPLY_REQUIRED 0x02


//KWP Diagnostics sessions
#define KWP_DEFAULT_SESSION 0x81
#define KWP_EOL_MANUFACTURER 0x83
#define KWP_EOL_SYSTEM_SUPPLIER 0x84
#define KWP_ECU_PROGRAMMING_MODE 0x85
#define KWP_COMPONENT_STARTING 0x89


//KWP READ ECU ID PARAMETERS
#define KWP_ECUID_ID_CODE 0x80
#define KWP_ECUID_VIN_HW 0x86											
#define KWP_ECUID_BOOT_VERSION 0x8A			
#define KWP_ECUID_VIN_SW 0x90	
#define KWP_ECUID_DRAWING_NO 0x91
#define KWP_ECUID_HW_NO 0x92
#define KWP_ECUID_HW_VERSION_NO 0x93
#define KWP_ECUID_SW_NO 0x94
#define KWP_ECUID_SW_VERSION_NO 0x95
#define KWP_ECUID_HOMOLOGATION_CODE 0x96
#define KWP_ECUID_ISO_CODE 0x97
#define KWP_ECUID_TESTER_CODE 0x98
#define KWP_ECUID_REPROG_PROD_DATE 0x99
#define KWP_ECUID_CONFIG_SW_VERSION 0x9B


//KWP ACCESS TIMING PARAMETERS IDENTIFIERS
#define KWP_ATP_READ_LIMITS 0x00
#define KWP_ATP_RESET 0x01
#define KWP_ATP_READ_CURRENT 0x02
#define KWP_ATP_SET 0x03


//KWP ROUTINE BY LOCAL ID
//ROUTINE EXIT OPTIONS
//80-FF ARE MANUFACTURER SPECIFIC. REST IS RESERVED
#define KWP_NORMAL_EXIT_WITH_RESULTS_AVAILABLE 0x61
#define KWP_NORMAL_EXIT_WITHOUT_RESULTS_AVAILABLE 0x62
#define KWP_ABNORMAL_EXIT_WITH_RESULTS_AVAILABLE 0x63
#define KWP_ABNORMAL_EXIT_WITHOUT_RESULTS_AVAILABLE 0x64



//KWP TRANSMISSION MODE
#define KWP_TRANSMISSION_MODE_SINGLE 0x01
#define KWP_TRANSMISSION_MODE_SLOW 0x02
#define KWP_TRANSMISSION_MODE_MEDIUM 0x03
#define KWP_TRANSMISSION_MODE_FAST 0x04
#define KWP_TRANSMISSION_MODE_STOP 0x05


//KWP ECU RESET
//80-FF ARE MANUFACTURER SPECIFIC
#define KWP_RESET_POWER_ON 0x01 //basically cycling ignition
#define KWP_RESET_POWER_ON_WHILE_MAINTAINING_COMMS 0x02

//KWP TRANSFER DATA
#define KWP_TRANSFER_DATA_BLOCK_COMPLETE 0x73
