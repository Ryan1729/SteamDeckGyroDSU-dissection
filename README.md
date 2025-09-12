# Declaration of Intent

So this isn't a fork that is intended to have anything to contribute back to the source repo. Instead the intent is to cutaway things until I personally understand more about how this works, such that I can make a new program that does a simlar thing. The ide is that I can keep the interesting part of the program working, so I know I haven't broken that part.

Specifically, I want to get to the point that I have a program that just feeds generated test data to an instance of Dolphin, ideally eventually on the same machine as the program is running on, to simplify things.

I want to do this as a step in getting a program that woudl ultimately getting its motion data from an entirely different source than a Steam Deck.

I attempted to try and get a working version of a program that jsut spews test data into Dolphin workign from scratch and failed. Specifically I only got as far as causing Dolphin to log errors about the crc value being bad, and not getting anything to show up in the devices list.

Dissecting things like this is slow and tedious, but has the considerable advantage of starting from something that demonstrably works, and maintaining that it works as we go, meaning we don't have to ever deal with something not workign for large mysterious, untrackable reasons. If it stops working, it will be due to a change, we made, and we can back out to a working state, and examine the diff to understand the change. THis is much, much nicer than needing to deal with things not working and not knowing where to look for the answer!

----

# Current Plan

(Subject to revision, abandonment, and/or lack of completed items being tracked as such)

* Get an identifiably different version built and onto my steam deck
    * get a version building ✔
        * needed to make a change to get it to compile
    * modify the launch script to use a local exe instead of downloading it ✔
    * confirm changes didn't break anything ✘
        They did!
    * get original version building on steam deck, and confirm built version works ✔
        Not saved to repo
* Replace the bits that read the sensors with parts that just constantly feed varying dummy data across the socket
* Run it on the same machine as Dolphin, and confirm that all works still
* Repeatedly cut away chaff until there's little enough left that the conceptual steps are clear
    * Consider making a sequence diagram based on my understanding
* Take that understanding to a different project

----

# Dissection Build Steps

These are quick reference notes on how to build for the purposes of dissection.

`create_package.sh` makes a zip in `pkgbin/SteamDeckGyroDSU.zip`

If building elsewhere, one way to get it over to the steam deck is to run an HTTP server in tat directory on the local network
But currently that version doesn't work, so instead you can build it with `create_package.sh`, then run the `install.sh` script in pkgbin/SteamDeckGyroDSU

----

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
