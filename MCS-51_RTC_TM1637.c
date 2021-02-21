/*AT89C4051 program to read DS3231 or DS1307 chip and display time on TM1637*/
#define DEV_ADDR_RTC 0xD0    // I2C address of DS3231 and DS1307

sbit sclPin at P1.B0;        // I2C serial clock line
sbit sdaPin at P1.B1;        // I2C serial data line
sbit clkPin at P1.B2;
sbit dioPin at P1.B3;
sbit okBtn at P3.B3;
sbit hourBtn at P3.B4;
sbit minBtn at P3.B5;

unsigned char mArr[7], dpFlag, dpCtr = 0;
unsigned char tsFlag = 0, timeSet = 0, nArr[2];
unsigned char mHour, mMin;

void startI2C()          // I2C start condition
{
  sdaPin = 1;
  sclPin = 1;
  sdaPin = 0;
  sclPin = 0;
}

void stopI2C()           // I2C stop condition
{
  sclPin = 0;
  sdaPin = 0;
  sclPin = 1;
  sdaPin = 1;
}

void writeToSlave(unsigned char mByte) // Writing byte to I2C slave
{
 unsigned char k;
 for(k = 0; k <= 8; k++)
 {
   sdaPin = mByte & 0x80 ? 1 : 0;
   sclPin = 1;
   sclPin = 0;
   mByte <<= 1;
 }
}

unsigned char readFromSlave(unsigned char mByte) // Reading byte from I2C slave
{
 unsigned char k;
 mByte = 0;
 for(k = 0; k < 8; k++)
 {
   sdaPin = 1;
   sclPin = 1;
   mByte <<= 1;
   mByte |= sdaPin;
   sclPin = 0;
 }
 return mByte;
}

void writeToI2C(unsigned char mAddr, unsigned char mIndex, unsigned char mByte)
{
  startI2C();
  writeToSlave(mAddr);
  writeToSlave(mIndex);
  writeToSlave(mByte);
  stopI2C();
}

unsigned char readFromI2C(unsigned char mAddr, unsigned char mIndex)
{
  unsigned char mByte;
  startI2C();
  writeToSlave(mAddr);
  writeToSlave(mIndex);
  startI2C();
  writeToSlave(mAddr + 1);
  mByte = readFromSlave(mIndex);
  stopI2C();
  delay_ms(5);
  return mByte;
}

void readRtc()
{
  unsigned char k;
  for (k = 0; k < 6; k++)
   mArr[k] = readFromI2C(DEV_ADDR_RTC, k);
}

void tmStart(void) // 1637 start
{
   clkPin = 1;
   dioPin = 1;
   delay_us(2);
   dioPin = 0;
}

void tmAck(void) // 1637 Answer
{
   unsigned char n = 0;
   clkPin = 0;
   delay_us(2); // After the falling edge of the eighth clock delay 5us, ACK signals the beginning of judgment
   while(n <= 255)
   {
      if(!dioPin)
         break;
      else
         n++;
   }
   clkPin = 1;
   delay_us(5);
   clkPin = 0;
}

void tmStop(void) // 1637 Stop
{
   clkPin = 0;
   delay_us(2);
   dioPin = 0;
   delay_us(2);
   clkPin = 1;
   delay_us(2);
   dioPin = 1;
}

void tmWrite(unsigned char oneByte) // write a byte
{
   unsigned char k;
   for (k = 0; k < 8; k++)
   {
      clkPin = 0;
      if (oneByte & 0x01) // low front
       dioPin = 1;
      else
       dioPin = 0;
      delay_us(3);
      oneByte = oneByte >> 1;
      clkPin = 1;
      delay_us(3);
   }
}

