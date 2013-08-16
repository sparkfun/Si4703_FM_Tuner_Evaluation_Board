#include "Arduino.h"
#include "Si4703_Breakout.h"
#include "Wire.h"

Si4703_Breakout::Si4703_Breakout(int resetPin, int sdioPin, int sclkPin)
{
  _resetPin = resetPin;
  _sdioPin = sdioPin;
  _sclkPin = sclkPin;
}

boolean Si4703_Breakout::powerOn()
{
	if (si4703_init()){
		return 1; 
	}else{
		return 0;
	}
	
}

//reste à faire :
//param par defaut init
// AF
// ct


void Si4703_Breakout::setChannel(int channel)
{
  //Freq(MHz) = 0.200(in USA) * Channel + 87.5MHz
  //97.3 = 0.2 * Chan + 87.5
  //9.8 / 0.2 = 49
  int newChannel = channel * 10; //973 * 10 = 9730
  newChannel -= 8750; //9730 - 8750 = 980
  newChannel /= 10; //980 / 10 = 98

  //These steps come from AN230 page 20 rev 0.5
  readRegisters();
  si4703_registers[CHANNEL] &= 0xFE00; //Clear out the channel bits
  si4703_registers[CHANNEL] |= newChannel; //Mask in the new channel
  si4703_registers[CHANNEL] |= (1<<TUNE); //Set the TUNE bit to start
  updateRegisters();

  //delay(60); //Wait 60ms - you can use or skip this delay

  //timeout
  long endTime = millis() + 2000;
  //Poll to see if STC is set
  while(millis() < endTime) {
    readRegisters();
    if( (si4703_registers[STATUSRSSI] & (1<<STC)) != 0) break; //Tuning complete!
  }

  readRegisters();
  si4703_registers[CHANNEL] &= ~(1<<TUNE); //Clear the tune after a tune has completed
  updateRegisters();

  //Wait for the si4703 to clear the STC as well
  while(millis() < endTime) { 
    readRegisters();
    if( (si4703_registers[STATUSRSSI] & (1<<STC)) == 0) break; //Tuning complete!
  }
}



int Si4703_Breakout::seekUp(){
	return seek(SEEK_UP);
}



int Si4703_Breakout::seekDown(){
	return seek(SEEK_DOWN);
}




void Si4703_Breakout::setVolume(int volume){
  readRegisters(); //Read the current register set
  if(volume < 0) volume = 0;
  if (volume > 15) volume = 15;
  si4703_registers[SYSCONFIG2] &= 0xFFF0; //Clear volume bits
  si4703_registers[SYSCONFIG2] |= volume; //Set new volume
  updateRegisters(); //Update
}




void Si4703_Breakout::readPS(char* buffer){ 
	int timeout=2000;
	unsigned long  endTime = millis() + timeout;
	boolean completed[] = {false, false, false, false};
	int completedCount = 0;
	while(completedCount < 4 && millis() < endTime) {
		readRegisters();
		if(si4703_registers[STATUSRSSI] & (1<<RDSR)){

			uint16_t b = si4703_registers[RDSB];
			int index = b & 0x03;
			byte type_trame=(si4703_registers[RDSB] & 0xF000) >> 12;
			if (! completed[index] && type_trame==0x00)
			{
				completed[index] = true;
				completedCount ++;
				char Dh = (si4703_registers[RDSD] & 0xFF00) >> 8;
				char Dl = (si4703_registers[RDSD] & 0x00FF);
				buffer[index * 2] = Dh;
				buffer[index * 2 +1] = Dl;
			}
			delay(40); //Wait for the RDS bit to clear
		}
		else {
			delay(30); //From AN230, using the polling method 40ms should be sufficient amount of time between checks
		}
	}
	if (millis() >= endTime) {
		buffer[0] ='N';
		buffer[1] ='o';
		buffer[2] =' ';
		buffer[3] ='R';
		buffer[4] ='D';
		buffer[5] ='S';
		buffer[6] ='\0';
		return;
	}
	
	buffer[8] = '\0';
}





