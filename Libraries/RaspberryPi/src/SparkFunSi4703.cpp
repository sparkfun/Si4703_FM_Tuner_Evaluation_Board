//
// Original work Copyright 09.09.2011 Nathan Seidle (SparkFun)
// Modified work Copyright 11.02.2013 Aaron Weiss (SparkFun)
// Modified work Copyright 13.09.2013 Christoph Thoma
//

#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wiringPi.h>

#include "SparkFunSi4703.h"

using std::cerr;
using std::cout;
using std::dec;
using std::endl;
using std::hex;
using std::setfill;
using std::setw;

namespace {

// Max powerup time, from datasheet page 13.
uint16_t MAX_POWERUP_TIME = 110;

// Delay for clock to settle - from AN230 page 9.
uint16_t CLOCK_SETTLE_DELAY = 500;

uint16_t SwapEndian(uint16_t val) {
  return (val >> 8) | (val << 8);
}

uint16_t ToLittleEndian(uint16_t val) {
  return SwapEndian(val);
}

uint16_t ToBigEndian(uint16_t val) {
  return SwapEndian(val);
}

// Determine if two float values are "equal enough" - i.e. to within some small
// value.
bool FloatsEqual(float a, float b) {
  const float epsilon = 0.02;
  return std::abs(a - b) < epsilon;
}

char ToYesNo(uint16_t val) {
  return val ? 'Y' : 'N';
}

}  // anonymous namespace

Si4703_Breakout::Si4703_Breakout(int resetPin, int sdioPin, Region region)
    : resetPin_(resetPin),
      sdioPin_(sdioPin),
      region_(region),
      run_rds_thread_(false) {
  clearRDSBuffer();
  switch (region) {
    case Region::US:
      band_ = Band_US_Europe;
      channel_spacing_ = Spacing_200kHz;
      break;
    case Region::Europe:
      band_ = Band_US_Europe;
      channel_spacing_ = Spacing_100kHz;
      break;
    case Region::Japan:
      band_ = Band_Japan_Wide;
      // TODO: verify spacing.
      channel_spacing_ = Spacing_100kHz;
      break;
  }
}

Si4703_Breakout::~Si4703_Breakout() {
  powerOff();
}

// To get the Si4703 inito 2-wire mode, SEN needs to be high and SDIO needs to
// be low after a reset. The breakout board has SEN pulled high, but also has
// SDIO pulled high. Therefore, after a normal power up the Si4703 will be in an
// unknown state. RST must be controlled
Status Si4703_Breakout::powerOn() {
  wiringPiSetupGpio();  // Setup gpio access in BCM mode.

  pinMode(resetPin_, OUTPUT);  // gpio bit-banging to get 2-wire (I2C) mode.
  pinMode(sdioPin_, OUTPUT);   // SDIO is connected to A4 for I2C.

  digitalWrite(sdioPin_, LOW);    // A low SDIO indicates a 2-wire interface.
  digitalWrite(resetPin_, LOW);   // Put Si4703 into reset.
  delay(1);                       // Some delays while we allow pins to settle.
  digitalWrite(resetPin_, HIGH);  // Bring Si4703 out of reset with SDIO set to
                                  // low and SEN pulled high with on-board
                                  // resistor.
  delay(1);                       // Allow Si4703 to come out of reset.

  // Setup I2C
  const char filename[] = "/dev/i2c-1";
  if ((si4703_fd_ = open(filename, O_RDWR)) < 0) {  // Open I2C slave device.
    perror(filename);
    return Status::FAIL;
  }

  if (ioctl(si4703_fd_, I2C_SLAVE, SI4703) < 0) {  // Set device address 0x10.
    perror("Failed to acquire bus access and/or talk to slave");
    return Status::FAIL;
  }

  if (ioctl(si4703_fd_, I2C_PEC, 1) < 0) {  // Enable "Packet Error Checking".
    perror("Failed to enable PEC");
    return Status::FAIL;
  }

  Status s = readRegisters();
  if (s != Status::SUCCESS)
    return s;

  // Enable the oscillator, from AN230 page 9, rev 0.61 (works).
  shadow_reg_[0x07] = 0x8100;
  updateRegisters();

  delay(CLOCK_SETTLE_DELAY);

  readRegisters();                 // Read the current register set.
  shadow_reg_[POWERCFG] = 0x4001;  // Enable the IC.

  shadow_reg_[SYSCONFIG1] |= RDS;  // Enable RDS.
  if (region_ == Region::Europe)
    shadow_reg_[SYSCONFIG1] |= DE;
  shadow_reg_[SYSCONFIG2] |= band_;
  shadow_reg_[SYSCONFIG2] |= channel_spacing_;
  shadow_reg_[SYSCONFIG2] &= 0xFFF0;  // Clear volume bits.
  shadow_reg_[SYSCONFIG2] |= 0x0001;  // Set volume to lowest.
  updateRegisters();

  delay(MAX_POWERUP_TIME);

  // Start the RDS reading thread.
  run_rds_thread_ = true;
  rds_thread_.reset(new std::thread(&Si4703_Breakout::rdsReadFunc, this));

  return Status::SUCCESS;
}

