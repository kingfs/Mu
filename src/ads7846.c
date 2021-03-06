#include <stdint.h>
#include <stdbool.h>

#include "emulator.h"
#include "portability.h"
#include "hardwareRegisters.h"


bool ads7846PenIrqEnabled;

static const uint16_t ads7846DockResistorValues[PORT_END] = {0xFFF/*none*/, 0x1EB/*USB cradle*/, 0x000/*serial cradle, unknown*/, 0x000/*USB peripheral, unknown*/, 0x000/*serial peripheral, unknown*/};

static uint8_t  ads7846BitsToNextControl;
static uint8_t  ads7846ControlByte;
static uint16_t ads7846OutputValue;
static bool     ads7846ChipSelect;


static float ads7846RangeMap(float oldMin, float oldMax, float value, float newMin, float newMax){
   return (value - oldMin) / (oldMax - oldMin) * (newMax - newMin) + newMin;
}

static bool ads7846GetAdcBit(void){
   bool bit = !!(ads7846OutputValue & 0x8000);
   ads7846OutputValue <<= 1;
   return bit;
}

void ads7846Reset(void){
   ads7846BitsToNextControl = 0;
   ads7846ControlByte = 0x00;
   ads7846PenIrqEnabled = true;
   ads7846OutputValue = 0x0000;
   ads7846ChipSelect = true;
#if !defined(EMU_NO_SAFETY)
   refreshTouchState();
#endif
}

uint64_t ads7846StateSize(void){
   uint64_t size = 0;

   size += sizeof(uint8_t) * 4;
   size += sizeof(uint16_t);

   return size;
}

void ads7846SaveState(uint8_t* data){
   uint64_t offset = 0;

   writeStateValue8(data + offset, ads7846PenIrqEnabled);
   offset += sizeof(uint8_t);
   writeStateValue8(data + offset, ads7846BitsToNextControl);
   offset += sizeof(uint8_t);
   writeStateValue8(data + offset, ads7846ControlByte);
   offset += sizeof(uint8_t);
   writeStateValue16(data + offset, ads7846OutputValue);
   offset += sizeof(uint16_t);
   writeStateValue8(data + offset, ads7846ChipSelect);
   offset += sizeof(uint8_t);
}

void ads7846LoadState(uint8_t* data){
   uint64_t offset = 0;

   ads7846PenIrqEnabled = readStateValue8(data + offset);
   offset += sizeof(uint8_t);
   ads7846BitsToNextControl = readStateValue8(data + offset);
   offset += sizeof(uint8_t);
   ads7846ControlByte = readStateValue8(data + offset);
   offset += sizeof(uint8_t);
   ads7846OutputValue = readStateValue16(data + offset);
   offset += sizeof(uint16_t);
   ads7846ChipSelect = readStateValue8(data + offset);
   offset += sizeof(uint8_t);
}

void ads7846SetChipSelect(bool value){
   //reset the chip when disabled, chip is active when chip select is low
   if(value && !ads7846ChipSelect){
      ads7846BitsToNextControl = 0;
      ads7846ControlByte = 0x00;
      ads7846PenIrqEnabled = true;
      ads7846OutputValue = 0x0000;
#if !defined(EMU_NO_SAFETY)
      refreshTouchState();
#endif
   }
   ads7846ChipSelect = value;
}