void Si4703_Breakout::readRT(char* buffer){ 
	int timeout=20000;
	int select;
	unsigned long  endTime = millis() + timeout;
	boolean completed[] = {false, false, false, false,false, false, false, false,false, false, false, false,false, false, false, false};
	int completedCount = 0;
	while(completedCount < 16 && millis() < endTime) {
		readRegisters();
		if(si4703_registers[STATUSRSSI] & (1<<RDSR)){
            uint16_t b = si4703_registers[RDSB];
			int index = b & 0x0F;
			byte type_trame=(si4703_registers[RDSB] & 0xF000) >> 12;
			byte sous_type_trame=(si4703_registers[RDSB] & 0x0800) >> 8;
			//Serial.println(type_trame,BIN);
			if (! completed[index] && type_trame==0x02)
			{
				if (sous_type_trame==0x00)
				{
					completed[index] = true;
					completedCount ++;
					char Ch = (si4703_registers[RDSC] & 0xFF00) >> 8;
					char Cl = (si4703_registers[RDSC] & 0x00FF);
					char Dh = (si4703_registers[RDSD] & 0xFF00) >> 8;
					char Dl = (si4703_registers[RDSD] & 0x00FF);
					buffer[index * 4]    = Ch;
					buffer[index * 4 +1] = Cl;
					buffer[index * 4 +2] = Dh;
					buffer[index * 4 +3] = Dl;
				}else{
					completed[index] = true;
					completedCount ++;
					char Dh = (si4703_registers[RDSD] & 0xFF00) >> 8;
					char Dl = (si4703_registers[RDSD] & 0x00FF);
					buffer[index * 2]    = Dh;
					buffer[index * 2 +1] = Dl;					
			    }
			}
			delay(40); //Wait for the RDS bit to clear
		}
		else {
			delay(30); //From AN230, using the polling method 40ms should be sufficient amount of time between checks
		}
	}
	if (millis() >= endTime || select == LOW) {
		buffer[0] =' ';
		buffer[1] =' ';
		buffer[2] =' ';
		buffer[3] =' ';
		buffer[4] =' ';
		buffer[5] =' ';
		buffer[6] =' ';
		buffer[7] =' ';
		buffer[8] ='N';
		buffer[9] ='o';
		buffer[10] =' ';
		buffer[11] ='R';
		buffer[12] ='a';
		buffer[13] ='d';
		buffer[14] ='i';
		buffer[15] ='o';
		buffer[16] =' ';
		buffer[17] ='T';
		buffer[18] ='e';
		buffer[19] ='x';
		buffer[20] ='t';
		buffer[21] =' ';
		buffer[22] =' ';
		buffer[23] =' ';
		buffer[24] =' ';
		buffer[25] =' ';
		buffer[26] =' ';
		buffer[27] =' ';
		buffer[64] ='\0';
		return;
	}
	
	buffer[64] = '\0';
}






long Si4703_Breakout::readPI(){ 
	int timeout=400;
	unsigned long  endTime = millis() + timeout;
	while(millis() < endTime) {
		readRegisters();
		if(si4703_registers[STATUSRSSI] & (1<<RDSR)){
			return (long)si4703_registers[RDSA];
		}
		else {
			delay(30); //From AN230, using the polling method 40ms should be sufficient amount of time between checks
		}
	}
	if (millis() >= endTime) {
		return 0;
	}
}





int Si4703_Breakout::readTP(){ 
	int timeout=500;
	unsigned long  endTime = millis() + timeout;
	int completedCount = 0;
	while(completedCount < 1 && millis() < endTime) {
		readRegisters();
		if(si4703_registers[STATUSRSSI] & (1<<RDSR)){
			int value=(int)(si4703_registers[RDSB]& 0x0400) >> 10;
			return value;
		}
		else {
			delay(30); //From AN230, using the polling method 40ms should be sufficient amount of time between checks
		}
	}
	if (millis() >= endTime) {
		return 0;
	}
}





int Si4703_Breakout::readTA(){ 
	int timeout=1000;
	unsigned long  endTime = millis() + timeout;
	while(millis() < endTime) {
		readRegisters();
		if(si4703_registers[STATUSRSSI] & (1<<RDSR)){
			byte type_trame=(si4703_registers[RDSB] & 0xF000) >> 12;
			if (type_trame==0x00)
			{
				int value=(si4703_registers[RDSB]& 0x0010) >> 4;
				return value;
			}
		}
		else {
			delay(30); //From AN230, using the polling method 40ms should be sufficient amount of time between checks
		}
	}
	if (millis() >= endTime) {
		return 0;
	}
}



