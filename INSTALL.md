# Installation
* [Dependencies](#dependencies)
  * [OS X Notes](#os-x-notes)
* [Compiling](#compiling)
  * [Documentation](#documentation)
* [Notes](#notes)
  * [Compilation variables](#compilation-variables)
  * [Environment variables](#environment-variables)

## Dependencies
| Name                                                 | Needed by                  | Debian package      |
|------------------------------------------------------|----------------------------|---------------------|
| [Tox Core](https://github.com/toktok/c-toxcore)      | BASE                       | *None*              |
| [NCurses](https://www.gnu.org/software/ncurses)      | BASE                       | libncursesw5-dev    |
| [LibConfig](http://www.hyperrealm.com/libconfig)     | BASE                       | libconfig-dev       |
| [Linux Headers](https://www.kernel.org)              | BASE                       | linux-headers-*     |
| [GNUmake](https://www.gnu.org/software/make)         | BASE                       | make                |
| [libcurl](http://curl.haxx.se/)                      | BASE                       | libcurl4-openssl-dev|
| [libqrencode](https://fukuchi.org/works/qrencode/)   | QRCODE                     | libqrencode-dev     |
| [OpenAL](http://openal.org)                          | AUDIO, SOUND NOTIFICATIONS | libopenal-dev       |
| [OpenALUT](http://openal.org)                        | SOUND NOTIFICATIONS        | libalut-dev         |
| [LibNotify](https://developer.gnome.org/libnotify)   | DESKTOP NOTIFICATIONS      | libnotify-dev       |
| [Python 3](http://www.python.org/)                   | PYTHON                     | python3-dev         |
| [AsciiDoc](http://asciidoc.org/index.html)           | DOCUMENTATION<sup>1</sup>  | asciidoc            |

<sup>1</sup>: see [Documentation](#documentation)

#### OS X Notes
Using [Homebrew](http://brew.sh):
```
brew install curl qrencode openal-soft freealut libconfig libpng
brew install --HEAD https://raw.githubusercontent.com/Tox/homebrew-tox/master/Formula/libtoxcore.rb
brew install libnotify
export PKG_CONFIG_PATH=/usr/local/opt/openal-soft/lib/pkgconfig
make
```

You can omit `libnotify` if you intend to build without desktop notifications enabled.

## Compiling
```
make
sudo make install
```

#### Documentation
Run `make doc` in the build directory after editing the asciidoc files to regenerate the manpages.<br />
**Note for developers**: asciidoc files and generated manpages will need to be committed together.<br />
**Note for everyone**: [asciidoc](http://asciidoc.org/index.html) (and this step) is only required for regenerating manpages when you modify them.

## Notes

#### Compilation variables
* You can add specific flags to the Makefile with `USER_CFLAGS=""` and `USER_LDFLAGS=""` passed as arguments to make, or as environment variables
* Default compile options can be overridden by using special variables:
  * `DISABLE_X11=1` → Disable X11 support (needed for focus tracking)
  * `DISABLE_AV=1` → Disable audio call support
  * `DISABLE_SOUND_NOTIFY=1` → Disable sound notifications support
  * `DISABLE_QRCODE` → Disable QR exporting support
  * `DISABLE_QRPNG` → Disable support for exporting QR as PNG
  * `DISABLE_DESKTOP_NOTIFY=1` → Disable desktop notifications support
  * `DISABLE_GAMES=1` → Disable support for games
  * `ENABLE_PYTHON=1` → Build toxic with Python scripting support
  * `ENABLE_RELEASE=1` → Build toxic without debug symbols and with full compiler optimizations
  * `ENABLE_ASAN=1` → Build toxic with LLVM Address Sanitizer enabled

* `DESTDIR=""` Specifies the base install directory for binaries and data files (e.g.: DESTDIR="/tmp/build/pkg")

#### Environment variables
* You can use the `CFLAGS` and `LDFLAGS` environment variables to add specific flags to the Makefile
* The `PREFIX` environment variable specifies a base install directory for binaries and data files. This is interchangeable with the `DESTDIR` variable, and is generally used by systems that have the `PREFIX` environment variable set by default.<br />
**Note**: `sudo` does not preserve user environment variables by default on some systems. See the `sudoers` manual for more information.
