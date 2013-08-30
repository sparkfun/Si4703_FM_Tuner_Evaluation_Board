
#include <Si4703_Breakout.h>
#include <Wire.h>

//pinout from arduino micro
int resetPin = 4;
int SDIO = 2;
int SCLK = 3;

//comment or uncomment US or Europe version

//Europe version
//Si4703_Breakout radio(resetPin, SDIO, SCLK,0);

//US version
Si4703_Breakout radio(resetPin, SDIO, SCLK,1);

//end Europe or US version


int channel=927;
int volume=10;

void setup()

{
  
  Serial.begin(9600);
  delay(1000);
  Serial.println("\n\nSi4703_Breakout Test Sketch");
  Serial.println("===========================");
  Serial.println("+ - Volume (max 15)");
  Serial.println("u d Seek up / down");
  Serial.println("r display RDS Data");
  Serial.println("Send me a command letter.");
  

  radio.powerOn();
  radio.setVolume(volume);
  radio.setChannel(channel);
  displayInfo();
  
}

void loop()
{
  if (Serial.available())
  {
    char ch = Serial.read();
    if (ch == 'u')
    {
      channel = radio.seekUp();
      displayInfo();
    }
    else if (ch == 'd')
    {
      channel = radio.seekDown();
      displayInfo();
    }
    else if (ch == '+')
    {
      volume ++;
      if (volume == 16) volume = 15;
      radio.setVolume(volume);
      displayInfo();
    }
    else if (ch == '-')
    {
      volume --;
      if (volume < 0) volume = 0;
      radio.setVolume(volume);
      displayInfo();
    }
    else if (ch == 'r')
    {  
        //get signal level
        int RSSI=radio.getRSSI();
        Serial.print("signal level :");
        Serial.println(RSSI);
        
        //get programme service
        char PS[8];
        radio.readPS(PS);
        Serial.print("programme service :");
        Serial.println(PS);
        
        //get radio text
        Serial.println("Retreiving RDS Data please wait...(15sec max)");
        char RT[64];
        radio.readRT(RT);
        Serial.print("radio text :");
        Serial.println(RT);  
    
        //get programme ID
        long PId=radio.readPI();
        Serial.print("programme ID :");
        Serial.println(PId, HEX);  
        
        //get programme type 
        //Num 	Europe 	                        North America
        //0 	No programme type defined 	No programme type defined
        //1 	News 	                        News
        //2 	Current affairs 	        Information
        //3 	Information 	                Sport
        //4 	Sport 	                        Talk
        //5 	Education 	                Rock
        //6 	Drama 	                        Classic Rock
        //7 	Culture 	                Adult Hits
        //8 	Science 	                Soft Rock
        //9 	Varied 	                        Top 40
        //10 	Popular Music (Pop) 	        Country Music
        //11 	Rock Music 	                Oldies (Music)
        //12 	Easy Listening 	                Soft Music
        //13 	Light Classical 	        Nostalgia
        //14 	Serious Classical 	        Jazz
        //15 	Other Music 	                Classical
        //16 	Weather 	                Rhythm & Blues
        //17 	Finance 	                Soft Rhythm & Blues
        //18 	Children's Programmes 	        Language
        //19 	Social Affairs 	                Religious Music
        //20 	Religion 	                Religious Talk
        //21 	Phone-in 	                Personality
        //22 	Travel 	                        Public
        //23 	Leisure 	                College
        //24 	Jazz Music 	                Not assigned
        //25 	Country Music 	                Not assigned
        //26 	National Music           	Not assigned
        //27 	Oldies Music 	                Not assigned
        //28 	Folk Music 	                Not assigned
        //29 	Documentary 	                Weather
        //30 	Alarm Test 	                Emergency Test
        //31 	Alarm 	                        Emergency 
        int PTY=radio.readPTY();
        Serial.print("programme type :");
        Serial.println(PTY);  
    
        //get traffic programme
        int TP=radio.readTP();
        Serial.print("traffic programme :");
        Serial.println(TP);  
     
        //get traffic announcement
        int TA=radio.readTA();
        Serial.print("traffic announcement :");
        Serial.println(TA);    
        
        
        //get alternative frequencies
        int Alternative_frequencies[25]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        radio.readAF(Alternative_frequencies);
        Serial.print("alternative frequencies x10 Mhz :");
        for (int i = 0; i < 25; i++) {
           if(Alternative_frequencies[i]==0) {
               break;
           } 
           Serial.print(Alternative_frequencies[i]); 
           Serial.print(" ");    
        }
        Serial.println("");
        
        //get clock time
        Serial.println("Retreiving RDS Data please wait...(70sec max)");
        int CT[5]={0,0,0,0,0};
        radio.readCT(CT);
        Serial.print("Clock time : ");
        //hour
        Serial.print(CT[0]); 
        Serial.print(":");
        //minute
        Serial.print(CT[1]);    
        Serial.print(" ");
        //day
        Serial.print(CT[4]);
        Serial.print("/");
        //month
        Serial.print(CT[3]);
        Serial.print("/");
        //year
        Serial.print(CT[2]);
        
    }
  }
}

void displayInfo()
{
   
   char PS[8];
   radio.readPS(PS);
   Serial.println("");
   Serial.println("");
   Serial.print("Channel:"); 
   Serial.print(channel);
   Serial.print(" - ");
   Serial.print(PS);
   Serial.print(" (signal level :");
   int RSSI=radio.getRSSI();
   Serial.print(RSSI);
   Serial.println(")");
   
   Serial.print(" Volume:"); Serial.println(volume);
}