void Si4703_Breakout::readAF(int* buffer){ 
	int timeout=10000;
	unsigned long  endTime = millis() + timeout;
	int buffer_temp[25]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int number_of_frequencies=25; //init max, but overwritten when real number of frequencies will be detected
	int completed=0;
	int index=0;
	int i;
	int j;
	long Ch;
	long Cl;
	int ok;
	int select;
	while(millis() < endTime) {
		readRegisters();
		if(si4703_registers[STATUSRSSI] & (1<<RDSR)){
			byte type_trame=(si4703_registers[RDSB] & 0xF000) >> 12;
			byte sous_type_trame=(si4703_registers[RDSB] & 0x0800) >> 8;
			if (type_trame==0x00 && sous_type_trame==0x00)
			{
				if(completed < number_of_frequencies-1 || buffer_temp[0]==0) {
					Ch = (si4703_registers[RDSC] & 0xFF00) >> 8;
					Cl = (si4703_registers[RDSC] & 0x00FF);
					if (Ch > 223 && Ch < 250) {
						number_of_frequencies=Ch-224; //alternate frequencies number
						if (number_of_frequencies==0) {
							return;
						}
						buffer_temp[0]=Cl+875; //actual frequency

					} else {
						ok=1;
						for (j = 0; j < 25; j++) {
							if (buffer_temp[j]==(Ch+875) || buffer_temp[j]==(Cl+875) ) {
								ok=0;
								break;
							}
							
						}
						for (i = 1; i < 25; i++) {
								if (buffer_temp[i]==0 && ok!=0) {
									buffer_temp[i]=Ch+875; //add 1st frequency
									buffer_temp[i+1]=Cl+875;//add 2nd frequency
									completed=completed+2;
									break;
								}                
						}
					}
				}else{
					//all fake frequencies (aka 205->1080->108.0Mhz) are deleted
					for (i = 0; i < number_of_frequencies; i++) {
						if(buffer_temp[i]!=1080) {
							buffer[index]=buffer_temp[i];
							index++;
						} else {
							//Serial.print("Fake frequency");
						}
   
					}
					return;          
				}					
			}
		}else {
			delay(30); //From AN230, using the polling method 40ms should be sufficient amount of time between checks
		}
	}
	if (millis() >= endTime || select == LOW){
		return ;
	}
}






int Si4703_Breakout::readPTY(){	
	int timeout=400;
	int pty=0;
	unsigned long  endTime = millis() + timeout;
	int completedCount = 0;
	while(completedCount < 1 && millis() < endTime) {
		readRegisters();
		if(si4703_registers[STATUSRSSI] & (1<<RDSR)){
			byte type_trame=(si4703_registers[RDSB] & 0xF000) >> 12;
			if (type_trame==0x00)
			{
				pty = (int) (si4703_registers[RDSB] & 0x03E0) >> 5;
				return pty;
			}
			
		}
		else {
			delay(30); //From AN230, using the polling method 40ms should be sufficient amount of time between checks
		}
	}
	if (millis() >= endTime) {
		
		return 0;
	}
}




