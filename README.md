# Declaration of Intent

So this isn't a fork that is intended to have anythgin to contribute back to the source repo. Instead the intent is to cutaway things until I personally understand more about how this works, such that I can make a new program that does a simlar thing. The ide is that I can keep the interesting part of the program working, so I know I haven't broken that part.

Specifically, I want to get to the point that I have a program that just feeds generated test data to an instance of Dolphin, ideally eventually on the same machine as the program is running on, to simplify things.

I want to do this as a step in getting a program that woudl ultimately getting its motion data from an entirely different source than a Steam Deck.

I attempted to try and get a working version of a program that jsut spews test data into Dolphin workign from scratch and failed. Specifically I only got as far as causing Dolphin to log errors about the crc value being bad, and not getting anything to show up in the devices list.

Dissecting things like this is slow and tedious, but has the considerable advantage of starting from something that demonstrably works, and maintaining that it works as we go, meaning we don't have to ever deal with something not workign for large mysterious, untrackable reasons. If it stops working, it will be due to a change, we made, and we can back out to a working state, and examine the diff to understand the change. THis is much, much nicer than needing to deal with things not working and not knowing where to look for the answer!

Original README below

----

# SteamDeckGyroDSU
**DSU** (*cemuhook protocol*) server for motion data for **Steam Deck**.

## Install/Update

Open this page in the browser in **Steam Deck**'s desktop mode.

Download [Installation File](https://github.com/kmicki/SteamDeckGyroDSU/releases/latest/download/update-sdgyrodsu.desktop), save it to Desktop and run it by touching or double-clicking *Update GyroDSU*.

This Desktop shortcut may be used also to update the *SteamDeckGyroDSU* to the most recent version.

To uninstall, run *Uninstall GyroDSU* from Desktop by touching or double-clicking.
    
## Usage

Server is running as a service. It provides motion data for cemuhook at Deck's IP address and UDP port *26760*.

Optionally, another UDP server port may be specified in an environment variable **SDGYRO_SERVER_PORT**.

**Remark:** The server provides only motion data. Remaining controls (buttons/axes) are not provided.

### Client (emulator) Configuration

See [Client Configuration](https://github.com/kmicki/SteamDeckGyroDSU/wiki/Client-Configuration) wiki page for instructions on how to configure client applications (emulators).

## Reporting problems

Before reporting problems make sure you are running the most recent version of **SteamDeckGyroDSU** (see *Install/Update* section above).

When reporting a problem or an issue with the server, please generate a log file with following command:

    $HOME/sdgyrodsu/logcurrentrun.sh > sdgyrodsu.log
    
File `sdgyrodsu.log` will be generated in current directory. Attach it to the issue describing the problem.

## Alternative installation

To install the server using a binary package provided in a release, see [wiki page](https://github.com/kmicki/SteamDeckGyroDSU/wiki/Alternative-installation-instructions).

To build the server from source on Deck and install it, see [wiki page](https://github.com/kmicki/SteamDeckGyroDSU/wiki/Build-and-install-from-source).
