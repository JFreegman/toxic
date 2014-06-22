# Toxic

Toxic is an ncurses based instant messaging client for [Tox](http://tox.im) which formerly resided in the [Tox core repository](https://github.com/irungentoo/ProjectTox-Core) and is now available as a standalone program. It looks like [this](http://i.imgur.com/hL7WhVl.png).
## Installation
* Generate the configure script by running the ```autoreconf -i``` command.

* Execute the configure script with ```./configure``` (you may need to pass it the location of your dependency libraries, i.e.):
```./configure --prefix=/where/to/install --with-libtoxcore-headers=/path/to/ProjectTox-Core/toxcore --with-libtoxcore-libs=/path/to/ProjectTox-Core/build/.libs --with-libsodium-headers=/path/to/libsodium/include/ --with-libsodium-libs=/path/to/sodiumtest/lib/ ```

* Audio calling support requires openal installed
* Compile with --disable-av to build without audio call support
* Compile and install the program with ```make && sudo make install```

#### Notes
If your default prefix is /usr/local and you get the error: "error while loading shared libraries: libtoxcore.so.0: cannot open shared object file: No such file or directory", then you can try running ```sudo ldconfig```. If that doesn't fix it, run:
```
echo '/usr/local/lib/' | sudo tee -a /etc/ld.so.conf.d/locallib.conf
sudo ldconfig
```
If you dont already have them, you may need to install the ncurses libraries. For Debian based systems:
```
sudo apt-get install libncurses5-dev libncursesw5-dev
```
## Settings
After running Toxic for the first time an empty file called toxic.conf should reside in your home configuration directory (~/.config/tox for Linux users). For an example on how to use this config file to save settings such as auto-logging and time format see: toxic/misc/toxic.conf