void Si4703_Breakout::readCT(int* buffer){	
	long timeout=70000; //CT is transmitted one time per minute...very long timeout
	unsigned long  endTime = millis() + timeout;
	int hour_utc;
	int minute;
	int offset_direction_utc;
	int offset_utc;
	int hour_local;
	int time;
	long date_MJD;
	int Y;
	int M;
	int K;
	int year;
	int month;
	int day;
	int date;
	int select;
	while(millis() < endTime) {
		readRegisters();
		if(si4703_registers[STATUSRSSI] & (1<<RDSR)){
			byte type_trame=(si4703_registers[RDSB] & 0xF800) >> 8;
			if (type_trame==0x40)
			{
			  hour_utc= (int) (((si4703_registers[RDSC] & 0x0001) << 4)  | ((si4703_registers[RDSD] & 0xF000) >> 12));
			  minute=(int) ((si4703_registers[RDSD] & 0xFC0)>>6);
			  offset_direction_utc=(int) (si4703_registers[RDSD] & 0x0020>>5);
			  offset_utc=(int) (si4703_registers[RDSD] & 0x001F);
			  offset_utc=offset_utc/2;
			  if(offset_direction_utc==0) {
			    hour_local=(hour_utc+offset_utc)%24;
			  } else {
				hour_local=(hour_utc-offset_utc+24)%24;
			  }
				buffer[0]=hour_local;
				buffer[1]=minute;

			  // date calculated from MJD format to year-month-day format
			  // from US RDBS StandarD - April 9 - 1998 
			  // ftp://ftp.rds.org.uk/pub/acrobat/rbds1998.pdf page 103
				
			  date_MJD= (long) ( ((si4703_registers[RDSB]&0x0001)<<15) | ((si4703_registers[RDSC]&0xFFFE)>>1) );
			  Y = (int) ((date_MJD - 15078.2) / 365.25) ;
			  M=(int) (Y * 365.25);
			  M=date_MJD - 14956.1 - M;
			  M=(int) (M / 30.6001);
              if (M==14 || M==15) {
                K=1;
              }else{
                K=0;
              }        
			  year = Y + K;
			  buffer[2]=year+1900;
			  month = M - 1 - K * 12;
			  buffer[3]=month;
			  day = date_MJD - 14956 - (int) ( Y * 365.25 ) - (int) (M * 30.6001) ;
			  buffer[4]=day;;        
			  return ;
			}
		}
		else {
			delay(30); //From AN230, using the polling method 40ms should be sufficient amount of time between checks
		}
	}
	if (millis() >= endTime || select == LOW){
		return ;
	}
}





int Si4703_Breakout::getRSSI(){	
	readRegisters();
	int value=(si4703_registers[STATUSRSSI] & 0x00FF) ;
	return value;
}




//To get the Si4703 inito 2-wire mode, SEN needs to be high and SDIO needs to be low after a reset
//The breakout board has SEN pulled high, but also has SDIO pulled high. Therefore, after a normal power up
//The Si4703 will be in an unknown state. RST must be controlled
boolean Si4703_Breakout::si4703_init() 
{
  pinMode(_resetPin, OUTPUT);
  pinMode(_sdioPin, OUTPUT); //SDIO is connected to A4 for I2C
  digitalWrite(_sdioPin, LOW); //A low SDIO indicates a 2-wire interface
  digitalWrite(_resetPin, LOW); //Put Si4703 into reset
  delay(1); //Some delays while we allow pins to settle
  digitalWrite(_resetPin, HIGH); //Bring Si4703 out of reset with SDIO set to low and SEN pulled high with on-board resistor
  delay(1); //Allow Si4703 to come out of reset

  Wire.begin(); //Now that the unit is reset and I2C inteface mode, we need to begin I2C

  if (!readRegisters()){
	  return 0; //Read the current register set
  }
  //si4703_registers[0x07] = 0xBC04; //Enable the oscillator, from AN230 page 9, rev 0.5 (DOES NOT WORK, wtf Silicon Labs datasheet?)
  si4703_registers[0x07] = 0x8100; //Enable the oscillator, from AN230 page 9, rev 0.61 (works)
  updateRegisters(); //Update

  delay(500); //Wait for clock to settle - from AN230 page 9

  readRegisters(); //Read the current register set
  si4703_registers[POWERCFG] = 0x4001; //Enable the IC
  //  si4703_registers[POWERCFG] |= (1<<SMUTE) | (1<<DMUTE); //Disable Mute, disable softmute
  si4703_registers[POWERCFG] |= (1<<MONO);// sortie mono forcee
  si4703_registers[SYSCONFIG1] |= (1<<RDS); //Enable RDS

  si4703_registers[SYSCONFIG1] |= (1<<DE); //50kHz Europe setup
  si4703_registers[SYSCONFIG2] |= (1<<SPACE0); //100kHz channel spacing for Europe

  si4703_registers[SYSCONFIG2] &= 0xFFF0; //Clear volume bits
  si4703_registers[SYSCONFIG2] |= 0x0001; //Set volume to lowest
	
  updateRegisters(); //Update

  delay(110); //Max powerup time, from datasheet page 13
	 
	return 1;
}



