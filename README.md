<a href="https://scan.coverity.com/projects/toxic-tox">
  <img alt="Coverity Scan Build Status"
       src="https://scan.coverity.com/projects/4975/badge.svg"/>
</a>

Toxic is a [Tox](https://tox.chat)-based instant messaging and video chat client.

[![Toxic Screenshot](https://i.imgur.com/TwYA8L0.png "Toxic Home Screen")](https://i.imgur.com/TwYA8L0.png)

## Installation
[See the install instructions](/INSTALL.md)

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

