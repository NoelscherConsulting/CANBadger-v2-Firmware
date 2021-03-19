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

#include "canbadger_CAN.h"



CANbadger_CAN::CANbadger_CAN(CAN *canbus)
{
	_canbus=canbus;
}

CANbadger_CAN::~CANbadger_CAN()
{
}


bool CANbadger_CAN::available(uint8_t interfaceNo)
{
	if(interfaceNo == 1)
	{
		if((LPC_CAN1->GSR & 1) == 0)
		{
			return false;
		}
		return true;
	}
	else
	{
		if((LPC_CAN2->GSR & 1) == 0)
		{
			return false;
		}
		return true;
	}
}

bool CANbadger_CAN::sendCANFrame(uint32_t msgID, uint8_t *payload, uint8_t len, CANFormat frameFormat, CANType frameType, uint32_t timeout)
{
	uint32_t currentMS=0;
	while(((LPC_CAN1->GSR & 4) == 0 || (LPC_CAN2->GSR & 4) == 0) && currentMS < (timeout * 10))//hack to fix a silicon bug.
	{
		wait(0.0001);
		currentMS++;
	}
	_canbus->write(CANMessage(msgID, reinterpret_cast<char*>(payload), len, frameType, frameFormat));
	if(timeout != 0)
	{
		while(((LPC_CAN1->GSR & 4) == 0 || (LPC_CAN2->GSR & 4) == 0) && currentMS < (timeout * 10))//hack to fix a silicon bug.
		{
			wait(0.0001);
			currentMS++;
		}
	}
	if(currentMS >= (timeout * 10) && timeout != 0)
	{
		return false;
	}
	return true;	
}

bool CANbadger_CAN::setCANSpeed(uint32_t speed)
{
	if(!_canbus->frequency(speed))
	{
		return 0;
	}
	return 1;
}

uint8_t CANbadger_CAN::getCANFrame(uint32_t msgID, uint8_t *payload, uint32_t timeout )
{
	CANMessage can_msg(0,CANAny);
	uint32_t currentMS=0; // used to measure against timeout
	bool gotFrame = false;
	if(msgID == 0)
	{
		while(currentMS <= (timeout* 10))
		{
			if(_canbus->read(can_msg) != 0)
			{
				gotFrame = true;
				break;
			}
			else
			{
				wait(0.0001);
				currentMS++;
			}
			if(currentMS >= (timeout* 10))
			{
				break;
			}		
		}

	}
	else
	{
		if(timeout == 0)
		{			
			while(1)
			{
				uint8_t w = _canbus->read(can_msg);
				if(w != 0)
				{
					if(can_msg.id == msgID)
					{
						gotFrame = true;
						break;
					}
				}
			}
		}
		else
		{
			while(currentMS <= (timeout* 10))
			{
				if(_canbus->read(can_msg) && can_msg.id == msgID)
				{
					gotFrame = true;
					break;
				}
				else
				{
					wait(0.0001);
					currentMS++;
				}
				if(currentMS >= (timeout* 10))
				{
					break;
				}
			}
		}		
	}			 
	if(gotFrame == false)
	{
		return 0;
	}
	for(uint8_t a = 0; a < can_msg.len; a++)
	{
		payload[a]=can_msg.data[a];
	}			 
	return can_msg.len;
}