//Read the entire register control set from 0x00 to 0x0F
boolean Si4703_Breakout::readRegisters(){

  //Si4703 begins reading from register upper register of 0x0A and reads to 0x0F, then loops to 0x00.
  Wire.requestFrom(SI4703, 32); //We want to read the entire register set from 0x0A to 0x09 = 32 bytes.

  long endTime = millis() + 1000;	
  while(Wire.available() < 32 && millis() < endTime) ; //Wait for 16 words/32 bytes to come back from slave I2C device

  //Remember, register 0x0A comes in first so we have to shuffle the array around a bit
  for(int x = 0x0A ; ; x++) { //Read in these 32 bytes
    if(x == 0x10) x = 0; //Loop back to zero
    si4703_registers[x] = Wire.read() << 8;
    si4703_registers[x] |= Wire.read();
    if(x == 0x09) break; //We're done!
  }
  if (millis() >= endTime) {
	return 0;
  }
  return 1;
}



//Write the current 9 control registers (0x02 to 0x07) to the Si4703
//It's a little weird, you don't write an I2C addres
//The Si4703 assumes you are writing to 0x02 first, then increments
byte Si4703_Breakout::updateRegisters() {

  Wire.beginTransmission(SI4703);
  //A write command automatically begins with register 0x02 so no need to send a write-to address
  //First we send the 0x02 to 0x07 control registers
  //In general, we should not write to registers 0x08 and 0x09
  for(int regSpot = 0x02 ; regSpot < 0x08 ; regSpot++) {
    byte high_byte = si4703_registers[regSpot] >> 8;
    byte low_byte = si4703_registers[regSpot] & 0x00FF;

    Wire.write(high_byte); //Upper 8 bits
    Wire.write(low_byte); //Lower 8 bits
  }

  //End this transmission
  byte ack = Wire.endTransmission();
  if(ack != 0) { //We have a problem! 
    return(FAIL);
  }

  return(SUCCESS);
}


//Seeks out the next available station
//Returns the freq if it made it
//Returns zero if failed
int Si4703_Breakout::seek(byte seekDirection){
  readRegisters();
  //Set seek mode wrap bit
  //si4703_registers[POWERCFG] |= (1<<SKMODE); //Allow wrap
  si4703_registers[POWERCFG] &= ~(1<<SKMODE); //Disallow wrap - if you disallow wrap, you may want to tune to 87.5 first
  if(seekDirection == SEEK_DOWN) si4703_registers[POWERCFG] &= ~(1<<SEEKUP); //Seek down is the default upon reset
  else si4703_registers[POWERCFG] |= 1<<SEEKUP; //Set the bit to seek up

  si4703_registers[POWERCFG] |= (1<<SEEK); //Start seek
  updateRegisters(); //Seeking will now start

  long endTime = millis() + 4000;
  //Poll to see if STC is set
  while(millis() < endTime) {
    readRegisters();
    if((si4703_registers[STATUSRSSI] & (1<<STC)) != 0) break; //Tuning complete!
  }

  readRegisters();
  int valueSFBL = si4703_registers[STATUSRSSI] & (1<<SFBL); //Store the value of SFBL
  si4703_registers[POWERCFG] &= ~(1<<SEEK); //Clear the seek bit after seek has completed
  updateRegisters();


  //Wait for the si4703 to clear the STC as well
  while(millis() < endTime) {
    readRegisters();
    if( (si4703_registers[STATUSRSSI] & (1<<STC)) == 0) break; //Tuning complete!
  }

  if(valueSFBL) { //The bit was set indicating we hit a band limit or failed to find a station
    return(0);
  }
return getChannel();
}



//Reads the current channel from READCHAN
//Returns a number like 973 for 97.3MHz
int Si4703_Breakout::getChannel() {
  readRegisters();
  int channel = si4703_registers[READCHAN] & 0x03FF; //Mask out everything but the lower 10 bits
  //Freq(MHz) = 0.100(in Europe) * Channel + 87.5MHz
  //X = 0.1 * Chan + 87.5
  channel += 875; //98 + 875 = 973
  return(channel);
}