bool ads7846ExchangeBit(bool bitIn){
   //chip data out is high when off
   if(ads7846ChipSelect)
      return true;

   if(ads7846BitsToNextControl > 0)
      ads7846BitsToNextControl--;

   if(ads7846BitsToNextControl == 0){
      //check for control bit
      //a new control byte can be sent while receiving data
      //this is valid behavior as long as the start of the last control byte was 16 or more clock cycles ago
      if(bitIn){
         ads7846ControlByte = 0x01;
         ads7846BitsToNextControl = 15;
      }
      return ads7846GetAdcBit();
   }
   else if(ads7846BitsToNextControl >= 8){
      ads7846ControlByte <<= 1;
      ads7846ControlByte |= bitIn;
   }
   else if(ads7846BitsToNextControl == 6){
      //control byte and busy cycle finished, get output value
      bool bitMode = !!(ads7846ControlByte & 0x08);
      bool differentialMode = !(ads7846ControlByte & 0x04);
      uint8_t channel = (ads7846ControlByte & 0x70) >> 4;
      uint8_t powerSave = ads7846ControlByte & 0x03;

      //debugLog("Accessed ADS7846 Ch:%d, %d bits, %s Mode, Power Save:%d, PC:0x%08X.\n", channel, bitMode ? 8 : 12, differentialMode ? "Diff" : "Normal", ads7846ControlByte & 0x03, flx68000GetPc());

      //reference disabled currently isnt emulated, I dont know what the proper behavior for that would be

#if !defined(EMU_NO_SAFETY)
      //trigger fake IRQs
      ads7846OverridePenState(!(channel == 1 || channel == 3 || channel == 4 || channel == 5));
#endif

      if(powerSave != 2){
         //ADC enabled, get analog value
         if(differentialMode){
            switch(channel){
               case 0:
                  //temperature 0, wrong mode
                  ads7846OutputValue = 0xFFF;
                  break;

               case 1:
                  //touchscreen y
                  if(palmInput.touchscreenTouched)
                     ads7846OutputValue = ads7846RangeMap(0.0, 1.0, 1.0 - palmInput.touchscreenY, 0x0EE, 0xEE4);
                  else
                     ads7846OutputValue = 0xFEF;//y is almost fully on when dorment
                  break;

               case 2:
                  //battery, wrong mode
                  ads7846OutputValue = 0xFFF;
                  break;

               case 3:
                  //touchscreen x relative to y
                  if(palmInput.touchscreenTouched)
                      ads7846OutputValue = ads7846RangeMap(0.0, 1.0, 1.0 - palmInput.touchscreenX, 0x093, 0x600) + ads7846RangeMap(0.0, 1.0, 1.0 - palmInput.touchscreenY, 0x000, 0x280);
                  else
                     ads7846OutputValue = 0x000;
                  break;

               case 4:
                  //touchscreen y relative to x
                  if(palmInput.touchscreenTouched)
                      ads7846OutputValue = ads7846RangeMap(0.0, 1.0, 1.0 - palmInput.touchscreenY, 0x9AF, 0xF3F) + ads7846RangeMap(0.0, 1.0, 1.0 - palmInput.touchscreenX, 0x000, 0x150);
                  else
                     ads7846OutputValue = 0xFFF;
                  break;

               case 5:
                  //touchscreen x
                  if(palmInput.touchscreenTouched)
                     ads7846OutputValue = ads7846RangeMap(0.0, 1.0, 1.0 - palmInput.touchscreenX, 0x0FD, 0xF47);
                  else
                     ads7846OutputValue = 0x309;
                  break;

               case 6:
                  //dock, wrong mode
                  ads7846OutputValue = 0xFFF;
                  break;

               case 7:
                  //temperature 1, wrong mode, usualy 0xDFF/0xBFF, sometimes 0xFFF
                  ads7846OutputValue = 0xDFF;
                  break;
            }
         }
         else{
            if(!palmInput.touchscreenTouched){
               switch(channel){
                  case 0:
                     //temperature 0, room temperature
                     ads7846OutputValue = 0x3E2;
                     break;

                  case 1:
                     //touchscreen y
                     if(palmInput.touchscreenTouched)
                        ads7846OutputValue = ads7846RangeMap(0.0, 1.0, 1.0 - palmInput.touchscreenY, 0x0EE, 0xEE4);
                     else
                        ads7846OutputValue = 0xFFF;//y is almost fully on when dorment
                     break;

                  case 2:
                     //battery, unknown hasent gotten low enough to test yet
                     //ads7846OutputValue = 0x600;//5%
                     //ads7846OutputValue = 0x61C;//30%
                     //ads7846OutputValue = 0x63C;//40%
                     //ads7846OutputValue = 0x65C;//60%
                     //ads7846OutputValue = 0x67C;//80%
                     //ads7846OutputValue = 0x68C;//100%
                     ads7846OutputValue = 0x69C;//100%
                     //ads7846OutputValue = ads7846RangeMap(0, 100, palmMisc.batteryLevel, 0x000, 0x7F8);
                     break;

                  case 3:
                     //touchscreen x relative to y
                     if(palmInput.touchscreenTouched)
                        ads7846OutputValue = ads7846RangeMap(0.0, 1.0, 1.0 - palmInput.touchscreenX, 0x093, 0x600) + ads7846RangeMap(0.0, 1.0, 1.0 - palmInput.touchscreenY, 0x000, 0x280);
                     else
                        ads7846OutputValue = 0x000;
                     break;

                  case 4:
                     //touchscreen y relative to x
                     if(palmInput.touchscreenTouched)
                        ads7846OutputValue = ads7846RangeMap(0.0, 1.0, 1.0 - palmInput.touchscreenY, 0x9AF, 0xF3F) + ads7846RangeMap(0.0, 1.0, 1.0 - palmInput.touchscreenX, 0x000, 0x150);
                     else
                        ads7846OutputValue = 0xFFF;
                     break;

                  case 5:
                     //touchscreen x
                     if(palmInput.touchscreenTouched)
                        ads7846OutputValue = ads7846RangeMap(0.0, 1.0, 1.0 - palmInput.touchscreenX, 0x0FD, 0xF47);
                     else
                        ads7846OutputValue = 0x3FB;
                     break;

                  case 6:
                     //dock
                     ads7846OutputValue = ads7846DockResistorValues[palmMisc.dataPort];
                     break;

                  case 7:
                     //temperature 1, room temperature
                     ads7846OutputValue = 0x4A1;
                     break;
               }
            }
            else{
               //crosses lines with REF+(unverified)
               ads7846OutputValue = 0xF80;
            }
         }
      }
      else{
         //ADC disabled, return invalid data
         if((channel == 3 || channel == 5) && !palmInput.touchscreenTouched)
            ads7846OutputValue = 0x000;
         else
            ads7846OutputValue = 0xFFF;
      }

      //move to output position
      ads7846OutputValue <<= 4;

      //if 8 bit conversion, clear extra bits and shorten conversion by 4 bits
      if(bitMode){
         ads7846OutputValue &= 0xFF00;
         ads7846BitsToNextControl -= 4;
      }

      ads7846PenIrqEnabled = !(powerSave & 0x01);
#if !defined(EMU_NO_SAFETY)
      refreshTouchState();
#endif
   }

   return ads7846GetAdcBit();
}