void Si4703_Breakout::powerOff() {
  stopRDSThread();
  readRegisters();
  shadow_reg_[POWERCFG] = 0x0000;  // Clear Enable Bit disables chip.
  updateRegisters();
}

void Si4703_Breakout::setFrequency(float frequency) {
  // See frequencyToChannel for source of equation.
  float fchannel = (frequency - minFrequency()) / channelSpacing();
  uint16_t channel = frequencyToChannel(frequency);
  if (!FloatsEqual(fchannel, channel)) {
    // The freq must be a multiple of the channel spacing and offset from the
    // minimum freq.
    cerr << "Frequency (" << frequency << " MHz) is not a valid frequency."
         << endl;
    return;
  }
  clearRDSBuffer();
  readRegisters();
  shadow_reg_[CHANNEL] &= 0xFE00;   // Clear out the channel bits.
  shadow_reg_[CHANNEL] |= channel;  // Mask in the new channel.
  shadow_reg_[CHANNEL] |= TUNE;     // Set the TUNE bit to start.
  updateRegisters();

  delay(60);  // Wait 60ms - you can use or skip this delay.

  // Poll to see if STC is set.
  while (true) {
    readRegisters();
    if ((shadow_reg_[STATUSRSSI] & STC) != 0)
      break;  // Tuning complete!
  }

  readRegisters();
  shadow_reg_[CHANNEL] &= ~TUNE;  // Clear the tune after a tune has completed.
  updateRegisters();

  // Wait for the si4703 to clear the STC as well.
  while (true) {
    readRegisters();
    if ((shadow_reg_[STATUSRSSI] & STC) == 0)
      break;  // Tuning complete!
  }
}

void Si4703_Breakout::setVolume(int volume) {
  readRegisters();
  if (volume < 0)
    volume = 0;
  if (volume > 15)
    volume = 15;
  shadow_reg_[SYSCONFIG2] &= 0xFFF0;  // Clear volume bits.
  shadow_reg_[SYSCONFIG2] |= volume;  // Set new volume.
  updateRegisters();
}

void Si4703_Breakout::getRDS(char* buffer) {
  std::lock_guard<std::mutex> lock(rds_data_mutex_);
  strcpy(buffer, rds_chars_);
}