uint32_t CANbadger_CAN::detectSpeed()
{
    uint32_t cnt=0;//for timeout
    for(uint8_t a=0; a<13; a++)
    {
      uint32_t speed;
      switch (a)
      {
		 case 0:
		 {
			 speed=20000;
			 break;
		 }
		 case 1:
		 {
			 speed=33333;
			 break;
		 }
		 case 2:
		 {
			 speed=40000;
			 break;
		 }
		 case 3:
		 {
			 speed=50000;
			 break;
		 }
		 case 4:
		 {
			 speed=62500;
			 break;
		 }
		 case 5:
		 {
			 speed=83333;
			 break;
		 }
		 case 6:
		 {
			 speed=100000;
			 break;
		 }
		 case 7:
		 {
			 speed=125000;
			 break;
		 }
		 case 8:
		 {
			 speed=250000;
			 break;
		 }
		 case 9:
		 {
			 speed=500000;
			 break;
		 }
		 case 10:
		 {
			 speed=750000;
			 break;
		 }
		 case 11:
		 {
			 speed=800000;
			 break;
		 }
		 case 12:
		 {
			 speed=1000000;
			break;
		 }
      }
      CANMessage can_msg(0,CANAny);
      _canbus->frequency(speed);
      while(_canbus->read(can_msg)){}//just to clear the buffer
	  while(!_canbus->read(can_msg) || can_msg.id == 0)
	  {
		wait(0.01);
		cnt++;
		if(cnt == 101)
		{
			break;
		}
	  }
      if(cnt<101)
      {
		return speed;
      }
      cnt=0;//reset the delay counter
  }
  _canbus->frequency(500000);
  return 0;
}



void CANbadger_CAN::CANMonitorMode(bool silent)
{
	_canbus->monitor(silent);
}


uint32_t CANbadger_CAN::getIDsList(uint32_t *idList)
{
	CANMessage can_msg(0,CANAny);
	uint32_t currentMS=0; // used to measure against timeout
	bool gotFrame = false;
	uint32_t timeout = 30000;//3 seconds
	while(currentMS <= timeout)//we will wait 3 seconds to get a frame
	{
		if(_canbus->read(can_msg) != 0)
		{
			gotFrame = true;
			break;
		}
		else
		{
			wait(0.0001);
			currentMS++;
		}
		if(currentMS == timeout)
		{
			break;
		}
	}
	if(gotFrame == false)//if there was no traffic
	{
		return 0;
	}
	currentMS=0;//we did get a frame
	uint32_t f_counter=0;//value to be returned, how many IDs were found
    while(currentMS < timeout)
    {
    	currentMS=0;//we did get a frame
    	uint32_t cid=can_msg.id;
    	uint8_t doit=1;
    	for(uint32_t cnt2=0;cnt2<f_counter;cnt2++)
    	{
    		if(idList[cnt2]==cid)
    		{
    			doit=0;
    			break;
    		}
    	}
    	if(doit==1 || f_counter==0)//need to also save the first ID!
    	{
    		idList[f_counter]=cid;
    		f_counter++;
    	}
    	while(_canbus->read(can_msg) == 0 && currentMS<timeout)
    	{
    		currentMS++;
    		wait(0.0001);
    	}
   }
   return f_counter;
}

void CANbadger_CAN::disableCANFilters()
{
	LPC_CANAF->AFMR = 0x00000002;//disable filters
}

