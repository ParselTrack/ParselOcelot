### ParselOcelot

[![Build Status](https://secure.travis-ci.org/ParselTrack/ParselOcelot.png)](http://travis-ci.org/ParselTrack/ParselOcelot)

This is a fork of the What.CD tracker Ocelot to rewrite it to use a proper Tracker <-> Frontend communications API. This is an example implementation of the appropriate use of the API, which this project intends to publish as an open standard.

See https://github.com/ParselTrack/Specifications for the latest version of the API spec.

##### Installation (Debian/Ubuntu)

```
~$ export OCELOT_SRC_DIR="/opt/ParselOcelot"
~$ export OCELOT_INSTALL_DIR="/usr/bin"
~$ sudo apt-get install libboost-system-dev libboost-thread-dev libev-dev
~$ git clone git://github.com/ParselTrack/ParselOcelot.git $OCELOT_SRC_DIR
~$ cd $OCELOT_SRC_DIR
~$ editor src/config.h
~$ ./waf configure
~$ ./waf
~$ cp build/src/ocelot $OCELOT_INSTALL_DIR
~$ cd -
```