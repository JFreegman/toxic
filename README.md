# Toxic [![Build Status](https://travis-ci.org/Tox/toxic.png?branch=master)](https://travis-ci.org/Tox/toxic)
Toxic is a [Tox](https://tox.chat)-based instant messenging client which formerly resided in the [Tox core repository](https://github.com/irungentoo/toxcore), and is now available as a standalone application.

[![Toxic Screenshot](https://i.imgur.com/san99Z2.png "Home Screen")](https://i.imgur.com/san99Z2.png)

## Installation
[Use our repositories](https://wiki.tox.chat/binaries#other_linux)<br />
[Compile it yourself](/INSTALL.md)

## Settings
Running Toxic for the first time creates an empty file called toxic.conf in your home configuration directory ("~/.config/tox" for Linux users). Adding options to this file allows you to enable auto-logging, change the time format (12/24 hour), and much more.
You can view our example config file [here](misc/toxic.conf.example).

## Troubleshooting
If your default prefix is "/usr/local" and you receive the following:
```
error while loading shared libraries: libtoxcore.so.0: cannot open shared object file: No such file or directory
```
you can attempt to correct it by running `sudo ldconfig`. If that doesn't work, run:
```
echo '/usr/local/lib/' | sudo tee -a /etc/ld.so.conf.d/locallib.conf
sudo ldconfig
```