// This is the thread function that reads the RDS data and writes it to an
// instance character buffer.
void Si4703_Breakout::rdsReadFunc() {
  while (run_rds_thread_) {
    readRegisters();
    if (!(shadow_reg_[STATUSRSSI] & RDSR)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
      continue;
    }

    // lowest order two bits of B are the word pair index.
    uint16_t b = shadow_reg_[RDSB];
    int index = b & 0b11;

    auto now = std::chrono::system_clock::now();
    if (b < 500) {
      char Dh = (shadow_reg_[RDSD] & 0xFF00) >> 8;
      char Dl = (shadow_reg_[RDSD] & 0x00FF);
      {
        std::lock_guard<std::mutex> lock(rds_data_mutex_);
        rds_chars_[index * 2] = Dh;
        rds_chars_[index * 2 + 1] = Dl;
        rds_last_valid_[index] = now;
      }
    } else {
      // If we haven't received RDS data for this character tuple in 500msec
      // then clear that tuple.
      if (now - rds_last_valid_[index] > std::chrono::milliseconds(500)) {
        rds_chars_[index * 2] = rds_chars_[index * 2 + 1] = ' ';
      }
    }

    // Notify any listener that we have RDS data.
    rds_cv_.notify_all();

    // Wait for the RDS bit to clear.
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
  }
}

void Si4703_Breakout::clearRDSBuffer() {
  std::lock_guard<std::mutex> lock(rds_data_mutex_);
  strcpy(rds_chars_, "        ");
  for (int i = 0; i < 4; i++)
    rds_last_valid_[i] =
        std::chrono::time_point<std::chrono::system_clock>::min();
}

void Si4703_Breakout::stopRDSThread() {
  if (!rds_thread_)
    return;
  run_rds_thread_ = false;
  rds_cv_.notify_all();
  rds_thread_->join();
  rds_thread_.reset();
  clearRDSBuffer();
}

// Read the entire register control set from 0x00 to 0x0F.
Status Si4703_Breakout::readRegisters() {
  uint16_t buffer[16];

  // Si4703 begins reading from upper byte of register 0x0A and reads to 0x0F,
  // then loops to 0x00.
  // We want to read the entire register set from 0x0A to 0x09 = 32 bytes.
  if (read(si4703_fd_, buffer, 32) != 32) {
    perror("Could not read from I2C slave device");
    return Status::FAIL;
  }

  // We may want some time-out error here.

  // Remember, register 0x0A comes in first so we have to shuffle the array
  // around a bit.
  std::lock_guard<std::mutex> lock(shadow_reg_mutex_);

  int i = 0;
  for (int x = 0x0A;; x++) {
    if (x == 0x10)
      x = 0;  // Loop back to zero.
    shadow_reg_[x] = ToLittleEndian(buffer[i++]);
    if (x == 0x09)
      break;  // We're done!
  }

  return Status::SUCCESS;
}

// Write the current 9 control registers (0x02 to 0x07) to the Si4703.
// It's a little weird, you don't write an I2C address.
// The Si4703 assumes you are writing to 0x02 first, then increments.
Status Si4703_Breakout::updateRegisters() {
  int i = 0;
  uint16_t buffer[6];

  {
    std::lock_guard<std::mutex> lock(shadow_reg_mutex_);

    // A write command automatically begins with register 0x02 so no need to
    // send a write-to address. First we send the 0x02 to 0x07 control
    // registers, first upper byte, then lower byte and so on. In general, we
    // should not write to registers 0x08 and 0x09.
    for (int regSpot = 0x02; regSpot < 0x08; regSpot++)
      buffer[i++] = ToBigEndian(shadow_reg_[regSpot]);
  }

  if (write(si4703_fd_, buffer, 12) < 12) {
    perror("Could not write to I2C slave device");
    return Status::FAIL;
  }

  return Status::SUCCESS;
}

uint16_t Si4703_Breakout::manufacturer() const {
  return shadow_reg_[DEVICEID] & MANUFACTURER_MASK;
}

uint16_t Si4703_Breakout::part() const {
  return (shadow_reg_[DEVICEID] & PART_MASK) >> 12;
}

uint16_t Si4703_Breakout::firmware() const {
  return shadow_reg_[CHIPID] & FIRMWARE_MASK;
}

uint16_t Si4703_Breakout::device() const {
  return (shadow_reg_[CHIPID] & DEVICE_MASK) >> 6;
}

uint16_t Si4703_Breakout::revision() const {
  return (shadow_reg_[CHIPID] & REVISION_MASK) >> 10;
}

