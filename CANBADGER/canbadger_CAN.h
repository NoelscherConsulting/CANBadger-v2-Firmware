/*
* CanBadger CAN library
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

#ifndef __CANBADGER_CAN_H__
#define __CANBADGER_CAN_H__

typedef enum _CAN_FILTER_TYPE {
  CAN_FILTER_TYPE_EXACT_ID = 0U,
  CAN_FILTER_TYPE_RANGE_ID = 1U
} CAN_FILTER_TYPE;

enum  	ARM_CAN_FILTER_OPERATION {
  ARM_CAN_FILTER_ID_EXACT_ADD,
  ARM_CAN_FILTER_ID_EXACT_REMOVE,
  ARM_CAN_FILTER_ID_RANGE_ADD,
  ARM_CAN_FILTER_ID_RANGE_REMOVE,
  ARM_CAN_FILTER_ID_MASKABLE_ADD,
  ARM_CAN_FILTER_ID_MASKABLE_REMOVE
};


#define 	ARM_CAN_ID_IDE_Pos   31UL
#define 	ARM_CAN_ID_IDE_Msk   (1UL << ARM_CAN_ID_IDE_Pos)

#define CANAF_ENDofTable_ENDofTable_Pos (            2U)
#define CANAF_ENDofTable_ENDofTable_Msk (0x3FFU  <<  CANAF_ENDofTable_ENDofTable_Pos)
#define CANAF_ENDofTable_ENDofTable(x)  (((x)    <<  CANAF_ENDofTable_ENDofTable_Pos) & CANAF_ENDofTable_ENDofTable_Msk)

// CAN Acceptance Filter Registers bit definitions
#define CANAF_AFMR_AccOff               (1U            )
#define CANAF_AFMR_AccBP                (1U      <<  1U)
#define CANAF_AFMR_eFCANf               (1U      <<  2U)

#define CANAF_SFF_sa_SFF_sa_Pos         (            2U)
#define CANAF_SFF_sa_SFF_sa_Msk         (0x1FFU  <<  CANAF_SFF_sa_SFF_sa_Pos)
#define CANAF_SFF_sa_SFF_sa(x)          (((x)    <<  CANAF_SFF_sa_SFF_sa_Pos) & CANAF_SFF_sa_SFF_sa_Msk)

#define CANAF_SFF_GRP_sa_SFF_GRP_sa_Pos (            2U)
#define CANAF_SFF_GRP_sa_SFF_GRP_sa_Msk (0x3FFU  <<  CANAF_SFF_GRP_sa_SFF_GRP_sa_Pos)
#define CANAF_SFF_GRP_sa_SFF_GRP_sa(x)  (((x)    <<  CANAF_SFF_GRP_sa_SFF_GRP_sa_Pos) & CANAF_SFF_GRP_sa_SFF_GRP_sa_Msk)

#define CANAF_EFF_sa_EFF_sa_Pos         (            2U)
#define CANAF_EFF_sa_EFF_sa_Msk         (0x1FFU  <<  CANAF_EFF_sa_EFF_sa_Pos)
#define CANAF_EFF_sa_EFF_sa(x)          (((x)    <<  CANAF_EFF_sa_EFF_sa_Pos) & CANAF_EFF_sa_EFF_sa_Msk)

#define CANAF_EFF_GRP_sa_Eff_GRP_sa_Pos (            2U)
#define CANAF_EFF_GRP_sa_Eff_GRP_sa_Msk (0x3FFU  <<  CANAF_EFF_GRP_sa_Eff_GRP_sa_Pos)
#define CANAF_EFF_GRP_sa_Eff_GRP_sa(x)  (((x)    <<  CANAF_EFF_GRP_sa_Eff_GRP_sa_Pos) & CANAF_EFF_GRP_sa_Eff_GRP_sa_Msk)

#define CANAF_ENDofTable_ENDofTable_Pos (            2U)
#define CANAF_ENDofTable_ENDofTable_Msk (0x3FFU  <<  CANAF_ENDofTable_ENDofTable_Pos)
#define CANAF_ENDofTable_ENDofTable(x)  (((x)    <<  CANAF_ENDofTable_ENDofTable_Pos) & CANAF_ENDofTable_ENDofTable_Msk)

#define CANAF_LUTerrAd_LUTerrAd_Pos     (            2U)
#define CANAF_LUTerrAd_LUTerrAd_Msk     (0x1FFU  <<  CANAF_LUTerrAd_LUTerrAd_Pos)
#define CANAF_LUTerrAd_LUTerrAd(x)      (((x)    <<  CANAF_LUTerrAd_LUTerrAd_Pos) & CANAF_LUTerrAd_LUTerrAd_Msk)

#define CANAF_LUTerr_LUTerr             (1U            )

#define CANAF_FCANIE_FCANIE             (1U            )

#define CANAF_FCANIC0_IntPnd(x)         (1U      <<  (x)     )

#define CANAF_FCANIC1_IntPnd(x)         (1U      << ((x)-32U))



#include "mbed.h"

class CANbadger_CAN
{
	public:
		
				CANbadger_CAN(CAN *canbus);
	
				~CANbadger_CAN();
	

        /** Sends a CAN frame on the specified bus.
            @param frameFormat determines if the frame is a Standard (CANStandard) or an extended (CANExtended) frame
						@param frameType determines if the frame is a Data frame (CANData) or a Remote frame (CANRemote) frame
						
						@return TRUE if the frame was sent within the required timeout, FALSE if it wasnt.
						
				*/
				bool sendCANFrame(uint32_t msgID, uint8_t *payload, uint8_t len, CANFormat frameFormat = CANStandard, CANType frameType = CANData, uint32_t timeout = 10);

				/** Retrieves a CAN frame on the specified bus.
						@param msgID is used to tell the function to look for a specific CAN ID or to any (when value is set to zero), and in the return of the function contains the CAN ID of the captured frame. This parameter must be passed as the variable address (preceded by &)
						@param frameFormat determines if the frame is a Standard (CANStandard) or an extended (CANExtended) frame
						@param frameType determines if the frame is a Data frame (CANData) or a Remote frame (CANRemote) frame
						
						@return the length of the captured frame, zero if no frame was captured.
						
				*/
				uint8_t getCANFrame(uint32_t msgID, uint8_t *payload, uint32_t timeout = 10);


				/**
					\fn          int32_t CANx_AddFilter (CAN_FILTER_TYPE filter_type, uint32_t id, uint32_t id_range_end, uint8_t x)
					\brief       Add receive filter for specified id or id range.
					\param[in]   filter_type  Type of filter to add
												 - CAN_FILTER_TYPE_EXACT_ID: exact id filter (id only)
												 - CAN_FILTER_TYPE_RANGE_ID: range id filter (id of range start and id of range end)
					\param[in]   id           Exact identifier or identifier of range start
					\param[in]   id_range_end Identifier of range end
					\param[in]   x            Controller number (0..1)
					\return      execution status
				*/

				bool CANx_AddFilter (CAN_FILTER_TYPE filter_type, uint32_t id, uint32_t id_range_end, uint8_t x=1);//CMSIS code, pretty useful

				/**
					\fn          int32_t CANx_RemoveFilter (CAN_FILTER_TYPE filter_type, uint32_t id, uint32_t id_range_end, uint8_t x)
					\brief       Remove receive filter for specified id or id range.
					\param[in]   filter_type  Type of filter to remove
												 - CAN_FILTER_TYPE_EXACT_ID: exact id filter (id only)
												 - CAN_FILTER_TYPE_RANGE_ID: range id filter (id of range start and id of range end)
					\param[in]   id           Exact identifier or identifier of range start
					\param[in]   id_range_end Identifier of range end
					\param[in]   x            Controller number (0..1)
					\return      execution status
				*/

				bool CANx_RemoveFilter (CAN_FILTER_TYPE filter_type, uint32_t id, uint32_t id_range_end, uint8_t x=1);

				/**
					\fn          int32_t CANx_ObjectSetFilter (uint32_t obj_idx, ARM_CAN_FILTER_OPERATION operation, uint32_t id, uint32_t arg, uint8_t x)
					\brief       Add or remove filter for message reception.
					\param[in]   obj_idx      Object index of object that filter should be or is assigned to
					\param[in]   operation    Operation on filter
												 - ARM_CAN_FILTER_ID_EXACT_ADD :       add    exact id filter
												 - ARM_CAN_FILTER_ID_EXACT_REMOVE :    remove exact id filter
												 - ARM_CAN_FILTER_ID_RANGE_ADD :       add    range id filter
												 - ARM_CAN_FILTER_ID_RANGE_REMOVE :    remove range id filter
												 - ARM_CAN_FILTER_ID_MASKABLE_ADD :    add    maskable id filter
												 - ARM_CAN_FILTER_ID_MASKABLE_REMOVE : remove maskable id filter
					\param[in]   id           ID or start of ID range (depending on filter type)
					\param[in]   arg          Mask or end of ID range (depending on filter type)
					\param[in]   x            Controller number (0..1)
					\return      execution status
				*/


				bool CANx_ObjectSetFilter (ARM_CAN_FILTER_OPERATION operation, uint32_t id, uint32_t arg, uint8_t y);

		        /** Used to disable the CAN filters. This will make the CAN PHY accept incoming frames from all IDs

				*/
				void disableCANFilters();//disables CAN filters

		        /** Used to enable the CAN filters. This will make the CAN PHY only accept IDs from the set filters.
						If no filters are set, then no IDs will be accepted

				*/
				void enableCANFilters(); // enables CAN filters


				bool available(uint8_t interfaceNo=1);//returns true if there are pending frames to be read



				/** Puts or removes the CAN interface into silent monitoring mode. This is useful for non-MITM logging without sending ACK to frames
				 
						@param silent boolean indicating whether to go into silent mode or not
				 */				
				void CANMonitorMode(bool silent = false);
				
				/** Sets the CAN interface to the specified speed
				 
						@return True if the operation was performed, false if it was not.
				 */				

				bool setCANSpeed(uint32_t speed);
				
				

				/** Tries to detect the speed

						@return 0 if it was not found, otherwise it will return the detected baudrate
				 */
				uint32_t detectSpeed();


				uint32_t getIDsList(uint32_t *idList);//generates a list of active CAN IDs so they can be filtered. returns the number of IDs it got


				private:
						
				CAN* _canbus;	
				
				
};







#endif
				
				
