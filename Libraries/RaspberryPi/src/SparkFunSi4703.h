//
// Original work Copyright 09.09.2011 Nathan Seidle (SparkFun)
// Modified work Copyright 11.02.2013 Aaron Weiss (SparkFun)
// Modified work Copyright 13.09.2013 Christoph Thoma
//

/*
Initial Raspberry Pi port by Christoph Thoma, 13/09/2013

2/11/13 Edited by Aaron Weiss @ SparkFun

Library for Sparkfun Si4703 breakout board.
Simon Monk. 2011-09-09

This is a library wrapper and a few extras to the excellent code produced
by Nathan Seidle from Sparkfun (Beerware).

Nathan's comments......

Look for serial output at 57600bps.

The Si4703 ACKs the first byte, and NACKs the 2nd byte of a read.

1/18 - after much hacking, I suggest NEVER write to a register without first
reading the contents of a chip.
ie, don't updateRegisters without first readRegisters.

If anyone manages to get this datasheet downloaded
http://wenku.baidu.com/view/d6f0e6ee5ef7ba0d4a733b61.html
Please let us know. It seem to be the latest version of the programming guide.
It had a change on page 12 (write 0x8100 to 0x07)
that allowed me to get the chip working..

Also, if you happen to find "AN243: Using RDS/RBDS with the Si4701/03", please
share. I love it when companies refer to
documents that don't exist.

1/20 - Picking up FM stations from a plane flying over Portugal! Sweet! 93.9MHz
sounds a little soft for my tastes,s but
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

The Si4703 breakout does work with line out into a stereo or other amplifier. Be
sure to test with different length 3.5mm
cables. Too short of a cable may degrade reception.
*/

#ifndef SparkFunSi4703_h
#define SparkFunSi4703_h

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <inttypes.h>

enum class Region { US, Europe, Japan };

enum class Status { SUCCESS, FAIL };

enum class SeekDirection { Up, Down };

class Si4703_Breakout {
 public:
  Si4703_Breakout(int resetPin, int sdioPin, Region region = Region::US);
  ~Si4703_Breakout();

  // Power on the radio.
  Status powerOn();

  // Power off the radio.
  void powerOff();

  // Tune the radio to the specified |frequency| in MHz (i.e. 93.5).
  void setFrequency(float freqency);

  // Seek the radio in the specified |direction|. Returns the new station
  // frequency or 0 of seek failed.
  float seek(SeekDirection direction);

  // Set the radio volume (0..15).
  void setVolume(int volume);

  // Read the current RDS characters into the |message| buffer.
  // |message| must be at least 9 chars. |message| will be null terminated.
  // This method is thread safe.
  void getRDS(char* message);

  // The condition variable to use to be signalled by the RDS thread that there
  // is new RDS data in the buffer. When signalled call getRDS() to retrieve the
  // characters.
  std::condition_variable& rdsCV() { return rds_cv_; }

  // Return the currently tuned frequency.
  float getFrequency();

  // Print the shadow register values to stdout. Does not refresh the shadow
  // registers before printing.
  void printRegisters();

  // Read the registers from the radio into the shadow registers.
  Status readRegisters();

  // The channel spacing (in MHz) between channels.
  float channelSpacing() const;

  // The minimum (lowest) frequency possible with the current band.
  float minFrequency() const;

  // The portions of the DEVICEID/CHIPID registers shifted accordingly.
  uint16_t manufacturer() const;
  uint16_t part() const;
  uint16_t firmware() const;
  uint16_t device() const;
  uint16_t revision() const;
  int signalStrength() const;
  uint16_t blockAErrors() const;

  // The human-readable decoded DEVICEID/CHIPID register values.
  std::string manufacturer_str() const;
  std::string part_str() const;
  std::string firmware_str() const;
  std::string device_str() const;
  std::string revision_str() const;
  std::string blockAErrors_str() const;

 private:
  // See AN230 Programmers Guide section 3.4.1 for bands. Band is bits 6:7 in
  // SYSCONFIG2 register.
  enum Band : uint16_t {
    Band_US_Europe = 0b0000000,
    Band_Japan_Wide = 0b0100000,
    Band_Japan = 0b1000000,
    Band_Reserved = 0b1100000
  };