std::string Si4703_Breakout::manufacturer_str() const {
  uint16_t mfr = manufacturer();
  if (mfr == 0x242)
    return "Silicon Labs";
  else {
    std::stringstream ss;
    ss << "<Unknown: 0x" << hex << mfr << '>';
    return ss.str();
  }
}

std::string Si4703_Breakout::part_str() const {
  if (part() == 1) {
    return "Si4700/01/02/03";
  } else {
    std::stringstream ss;
    ss << "<Unknown: 0x" << hex << part() << '>';
    return ss.str();
  }
}

std::string Si4703_Breakout::firmware_str() const {
  std::stringstream ss;
  ss << firmware();
  return ss.str();
}

std::string Si4703_Breakout::device_str() const {
  uint16_t dev = device();
  switch (dev) {
    case 0b0000:
      return "Si4700";  // or not yet powered up.
    case 0b0001:
      return "Si4702";
    case 0b1000:
      return "Si4701";
    case 0b1001:
      return "Si4703";
    default: {
      std::stringstream ss;
      ss << "<Unknown: 0x" << hex << dev << '>';
      return ss.str();
    }
  }
}

std::string Si4703_Breakout::revision_str() const {
  switch (revision()) {
    case 1:
      return "A";
    case 2:
      return "B";
    case 3:
      return "C";
    case 4:
      return "D";
    default: {
      std::stringstream ss;
      ss << "<Unknown: 0x" << hex << revision() << '>';
      return ss.str();
    }
  }
}

int Si4703_Breakout::signalStrength() const {
  return shadow_reg_[STATUSRSSI] & RSSI_MASK;
}

uint16_t Si4703_Breakout::blockAErrors() const {
  return (shadow_reg_[STATUSRSSI] & BLERA_MASK) >> 9;
}

std::string Si4703_Breakout::blockAErrors_str() const {
  uint16_t val = blockAErrors();
  switch (val) {
    case 0b00:
      return "0";
    case 0b01:
      return "1-2";
    case 0b10:
      return "3-5";
    case 0b11:
      return "6+";
    default: {
      std::stringstream ss;
      ss << "<Unknown: 0x" << hex << val << '>';
      return ss.str();
    }
  }
}

// static
const char* Si4703_Breakout::registerName(uint16_t idx) {
  switch (idx) {
    case DEVICEID:
      return "  DEVICEID";
    case CHIPID:
      return "    CHIPID";
    case POWERCFG:
      return "  POWERCFG";
    case CHANNEL:
      return "   CHANNEL";
    case SYSCONFIG1:
      return "SYSCONFIG1";
    case SYSCONFIG2:
      return "SYSCONFIG2";
    case STATUSRSSI:
      return "STATUSRSSI";
    case READCHAN:
      return "  READCHAN";
    case RDSA:
      return "      RDSA";
    case RDSB:
      return "      RDSB";
    case RDSC:
      return "      RDSC";
    case RDSD:
      return "      RDSD";
    default:
      return "  RESERVED";
  }
}

void Si4703_Breakout::printRegisters() {
  cout << "Register  Value" << endl;
  cout << "========= ================================================" << endl;

  for (int i = 0; i < 16; i++) {
    cout << registerName(i) << hex << "[0x" << i << "]: 0x" << setfill('0')
         << setw(4) << shadow_reg_[i];
    // Now the decoded supplemental data.
    switch (i) {
      case DEVICEID:
        cout << setfill('0') << " (mfr=\"" << manufacturer_str()
             << "\", part=" << part_str() << ')' << endl;
        break;
      case CHIPID:
        cout << " (firmware=" << firmware_str() << ", device=\"" << device_str()
             << "\", rev=" << revision_str() << ')' << endl;
        break;
      case READCHAN: {
        const int channel = shadow_reg_[READCHAN] & 0x03FF;
        cout << " (channel=" << dec << channel << " ("
             << channelToFrequency(channel) << "MHz))" << endl;
      } break;
      case STATUSRSSI:
        cout << " (RDSR:" << ToYesNo(shadow_reg_[i] & RDSR)
             << ", STC:" << ToYesNo(shadow_reg_[i] & STC)
             << ", SFBL:" << ToYesNo(shadow_reg_[i] & SFBL)
             << ", AFCRL:" << ToYesNo(shadow_reg_[i] & AFCRL)
             << ", RDSS:" << ToYesNo(shadow_reg_[i] & RDSS)
             << ", STEREO:" << ToYesNo(shadow_reg_[i] & STEREO)
             << ", RSSI:" << dec << signalStrength()
             << ", BLERA:" << blockAErrors_str() << ')' << endl;
        break;
      default:
        cout << endl;
    }
  }
}

