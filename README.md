# flashy
Code for controlling Adafruit neopixel and feather via bluetooth from OS X


Run make to build swift based osx bluetooth uart bridge... 
  flashy from flashy.swift
this code reads initiates a connection to a bluetooth uart device and then reads lines from standard in and then writes them to bluetooth uart connection
It waits for a response and writes this to standard out (newline terminated)

ledSrv.sh creates an input pipe and runs flashy tailing the pipe into flashy stdin.

So to see how all this works do the following:

In one terminal run ./ledSrv.sh this should connect to your device and display output from it.

In another terminal you can do the following:
echo "F" > ~/.ledsin

or 
./random.sh

this will use the named pipe ~/.ledsin that ledSrv.sh should have created and send data to drive the leds on the remote device.

see the arduino code for the remote device code.
 
