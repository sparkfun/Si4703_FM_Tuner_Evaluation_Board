/* 

Library for Sparkfun Si4703 breakout board.
Alexandre OGER 2013-08-15
based on Simon Monk and Nathan Seidle work
I added lots of RDS data decoding : PS RT PI PTY TP TA AF CT
(look at http://en.wikipedia.org/wiki/Radio_Data_System for definition and ftp://ftp.rds.org.uk/pub/acrobat/rbds1998.pdf for specifications)


plus some minor changes (like timeout on function with 'While(1)' condition)


original comment:
		Library for Sparkfun Si4703 breakout board.
		Simon Monk. 2011-09-09
		
		This is a library wrapper and a few extras to the excellent code produced
		by Nathan Seidle from Sparkfun (Beerware).
		
		Nathan's comments......
		
		Look for serial output at 57600bps.
		
		The Si4703 ACKs the first byte, and NACKs the 2nd byte of a read.
		
		1/18 - after much hacking, I suggest NEVER write to a register without first reading the contents of a chip.
		ie, don't updateRegisters without first readRegisters.
		
		If anyone manages to get this datasheet downloaded
		http://wenku.baidu.com/view/d6f0e6ee5ef7ba0d4a733b61.html
		Please let us know. It seem to be the latest version of the programming guide. It had a change on page 12 (write 0x8100 to 0x07)
		that allowed me to get the chip working..
		
		Also, if you happen to find "AN243: Using RDS/RBDS with the Si4701/03", please share. I love it when companies refer to
		documents that don't exist.
		
		1/20 - Picking up FM stations from a plane flying over Portugal! Sweet! 93.9MHz sounds a little soft for my tastes,s but
		it's in Porteguese.
		
		ToDo:
		Display current status (from 0x0A) - done 1/20/11
		Add RDS decoding - works, sort of
		Volume Up/Down - done 1/20/11
		Mute toggle - done 1/20/11
		Tune Up/Down - done 1/20/11
		Read current channel (0xB0) - done 1/20/11
		Setup for Europe - done 1/20/11
		Seek up/down - done 1/25/11
		
		The Si4703 breakout does work with line out into a stereo or other amplifier. Be sure to test with different length 3.5mm
		cables. Too short of a cable may degrade reception.
*/

#ifndef Si4703_Breakout_h
#define Si4703_Breakout_h

#include "Arduino.h"



class Si4703_Breakout
{
 public:
 
    Si4703_Breakout(int resetPin, int sdioPin, int sclkPin);
    boolean powerOn();					
	void setChannel(int channel);  	// Alex OGER add timeout 2sec
	int seekUp(); 					
	int seekDown(); 				
	void setVolume(int volume); 	

	
	void readPS(char* message);		 //added by Alex OGER 
	void readRT(char* message);		 //added by Alex OGER
	long  readPI();					         //added by Alex OGER
	int  readPTY();					         //added by Alex OGER
	int  readTP();					         //added by Alex OGER
	int  readTA();					         //added by Alex OGER
	void  readAF(int* frequencies); //added by Alex OGER
	void  readCT(int* date_time);		           //added by Alex OGER 
	int  getRSSI();					           //added by Alex OGER
	int  getChannel();				       //Alex OGER move from private to public
	
 private:
 
  int  _resetPin;
	int  _sdioPin;
	int  _sclkPin;
	boolean si4703_init();			     //Alex OGER change type void ->boolean
	boolean readRegisters();		     //Alex OGER change type void ->boolean + timout 2sec. in I2c communication
	byte updateRegisters();
	int seek(byte seekDirection);	   //Alex OGER add timeout 4sec
	
	uint16_t si4703_registers[16]; 
	static const uint16_t  FAIL = 0;
	static const uint16_t  SUCCESS = 1;

	static const int  SI4703 = 0x10; 
	static const uint16_t  I2C_FAIL_MAX = 10; 
	static const uint16_t  SEEK_DOWN = 0; 
	static const uint16_t  SEEK_UP = 1;

	//Define the register names
	static const uint16_t  DEVICEID = 0x00;
	static const uint16_t  CHIPID = 0x01;
	static const uint16_t  POWERCFG = 0x02;
	static const uint16_t  CHANNEL = 0x03;
	static const uint16_t  SYSCONFIG1 = 0x04;
	static const uint16_t  SYSCONFIG2 = 0x05;
	static const uint16_t  STATUSRSSI = 0x0A;
	static const uint16_t  READCHAN = 0x0B;
	static const uint16_t  RDSA = 0x0C;
	static const uint16_t  RDSB = 0x0D;
	static const uint16_t  RDSC = 0x0E;
	static const uint16_t  RDSD = 0x0F;

	//Register 0x02 - POWERCFG
	static const uint16_t  SMUTE = 15;
	static const uint16_t  DMUTE = 14;
	static const uint16_t  MONO = 13;
	static const uint16_t  SKMODE = 10;
	static const uint16_t  SEEKUP = 9;
	static const uint16_t  SEEK = 8;

	//Register 0x03 - CHANNEL
	static const uint16_t  TUNE = 15;

	//Register 0x04 - SYSCONFIG1
	static const uint16_t  RDS = 12;
	static const uint16_t  DE = 11;

	//Register 0x05 - SYSCONFIG2
	static const uint16_t  SPACE1 = 5;
	static const uint16_t  SPACE0 = 4;

	//Register 0x0A - STATUSRSSI
	static const uint16_t  RDSR = 15;
	static const uint16_t  STC = 14;
	static const uint16_t  SFBL = 13;
	static const uint16_t  AFCRL = 12;
	static const uint16_t  RDSS = 11;
	static const uint16_t  STEREO = 8;
};

#endif