  // See AN230 Programmers Guide section 3.4.2 for channel spacing. Spacing is
  // bits 4:5 in SYSCONFIG2 register.
  enum ChannelSpacing : uint16_t {
    Spacing_200kHz = 0b00000,
    Spacing_100kHz = 0b01000,
    Spacing_50kHz = 0b10000,
    Spacing_Reserved = 0b11000,
  };

  // 0b._001.0000 = I2C address of Si4703 - note that the Wire
  // function assumes non-left-shifted I2C address, not 0b.0010.000W
  static const int SI4703 = 0x10;
  static const uint16_t I2C_FAIL_MAX = 10;  // This is the number of attempts we
                                            // will try to contact the device
                                            // before erroring out.
  // Define the register names
  static const uint16_t DEVICEID = 0x00;
  static const uint16_t CHIPID = 0x01;
  static const uint16_t POWERCFG = 0x02;
  static const uint16_t CHANNEL = 0x03;
  static const uint16_t SYSCONFIG1 = 0x04;
  static const uint16_t SYSCONFIG2 = 0x05;
  static const uint16_t STATUSRSSI = 0x0A;
  static const uint16_t READCHAN = 0x0B;
  static const uint16_t RDSA = 0x0C;
  static const uint16_t RDSB = 0x0D;
  static const uint16_t RDSC = 0x0E;
  static const uint16_t RDSD = 0x0F;

  // Register 0x02 - POWERCFG
  static const uint16_t SMUTE = 1 << 15;
  static const uint16_t DMUTE = 1 << 14;
  static const uint16_t SKMODE = 1 << 10;
  static const uint16_t SEEKUP = 1 << 9;
  static const uint16_t SEEK = 1 << 8;

  // Register 0x03 - CHANNEL
  static const uint16_t TUNE = 1 << 15;

  // Register 0x04 - SYSCONFIG1
  static const uint16_t RDS = 1 << 12;
  static const uint16_t DE = 1 << 11;

  // Register 0x0A - STATUSRSSI
  static const uint16_t RDSR = 1 << 15;
  static const uint16_t STC = 1 << 14;    // Seek/Tune Complete.
  static const uint16_t SFBL = 1 << 13;   // Seek Fail/Band Limit.
  static const uint16_t AFCRL = 1 << 12;  // AFC Rail.
  static const uint16_t RDSS = 1 << 11;   // RDS Synchronized.
  static const uint16_t STEREO = 1 << 8;  // Stereo Indicator.
  // RSSI (Received Signal Strength Indicator).
  static const uint16_t RSSI_MASK = 0xf;
  static const uint16_t BLERA_MASK = 0b11000000000;  // RDS Block A Errors.

  // Register 0x00 - DEVICEID
  // https://www.silabs.com/documents/public/data-sheets/Si4702-03-C19.pdf
  static const uint16_t MANUFACTURER_MASK = 0b111111111111;
  static const uint16_t PART_MASK = 0b1111000000000000;

  // Register 0x01 - CHIPID
  // https://www.silabs.com/documents/public/data-sheets/Si4702-03-C19.pdf
  static const uint16_t FIRMWARE_MASK = 0b111111;
  static const uint16_t DEVICE_MASK = 0b1111000000;
  static const uint16_t REVISION_MASK = 0b1111110000000000;

  static const char* registerName(uint16_t idx);

  Status updateRegisters();
  float channelToFrequency(uint16_t channel) const;
  uint16_t frequencyToChannel(float frequency) const;
  void rdsReadFunc();
  void stopRDSThread();
  void clearRDSBuffer();

  int resetPin_;
  int sdioPin_;
  std::mutex shadow_reg_mutex_;  // Synchronize access to shadow_reg_.
  uint16_t shadow_reg_[16];      // There are 16 registers, each 16 bits large.
  int si4703_fd_;                // I2C file descriptor.
  Region region_;
  Band band_;
  std::mutex rds_data_mutex_;  // protect both RDS variables below.
  char rds_chars_[9];          // The current RDS characters.
  // The last time a pair of chars was valid.
  std::chrono::time_point<std::chrono::system_clock> rds_last_valid_[4];
  std::unique_ptr<std::thread> rds_thread_;
  std::condition_variable rds_cv_;
  std::atomic<bool> run_rds_thread_;
  ChannelSpacing channel_spacing_;
};

#endif