void CANbadger_CAN::enableCANFilters()
{
	LPC_CANAF->AFMR = 0x00000000;//enable filters
}

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
bool CANbadger_CAN::CANx_RemoveFilter(CAN_FILTER_TYPE filter_type, uint32_t id, uint32_t id_range_end, uint8_t x)
{
  uint32_t  i, i_end;
  uint32_t  entry;

  if (x >= 2)
  {
	  return false;
  }

  if ((id & ARM_CAN_ID_IDE_Msk) == 0U) {                        // Standard Identifier (11 bit)
    switch (filter_type) {
      case CAN_FILTER_TYPE_EXACT_ID:                            // Exact
        // Entry in this section is 16 bits large
        if (LPC_CANAF->SFF_sa >= LPC_CANAF->SFF_GRP_sa) {
          // If this section does not contain any entries
          return 0;
        }

        // Find entry to be removed
        entry = (x << 13) | (id & 0x7FFU);
        i     =  LPC_CANAF->SFF_sa     / 4U;
        i_end = (LPC_CANAF->SFF_GRP_sa / 4U) - 1U;
        for (; i <= i_end; i++) {
          if ((LPC_CANAF_RAM->mask[i] >> 16) == entry) {        // If this is entry to be removed (in high 16 bits)
            // Move all remaining entries in this section 16 bits towards start of table
            for (; i < i_end; i++) {
              LPC_CANAF_RAM->mask[i] = (LPC_CANAF_RAM->mask[i] << 16) | (LPC_CANAF_RAM->mask[i+1U] >> 16);
            }
            // Insert disabled entry into lower 16 bits of last entry
            LPC_CANAF_RAM->mask[i] = (LPC_CANAF_RAM->mask[i] << 16) | (1U << 13) | (1U << 12) | (0x7FFU);
            break;
          } else if ((LPC_CANAF_RAM->mask[i] & 0xFFFFU) == entry) {   // If this is entry to be removed (in low 16 bits)
            // Move all remaining entries in this section 16 bits towards start of table
            if (i == i_end) {                                   // Corner case when requested entry is in lower 16 bits of last entry
              LPC_CANAF_RAM->mask[i]= (LPC_CANAF_RAM->mask[i] & 0xFFFF0000U) | (1U << 13) | (1U << 12) | (0x7FFU);
            } else {
              LPC_CANAF_RAM->mask[i]= (LPC_CANAF_RAM->mask[i] & 0xFFFF0000U) | (LPC_CANAF_RAM->mask[i+1U] >> 16);
              i++;
              for (; i < i_end; i++) {
                LPC_CANAF_RAM->mask[i] = (LPC_CANAF_RAM->mask[i] << 16) | (LPC_CANAF_RAM->mask[i+1U] >> 16);
              }
              // Disable lower 16 bits entry in last location in this section
              LPC_CANAF_RAM->mask[i] = (LPC_CANAF_RAM->mask[i] << 16) | (1U << 13) | (1U << 12) | (0x7FFU);
            }
            break;
          }
        }

        if (LPC_CANAF_RAM->mask[i] == ((1U << 29) | (1U << 28) | (0x7FFU << 16) | (1U << 13) | (1U << 12) | (0x7FFU))) {
          // If we should reduce this section (last 2 entries in 32 bits are both disabled)

          // Move all remaining entries in remaining sections 32 bits towards start of table
          i     = (LPC_CANAF->SFF_GRP_sa / 4U) - 1U;
          i_end =  LPC_CANAF->ENDofTable / 4U;
          for (; i < i_end; i++) {
            LPC_CANAF_RAM->mask[i] = LPC_CANAF_RAM->mask[i+1U];
          }

          // Clear last entry in table
          LPC_CANAF_RAM->mask[i+1U] = 0U;

          // Decrement affected pointers
          LPC_CANAF->ENDofTable -= 4U;
          LPC_CANAF->EFF_GRP_sa -= 4U;
          LPC_CANAF->EFF_sa     -= 4U;
          LPC_CANAF->SFF_GRP_sa -= 4U;
        }
        break;
      case CAN_FILTER_TYPE_RANGE_ID:                            // Range
        // Entry in this section is 32 bits large
        if (LPC_CANAF->SFF_GRP_sa >= LPC_CANAF->EFF_sa) {
          // If this section does not contain any entries
          return 0;
        }

        // Find entry to be removed
        entry = (x << 29) | ((id & 0x7FFU) << 16) | (x << 13) | (id_range_end & 0x7FFU);
        i     =  LPC_CANAF->SFF_GRP_sa / 4U;
        i_end = (LPC_CANAF->EFF_sa     / 4U) - 1U;
        for (; i <= i_end; i++) {
          if (LPC_CANAF_RAM->mask[i] == entry) {                // If this is entry to be removed
            // Move all remaining entries in this section 32 bits towards start of table
            for (; i < i_end; i++) {
              LPC_CANAF_RAM->mask[i] = LPC_CANAF_RAM->mask[i+1U];
            }
            // Move all remaining entries in remaining sections 32 bits towards start of table
            i     = (LPC_CANAF->EFF_sa     / 4U) - 1U;
            i_end =  LPC_CANAF->ENDofTable / 4U;
            for (; i < i_end; i++) {
              LPC_CANAF_RAM->mask[i] = LPC_CANAF_RAM->mask[i+1U];
            }

            // Clear last entry in table
            LPC_CANAF_RAM->mask[i+1U] = 0U;

            // Decrement affected pointers
            LPC_CANAF->ENDofTable -= 4U;
            LPC_CANAF->EFF_GRP_sa -= 4U;
            LPC_CANAF->EFF_sa     -= 4U;
            break;
          }
        }
        break;
    }
  } else {                                                      // Extended Identifier (29 bit)
    switch (filter_type) {
      case CAN_FILTER_TYPE_EXACT_ID:                            // Exact
        // Entry in this section is 32 bits large
        if (LPC_CANAF->EFF_sa >= LPC_CANAF->EFF_GRP_sa) {
          // If this section does not contain any entries
          return 0;
        }

        // Find entry to be removed
        entry = (x << 29) | (id & 0x1FFFFFFFU);
        i     =  LPC_CANAF->EFF_sa     / 4U;
        i_end = (LPC_CANAF->EFF_GRP_sa / 4U) - 1U;
        for (; i <= i_end; i++) {
          if (LPC_CANAF_RAM->mask[i] == entry) {                // If this is entry to be removed
            // Move all remaining entries in this section 32 bits towards start of table
            for (; i < i_end; i++) {
              LPC_CANAF_RAM->mask[i] = LPC_CANAF_RAM->mask[i+1U];
            }
            // Move all remaining entries in remaining sections 32 bit towards start of table
            i     = (LPC_CANAF->EFF_GRP_sa / 4U) - 1U;
            i_end =  LPC_CANAF->ENDofTable / 4U;
            for (; i < i_end; i++) {
              LPC_CANAF_RAM->mask[i] = LPC_CANAF_RAM->mask[i+1U];
            }

            // Clear last entry in table
            LPC_CANAF_RAM->mask[i+1U] = 0U;

            // Decrement affected pointers
            LPC_CANAF->ENDofTable -= 4U;
            LPC_CANAF->EFF_GRP_sa -= 4U;
            break;
          }
        }
        break;
      case CAN_FILTER_TYPE_RANGE_ID:                            // Range
        // Entry in this section is 64 bits large
        if (LPC_CANAF->EFF_GRP_sa >= LPC_CANAF->ENDofTable) {
          // If this section does not contain any entries
          return 0;
        }

        // Find entry to be removed
        entry = (x << 29) | (id & 0x1FFFFFFFU);
        i     =  LPC_CANAF->EFF_GRP_sa / 4U;
        i_end = (LPC_CANAF->ENDofTable / 4U) - 2U;
        for (; i <= i_end; i += 2U) {
          if ((LPC_CANAF_RAM->mask[i] == entry) &&              // If this is entry to be removed
              (LPC_CANAF_RAM->mask[i+1U] == ((x << 29) | (id_range_end & 0x1FFFFFFFU)))) {
            // Move all remaining entries in this section 64 bits towards start of table
            for (; i < i_end; i += 2U) {
              LPC_CANAF_RAM->mask[i   ] = LPC_CANAF_RAM->mask[i+2U];
              LPC_CANAF_RAM->mask[i+1U] = LPC_CANAF_RAM->mask[i+3U];
            }

            // Clear last entry in table
            LPC_CANAF_RAM->mask[i_end   ] = 0U;
            LPC_CANAF_RAM->mask[i_end+1U] = 0U;

            // Decrement end of table table pointer
            LPC_CANAF->ENDofTable -= 8U;
            break;
          }
        }
        break;
    }
  }

  return 1;
}

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
bool CANbadger_CAN::CANx_ObjectSetFilter (ARM_CAN_FILTER_OPERATION operation, uint32_t id, uint32_t arg, uint8_t y)
{
  uint32_t afmr;
  int32_t  status;

  if (y > 2 || y == 0)
  {
	  return false;
  }
  uint8_t x = (y - 1);


  afmr = LPC_CANAF->AFMR;                                       // Store current acceptance filter configuration
  LPC_CANAF->AFMR = CANAF_AFMR_AccBP | CANAF_AFMR_AccOff;       // Disable filter and allow table RAM access

  switch (operation)
	{
    case ARM_CAN_FILTER_ID_EXACT_ADD:
		{
			CANx_RemoveFilter(CAN_FILTER_TYPE_EXACT_ID, id,  0U, x);//first we try to remove it to make sure that it is not being duplicated
      status = CANx_AddFilter   (CAN_FILTER_TYPE_EXACT_ID, id,  0U, x);
      break;
		}
    case ARM_CAN_FILTER_ID_RANGE_ADD:
			CANx_RemoveFilter(CAN_FILTER_TYPE_RANGE_ID, id, arg, x);//first we try to remove it to make sure that it is not being duplicated
      status = CANx_AddFilter   (CAN_FILTER_TYPE_RANGE_ID, id, arg, x);
      break;
    case ARM_CAN_FILTER_ID_EXACT_REMOVE:
      status = CANx_RemoveFilter(CAN_FILTER_TYPE_EXACT_ID, id,  0U, x);
      break;
    case ARM_CAN_FILTER_ID_RANGE_REMOVE:
      status = CANx_RemoveFilter(CAN_FILTER_TYPE_RANGE_ID, id, arg, x);
      break;
    case ARM_CAN_FILTER_ID_MASKABLE_ADD:
    case ARM_CAN_FILTER_ID_MASKABLE_REMOVE:
    default:
      status = 0;
      break;
  }
  LPC_CANAF->AFMR = afmr;                                       // Restore acceptance filter configuration

  return status;
}

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
bool CANbadger_CAN::CANx_AddFilter (CAN_FILTER_TYPE filter_type, uint32_t id, uint32_t id_range_end, uint8_t x)
{
  uint32_t  i, i_end, j, j_end;
  uint32_t  entry;

  if (x >= 2)
  {
	  return false;
  }
  if ((id & ARM_CAN_ID_IDE_Msk) == 0U) // Standard Identifier (11 bit)
	{
    switch (filter_type) {
      case CAN_FILTER_TYPE_EXACT_ID:                            // Exact
        // Entry in this section is 16 bits large
        if (((LPC_CANAF->ENDofTable & CANAF_ENDofTable_ENDofTable_Msk) >= 0x800U) &&
            ((LPC_CANAF->SFF_GRP_sa == 0U) || ((LPC_CANAF->SFF_GRP_sa != 0U) && ((LPC_CANAF_RAM->mask[(LPC_CANAF->SFF_GRP_sa/4U)-1U] & (1U << 12)) == 0U)))) {
          // If table is full and no disabled entries are available
          return 0;
        }
        if ((LPC_CANAF->SFF_GRP_sa == 0U) || ((LPC_CANAF_RAM->mask[(LPC_CANAF->SFF_GRP_sa/4U)-1U] & (1U << 12)) == 0U)) {
          // If we should enlarge this section (contains no entries yet or last entry is not disabled)

          // Increment affected pointers
          LPC_CANAF->ENDofTable += 4U;
          LPC_CANAF->EFF_GRP_sa += 4U;
          LPC_CANAF->EFF_sa     += 4U;
          LPC_CANAF->SFF_GRP_sa += 4U;

          // Move all existing entries 32 bits towards end of table
          i     =  LPC_CANAF->ENDofTable / 4U;
          i_end = (LPC_CANAF->SFF_GRP_sa / 4U) - 1U;
          for (; i > i_end; i--) {
            LPC_CANAF_RAM->mask[i] = LPC_CANAF_RAM->mask[i-1U];
          }

          // Disable 2 newly added entries in this section
          LPC_CANAF_RAM->mask[i] = (1U << 29) | (1U << 28) | (0x7FFU << 16) | (1U << 13) | (1U << 12) | (0x7FFU);
        }

        // Add new entry sorted into this section
        entry = (x << 13) | (id & 0x7FFU);
        i     =  LPC_CANAF->SFF_sa     / 4U;
        i_end = (LPC_CANAF->SFF_GRP_sa / 4U) - 1U;
        for (; i <= i_end; i++) {
          if ((LPC_CANAF_RAM->mask[i] >> 16) > entry) {         // If this is place to insert new entry (high 16 bits)
            // Move all remaining entries in this section 16 bits towards end of table
            j     = i_end;
            j_end = i;
            for (; j > j_end; j--) {
              LPC_CANAF_RAM->mask[j] = (LPC_CANAF_RAM->mask[j] >> 16) | (LPC_CANAF_RAM->mask[j-1U] << 16);
            }
            // Move entry to be replaced to low 16 bits and replace it with new entry in high 16 bits
            LPC_CANAF_RAM->mask[i] = (LPC_CANAF_RAM->mask[i] >> 16) | (entry << 16);
            break;
          } else if ((LPC_CANAF_RAM->mask[i] & 0xFFFFU) > entry) {    // If this is place to insert new entry (in low 16 bits)
            // Move all remaining entries in this section 16 bits towards end of table
            j     = i_end;
            j_end = i;
            for (; j > j_end; j--) {
              LPC_CANAF_RAM->mask[j] = (LPC_CANAF_RAM->mask[j] >> 16) | (LPC_CANAF_RAM->mask[j-1U] << 16);
            }
            // Insert new entry into low 16 bits and keep high 16 bits
            LPC_CANAF_RAM->mask[i] &= 0xFFFF0000U;
            LPC_CANAF_RAM->mask[i] |= entry;
            break;
          }
        }
        break;
      case CAN_FILTER_TYPE_RANGE_ID:                            // Range
        // Entry in this section is 32 bits large
        if ((LPC_CANAF->ENDofTable & CANAF_ENDofTable_ENDofTable_Msk) >= 0x800U) {
          // If table is full
          return 0;
        }
        // Enlarge this section

        // Increment affected pointers
        LPC_CANAF->ENDofTable += 4U;
        LPC_CANAF->EFF_GRP_sa += 4U;
        LPC_CANAF->EFF_sa     += 4U;

        // Move all existing entries 32 bits towards end of table
        i     =  LPC_CANAF->ENDofTable / 4U;
        i_end = (LPC_CANAF->EFF_sa     / 4U) - 1U;
        for (; i > i_end; i--) {
          LPC_CANAF_RAM->mask[i] = LPC_CANAF_RAM->mask[i-1U];
        }
        // Disable newly added entry in this section
        LPC_CANAF_RAM->mask[i] = (1U << 29) | (1U << 28) | (0x7FFU << 16) | (1U << 13) | (1U << 12) | (0x7FFU);

        // Add new entry sorted into this section
        entry = (x << 29) | ((id & 0x7FFU) << 16) | (x << 13) | (id_range_end & 0x7FFU);
        i     =  LPC_CANAF->SFF_GRP_sa / 4U;
        i_end = (LPC_CANAF->EFF_sa     / 4U) - 1U;
        for (; i <= i_end; i++) {
          if (LPC_CANAF_RAM->mask[i] > entry) {                 // If this is place to insert new entry
            // Move all remaining entries 32 bits towards end of table
            j     = i_end;
            j_end = i;
            for (; j > j_end; j--) {
              LPC_CANAF_RAM->mask[j] = LPC_CANAF_RAM->mask[j-1U];
            }
            // Insert new entry
            LPC_CANAF_RAM->mask[i] = entry;
            break;
          }
        }
        break;
    }
  }
	else
	{         // Extended Identifier (29 bit)
    switch (filter_type) {
      case CAN_FILTER_TYPE_EXACT_ID:                            // Exact
        // Entry in this section is 32 bits large
        if ((LPC_CANAF->ENDofTable & CANAF_ENDofTable_ENDofTable_Msk) >= 0x800U) {
          // If table is full
          return 0;
        }
        // Enlarge this section

        // Increment affected pointers
        LPC_CANAF->ENDofTable += 4U;
        LPC_CANAF->EFF_GRP_sa += 4U;

        // Move all entries 32 bits towards end of table
        i     =  LPC_CANAF->ENDofTable / 4U;
        i_end = (LPC_CANAF->EFF_GRP_sa / 4U - 1);
        for (; i > i_end; i--) {
          LPC_CANAF_RAM->mask[i] = LPC_CANAF_RAM->mask[i-1U];
        }
        // Disable newly added entry in this section
        LPC_CANAF_RAM->mask[i] = (7U << 29) | (0x1FFFFFFFU);

        // Add new entry sorted into this section
        entry = (x << 29) | (id & 0x1FFFFFFFU);
        i     =  LPC_CANAF->EFF_sa     / 4U;
        i_end = (LPC_CANAF->EFF_GRP_sa / 4U) - 1U;
        for (; i <= i_end; i++) {
          if (LPC_CANAF_RAM->mask[i] > entry) {                 // If this is place to insert new entry
            // Move all remaining entries in this section 32 bits towards end of table
            j     = i_end;
            j_end = i;
            for (; j > j_end; j--) {
              LPC_CANAF_RAM->mask[j] = LPC_CANAF_RAM->mask[j-1U];
            }
            // Insert new entry
            LPC_CANAF_RAM->mask[i] = entry;
            break;
          }
        }
        break;
      case CAN_FILTER_TYPE_RANGE_ID:                            // Range
        // Entry in this section is 64 bits large
        if ((LPC_CANAF->ENDofTable & CANAF_ENDofTable_ENDofTable_Msk) >= 0x800U) {
          // If table is full
          return 0;
        }
        // Enlarge this section

        // Increment end of table pointer
        LPC_CANAF->ENDofTable += 8U;

        // Disable newly added entry in this section
        LPC_CANAF_RAM->mask[(LPC_CANAF->ENDofTable/4U)-2U] = (7U << 29) | (0x1FFFFFFFU);
        LPC_CANAF_RAM->mask[(LPC_CANAF->ENDofTable/4U)-1U] = (7U << 29) | (0x1FFFFFFFU);

        // Add new entry sorted into this section
        entry = (x << 29) | (id & 0x1FFFFFFFU);
        i     =  LPC_CANAF->EFF_GRP_sa / 4U;
        i_end = (LPC_CANAF->ENDofTable / 4U) - 2U;
        for (; i <= i_end; i += 2U) {
          if (LPC_CANAF_RAM->mask[i] > entry) {             // If this is place to insert new entry
            // Move all remaining entries in this section 64 bits towards end of table
            j     = i_end;
            j_end = i;
            for (; j > j_end; j -= 2U) {
              LPC_CANAF_RAM->mask[j   ] = LPC_CANAF_RAM->mask[j-2U];
              LPC_CANAF_RAM->mask[j+1U] = LPC_CANAF_RAM->mask[j-1U];
            }
            // Insert new entry
            LPC_CANAF_RAM->mask[i   ] = entry;
            LPC_CANAF_RAM->mask[i+1U] = (x << 29) | (id_range_end & 0x1FFFFFFFU);
            break;
          }
        }
        break;
    }
  }

  return 1;
}
