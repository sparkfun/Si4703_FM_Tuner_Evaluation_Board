SparkFun Si4703 Raspberry Pi Library
========================================

![SparkFun Si4703](https://cdn.sparkfun.com//assets/parts/6/2/3/5/11083-02.jpg)

[*SparkFun Si4703 Breakout(BOB-11083)*](https://www.sparkfun.com/products/11083)

Basic functionality of the Si4703 FM tuner chip.
Allows user to tune to different FM stations, find station ID and song name, and
RDS and RBDS information.

Repository Contents
-------------------

* **/examples** - Sample program using the library to control the tuner chip.
* **/src** - Source files for the library (.cpp, .h).

Usage
-----

You have to install <a href="http://wiringpi.com/download-and-install/">wiringPi</a> first. SDA/SDIO and Reset from the breakout board should be wired to GPIO0 and GPIO23 respectively.
<pre>
	<code>
git clone https://github.com/cthoma/rpi-si4703.git
cd rpi-si4703/
gcc -o Radio example/Radio.cpp Si4703_Breakout.cpp -lwiringPi
sudo ./Radio
	</code>
</pre>
(Hint: If you're not from europe, you have to change the si4703_init() function
accordingly)

Products that use this Library 
---------------------------------
* [SparkFun FM Tuner Evaluation Board](https://www.sparkfun.com/products/10663)- Evaluation
  board for Si4703. Includes audio jack. 
* [SparkFun FM Tuner Basic Breakout](https://www.sparkfun.com/products/11083)- Basic
  breakout of Si4703.


License Information
-------------------

This product is _**open source**_! 

Distributed as-is; no warranty is given.

- Your friends at SparkFun.

_The original SparkFun code was modified by Simon Monk. Thanks Simon!_

The Raspberry Pi
src/SparkFunSi4703.cpp and src/SparkFunSi4703.cpp are open source so please feel free to do anything you want with it;
you buy me a beer if you use this and we meet someday. 
(<a href="http://en.wikipedia.org/wiki/Beerware">Beerware license</a>)