// Seeks out the next available station.
// Returns the freq if it made it.
// Returns zero if failed.
float Si4703_Breakout::seek(SeekDirection direction) {
  readRegisters();
  // Set seek mode wrap bit.
  shadow_reg_[POWERCFG] |= SKMODE;  // Allow wrap.
  // shadow_reg_[POWERCFG] &= ~SKMODE; // Disallow wrap - if you
  // disallow wrap, you may want to tune to 87.5 first.
  if (direction == SeekDirection::Down)
    shadow_reg_[POWERCFG] &= ~SEEKUP;  // The default upon reset.
  else
    shadow_reg_[POWERCFG] |= SEEKUP;

  shadow_reg_[POWERCFG] |= SEEK;  // Start seek.
  updateRegisters();              // Seeking will now start.

  // Poll to see if STC is set.
  while (true) {
    readRegisters();
    if ((shadow_reg_[STATUSRSSI] & STC) != 0)
      break;  // Tuning complete!
  }

  readRegisters();
  // Store the value of SFBL.
  int valueSFBL = shadow_reg_[STATUSRSSI] & SFBL;
  // Clear the seek bit after seek has completed.
  shadow_reg_[POWERCFG] &= ~SEEK;
  updateRegisters();

  // Wait for the si4703 to clear the STC as well.
  while (true) {
    readRegisters();
    if ((shadow_reg_[STATUSRSSI] & STC) == 0)
      break;  // Tuning complete!
  }

  if (valueSFBL) {  // The bit was set indicating we hit a band limit or failed
                    // to find a station.
    return 0.0f;
  }

  return getFrequency();
}

// Return the space between channels (in MHz).
float Si4703_Breakout::channelSpacing() const {
  switch (channel_spacing_) {
    case Spacing_200kHz:
      return 0.2;
    case Spacing_100kHz:
      return 0.1;
    case Spacing_50kHz:
      return 0.05;
  }
}

float Si4703_Breakout::minFrequency() const {
  switch (band_) {
    case Band_US_Europe:
      return 87.5;
    case Band_Japan:
      return 76.0;
  }
}

// Given the |channel| value from the READCHAN registry convert it to frequency.
float Si4703_Breakout::channelToFrequency(uint16_t channel) const {
  // This formula is from the AN230 Programmers Guide, section 3.7.1.
  // https://www.silabs.com/documents/public/application-notes/AN230.pdf
  return channelSpacing() * static_cast<float>(channel) + minFrequency();
}

// Given a |frequency| convert it to a registry CHANNEL value.
uint16_t Si4703_Breakout::frequencyToChannel(float frequency) const {
  // This formula is from the AN230 Programmers Guide, section 3.7.1.
  // https://www.silabs.com/documents/public/application-notes/AN230.pdf
  // Add a small value to account for floating point rounding errors.
  const float epsilon = 0.001f;
  return static_cast<uint16_t>(epsilon +
                               (frequency - minFrequency()) / channelSpacing());
}

float Si4703_Breakout::getFrequency() {
  readRegisters();
  // Mask out everything but the lower 10 bits.
  const int channel = shadow_reg_[READCHAN] & 0x03FF;
  return channelToFrequency(channel);
}
