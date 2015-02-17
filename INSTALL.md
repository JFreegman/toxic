# Installation
* [Dependencies](#deps)
  * [OS X Notes](#deps_osx)
* [Compiling](#compiling)
  * [Documentation](#docs)
* [Notes](#notes)
  * [Compilation variables](#comp_vars)
  * [Packaging](#packaging)

<a name="deps" />
## Dependencies
| Name                                                 | Needed by                  | Debian package   |
|------------------------------------------------------|----------------------------|------------------|
| [Tox Core](https://github.com/irungentoo/toxcore)    | BASE                       | *None*           |
| [NCurses](https://www.gnu.org/software/ncurses)      | BASE                       | libncursesw5-dev |
| [LibConfig](http://www.hyperrealm.com/libconfig)     | BASE                       | libconfig-dev    |
| [Tox Core AV](https://github.com/irungentoo/toxcore) | AUDIO                      | *None*           |
| [OpenAL](http://openal.org)                          | AUDIO, SOUND NOTIFICATIONS | libopenal-dev    |
| [OpenALUT](http://openal.org)                        | SOUND NOTIFICATIONS        | libalut-dev      |
| [LibNotify](https://developer.gnome.org/libnotify)   | DESKTOP NOTIFICATIONS      | libnotify-dev    |
| [AsciiDoc](http://asciidoc.org/index.html)           | DOCUMENTATION<sup>1</sup>  | asciidoc         |
<sup>1</sup>: see [Documentation](#docs)

<a name="deps_osx" />
#### OS X Notes
Using [Homebrew](http://brew.sh):
```
brew install openal-soft freealut libconfig
brew install https://raw.githubusercontent.com/Tox/homebrew-tox/master/Formula/libtoxcore.rb
brew install https://raw.githubusercontent.com/Homebrew/homebrew-x11/master/libnotify.rb
```

You can omit `libnotify` if you intend to build without desktop notifications enabled. 

<a name="Compiling">
## Compiling
```
cd build/
make PREFIX="/where/to/install"
sudo make install PREFIX="/where/to/install"
```

<a name="docs" />
#### Documentation
Run `make doc` in the build directory after editing the asciidoc files to regenerate the manpages.<br />
**NOTE FOR DEVELOPERS**: asciidoc files and generated manpages will need to be commited together.<br />
**NOTE FOR EVERYONE**: [asciidoc](http://asciidoc.org/index.html) (and this step) is only required for regenerating manpages when you modify them.

<a name="notes" />
## Notes

<a name="comp_vars" />
#### Compilation variables
* You can add specific flags to the Makefile with `USER_CFLAGS=""` and/or `USER_LDFLAGS=""`
* You can pass your own flags to the Makefile with `CFLAGS=""` and/or `LDFLAGS=""` (this will supersede the default ones)
* Additional features are automatically enabled if all dependencies are found, but you can disable them by using special variables:
  * `DISABLE_X11=1` → build toxic without X11 support (needed for focus tracking)
  * `DISABLE_AV=1` → build toxic without audio call support
  * `DISABLE_SOUND_NOTIFY=1` → build toxic without sound notifications support
  * `DISABLE_DESKTOP_NOTIFY=1` → build toxic without desktop notifications support

<a name="packaging" />
#### Packaging
* For packaging purpose, you can use `DESTDIR=""` to specify a directory where to store installed files
* `DESTDIR=""` can be used in addition to `PREFIX=""`:
  * `DESTDIR=""` is meant to specify a directory where to store installed files (ex: "/tmp/build/pkg")
  * `PREFIX=""` is meant to specify a prefix directory for binaries and data files (ex: "/usr/local")

