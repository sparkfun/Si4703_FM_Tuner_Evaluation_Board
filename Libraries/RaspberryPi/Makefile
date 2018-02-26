
lib_files= src/SparkFunSi4703.cpp src/SparkFunSi4703.h

Radio: ${lib_files} examples/Radio.cpp Makefile
	g++ -std=gnu++11 -lpthread -o Radio examples/Radio.cpp src/SparkFunSi4703.cpp -lwiringPi

Scan: ${lib_files} examples/Scan.cpp Makefile
	g++ -std=gnu++11 -lpthread -o Scan examples/Scan.cpp src/SparkFunSi4703.cpp -lwiringPi

.PHONY: clean
clean:
	rm -f Radio Scan

.PHONY: run
run: Radio
	sudo ./Radio 105.7

all: Radio Scan

.PHONY: format
format:
	clang-format -i --style=Chromium ${lib_files} examples/*.cpp
