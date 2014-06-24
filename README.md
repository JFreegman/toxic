# Toxic
Toxic is an ncurses based instant messaging client for [Tox](https://tox.im) which formerly resided in the [Tox core repository](https://github.com/irungentoo/toxcore) and is now available as a standalone program. It looks like [this](http://i.imgur.com/hL7WhVl.png).

## Installation

### Base dependencies
* libtoxcore
* ncurses (for Debian based systems: libncurses5-dev libncursesw5-dev)

### Audio dependencies
* libtoxav
* openal

### Compiling
* `cd build/`
* `make`
* `sudo make install DESTDIR="/path/you/like"`

* You can add specific flags to makefile with `USER_CFLAGS=""` and/or `USER_LDFLAGS=""`
* You can pass your own flags to makefile with `CFLAGS=""` and/or `LDFLAGS=""` (this will supersede the defaults one)

* Audio calling support is automatically enabled if all dependencies are found

### Troubleshooting
If your default prefix is "/usr/local" and you get the error:
`error while loading shared libraries: libtoxcore.so.0: cannot open shared object file: No such file or directory`
you can try fix it running `sudo ldconfig`.
If that doesn't fix it, run:
```
echo '/usr/local/lib/' | sudo tee -a /etc/ld.so.conf.d/locallib.conf
sudo ldconfig
```

## Settings
After running Toxic for the first time an empty file called toxic.conf should reside in your home configuration directory ("~/.config/tox" for Linux users). For an example on how to use this config file to save settings such as auto-logging and time format see: [misc/toxic.conf](misc/toxic.conf).