void tmDisplay(unsigned char mAddr, unsigned char mTime) // Write display register
{
   tmStart();
   tmWrite(0x44); //40H address is automatically incremented by 1 mode, 44H fixed address mode
   tmAck();
   tmStop();
   tmStart();
   tmWrite(mAddr); // Set the address
   tmAck();
   tmWrite(mTime); // Send displayble data
   tmAck();
   tmStop();
   tmStart();
   tmWrite(0x8F); // Open display, maximum brightness
   tmAck();
   tmStop();
}

void setRtc()
{
   unsigned char k;
   for(k = 1; k < 3; k++)
      writeToI2C(DEV_ADDR_RTC, k, (nArr[k - 1] / 10 * 16) + (nArr[k - 1] % 10));
}

void showTime(unsigned char segCode[])
{
   tmDisplay(0xC0, segCode[mHour / 16] | dpFlag);
   tmDisplay(0xC1, segCode[mHour % 16] | dpFlag);
   tmDisplay(0xC2, segCode[mMin / 16] | dpFlag);
   tmDisplay(0xC3, segCode[mMin % 16] | dpFlag);
}

void showTemperature(unsigned char segCode[])
{   unsigned char nByte;
    nByte = readFromI2C(DEV_ADDR_RTC, 0x0E);
    nByte |= 0x20;  // Setting CONV bit of 0x0E register to convert the temperature
    writeToI2C(DEV_ADDR_RTC, 0x0E, nByte);
    nByte = readFromI2C(DEV_ADDR_RTC, 0x11);
    nByte = (nByte / 10 * 16) + (nByte % 10); // Decimal to Hexadecimal (BCD)
    tmDisplay(0xC0, segCode[nByte / 16]);
    tmDisplay(0xC1, segCode[nByte % 16]);
    tmDisplay(0xC2, 0x63); // Degree symbol
    tmDisplay(0xC3, 0x39); // C letter
}

void main()
{
   unsigned char segCode[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F}; // use 0x80 for colons (dp)
   unsigned char colonDots = 12.5;
   unsigned int dataSwitch = 349;
   IE=0x81; // Enable external button interrupt
   for(;;)
   {
      if(timeSet)
      {
         nArr[0] = (mMin / 16 * 10) + (mMin % 16);
         nArr[1] = (mHour / 16 * 10) + (mHour % 16);
         setRtc();
         timeSet = 0;
      }

      readRtc();

      mMin = mArr[1];
      mHour = mArr[2];

      if(mHour > 0x12 && mHour <= 0x21)
         mHour = (mHour / 16 * 10) + (mHour % 16) - 12;

      if(mHour > 0x21)
         mHour = (mHour / 16 * 10) + (mHour % 16) - 6;

      if(mHour == 0)
         mHour = 0x12;

      showTime(segCode);

      if(colonDots > 0)
      {
         colonDots--;
         delay_ms(1);
      }
      else
      {
         if(dpCtr == 0)
         {
            dpCtr = 1;
            dpFlag = 0x80;
         }
         else
         {
            dpCtr = 0;
            dpFlag = 0x00;
         }
         colonDots = 12;
      }

      if(tsFlag)  // Time setting
      {
         dpFlag = 0x80;
         while(okBtn)
         {
            if(!hourBtn)
            {
               delay_ms(250);
               mHour++;

               if((mHour & 0x0F) == 0x0A)
                  mHour = mHour + 6;

               if(mHour > 0x12)
                  mHour = 0x01;
            }

            if(!minBtn)
            {
               delay_ms(250);
               mMin++;

               if((mMin & 0x0F) == 0x0A)
                  mMin = mMin + 6;

               if(mMin > 0x59)
                  mMin = 0;
            }
            showTime(segCode);
         }
         tsFlag = 0;
         timeSet = 1;
      }

      if(dataSwitch > 0)
      {
         dataSwitch--;
         delay_ms(1);
      }
      else
      {
         showTemperature(segCode);
         dataSwitch = 349;
         delay_ms(5000);
      }
   }
}

void setClock() iv IVT_ADDR_EX0 ilevel 0 ics ICS_AUTO
{
   tsFlag = 1;
}