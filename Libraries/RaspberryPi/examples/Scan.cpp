#include "../src/SparkFunSi4703.h"
#include <thread>
#include <iostream>

using std::cout;
using std::endl;

int main(int argc, const char** argv) {
  int resetPin = 23;  // GPIO_23.
  int sdaPin = 0;     // GPIO_0 (SDA).

  Si4703_Breakout radio(resetPin, sdaPin);
  radio.powerOn();
  radio.setVolume(5);

  // Increment integer kHz values to avoid cumulative floating point errors.
  int minFreq_kHz = 1000.0 * radio.minFrequency();
  int spacing_kHz = 1000.0 * radio.channelSpacing();
  int maxFreq_kHz = 107 * 1000;
  for (int iFreq = minFreq_kHz; iFreq <= maxFreq_kHz; iFreq += spacing_kHz) {
    float fFreq = iFreq / 1000.0;
    radio.setFrequency(fFreq);

    // Sleep to get some RDS data
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    char rdsBuffer[10] = {0};
    radio.getRDS(rdsBuffer);
    cout << fFreq << " MHz \"" << rdsBuffer
         << "\" RSSI:" << radio.signalStrength() << endl;
  }

  return 0;
}
