# Toxic [![Build Status](https://travis-ci.org/Tox/toxic.png?branch=master)](https://travis-ci.org/Tox/toxic)
Toxic is a [Tox](https://tox.im)-based instant messenging client which formerly resided in the [Tox core repository](https://github.com/irungentoo/toxcore), and is now available as a standalone application.

![Toxic Screenshot](https://i.imgur.com/san99Z2.png "Home Screen")

## Downloads
You can download precompiled binaries from [jenkins](https://jenkins.libtoxcore.so):
* [Linux (packages)](https://wiki.tox.im/Binaries#Linux)
* [Linux 32 bit](https://jenkins.libtoxcore.so/job/toxic_linux_i386/lastSuccessfulBuild/artifact/toxic_linux_i386.tar.xz)
* [Linux 64 bit](https://jenkins.libtoxcore.so/job/toxic_linux_amd64/lastSuccessfulBuild/artifact/toxic_linux_amd64.tar.xz)

## Installation
[Compiling](/INSTALL.md)

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

