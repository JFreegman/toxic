# Toxic [![Build Status](https://travis-ci.org/Tox/toxic.png?branch=master)](https://travis-ci.org/Tox/toxic)
Toxic is an ncurses-based instant messaging client for [Tox](https://tox.im) which formerly resided in the [Tox core repository](https://github.com/irungentoo/toxcore), and is now available as a standalone application.

![Toxic Screenshot](http://i.imgur.com/hL7WhVl.png "Main Screen").

## Installation

### Dependencies
##### Base
* [libtoxcore](https://github.com/irungentoo/toxcore)
* [ncurses](https://www.gnu.org/software/ncurses) (for Debian based systems 'libncursesw5-dev')

##### Audio
* libtoxav (libtoxcore compiled with audio support)
* [openal](http://openal.org)

### Compiling
1. `cd build/`
2. `make`
3. `sudo make install DESTDIR="/where/to/install"`

### Compilation Notes
* You can add specific flags to the Makefile with `USER_CFLAGS=""` and/or `USER_LDFLAGS=""`
* You can pass your own flags to the Makefile with `CFLAGS=""` and/or `LDFLAGS=""` (this will supersede the default ones)
* Audio call support is automatically enabled if all dependencies are found

### Troubleshooting
If your default prefix is "/usr/local" and you receive the following:
```
error while loading shared libraries: libtoxcore.so.0: cannot open shared object file: No such file or directory
```
you can attempt to correct it by running `sudo ldconfig`. If that doesn't work, run:
```
echo '/usr/local/lib/' | sudo tee -a /etc/ld.so.conf.d/locallib.conf
sudo ldconfig
```

## Settings
Running Toxic for the first time creates an empty file called toxic.conf in your home configuration directory ("~/.config/tox" for Linux users). Adding options to this file allows you to enable auto-logging, change the time format (12/24 hour), and much more.
You can view our example config file [here](misc/toxic.conf).

