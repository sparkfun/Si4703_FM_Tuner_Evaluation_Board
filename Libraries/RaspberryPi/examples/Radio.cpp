/*
Copyright (C) 2013 Christoph Thoma (thoma.christoph@gmx.at)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "../src/SparkFunSi4703.h"
#include <stdio.h>

int main() {
  int resetPin = 23;  // GPIO_23.
  int sdaPin = 0;     // GPIO_0 (SDA).

  Si4703_Breakout radio(resetPin, sdaPin);
  radio.powerOn();
  radio.setVolume(5);
  radio.setChannel(1040);  // FM4 104.0Mhz (Austria).

  char rdsBuffer[10] = {0};
  radio.readRDS(rdsBuffer, 15000);  // timeout 15sec.
  printf("Listening to station: %s %.1f\n", rdsBuffer,
         radio.getChannel() / 10.0);

  radio.printRegisters();

  return 0;
}
