# Installation
* [Dependencies](#deps)
  * [OS X Notes](#deps_osx)
* [Compiling](#compiling)
  * [Documentation](#docs)
* [Translating](#langs)
  * [Create new translation 1: PO file](#new_lang_1)
  * [Create new translation 2: MO file](#new_lang_2)
  * [Update existing translation](#upd_lang)
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
| [GNUmake](https://www.gnu.org/software/make)         | BASE                       | make             |
| [Tox Core AV](https://github.com/irungentoo/toxcore) | AUDIO                      | *None*           |
| [OpenAL](http://openal.org)                          | AUDIO, SOUND NOTIFICATIONS | libopenal-dev    |
| [OpenALUT](http://openal.org)                        | SOUND NOTIFICATIONS        | libalut-dev      |
| [LibNotify](https://developer.gnome.org/libnotify)   | DESKTOP NOTIFICATIONS      | libnotify-dev    |
| [AsciiDoc](http://asciidoc.org/index.html)           | DOCUMENTATION<sup>1</sup>  | asciidoc         |
| [Gettext](https://www.gnu.org/software/gettext)      | LOCALIZATION<sup>2</sup>   | gettext          |
<sup>1</sup>: see [Documentation](#docs)<br />
<sup>2</sup>: see [Translating](#langs)

<a name="deps_osx" />
#### OS X Notes
Using [Homebrew](http://brew.sh):
```
brew install openal-soft freealut libconfig gettext
brew install https://raw.githubusercontent.com/Tox/homebrew-tox/master/Formula/libtoxcore.rb
brew install https://raw.githubusercontent.com/Homebrew/homebrew-x11/master/libnotify.rb
brew link gettext
```

You can omit `libnotify` if you intend to build without desktop notifications enabled. 

<a name="compiling">
## Compiling
```
make PREFIX="/where/to/install"
sudo make install PREFIX="/where/to/install"
```

<a name="docs" />
#### Documentation
Run `make doc` in the build directory after editing the asciidoc files to regenerate the manpages.<br />
**NOTE FOR DEVELOPERS**: asciidoc files and generated manpages will need to be commited together.<br />
**NOTE FOR EVERYONE**: [asciidoc](http://asciidoc.org/index.html) (and this step) is only required for regenerating manpages when you modify them.

<a name="langs" />
## Translating
Toxic uses gettext to localize some strings in various languages.<br />
These notes are for people who want help translating toxic in new languages (or improve an existing translation).<br />
The following example shows how to create/update german translation (de).

<a name="new_lang_1" />
#### Create new translation 1: PO file
To start a new translation, you can use the [provided script](translations/tools/create_po.sh):
```
cd toxic-src/translations/tools
./create_po.sh
Insert locale to create (for example "en"): de
Created de.po.
```
Now you can proceed to translate `toxic-src/translations/de.po`.

<a name="new_lang_2" />
#### Create new translation 2: MO file
When you fully translated the PO file, you are ready to create the MO (Machine Object) file.<br />
Again you can use the [provided script](translations/tools/create_mo.sh) to achieve this:
```
cd toxic-src/translations/tools
./create_mo.sh
Insert locale (for example "en"): de
```

<a name="upd_lang" />
#### Update existing translation
When the toxic sources are updated, you probably need to update your translation as well.<br />
To do so use the [provided script](translations/tools/update_po.sh) to update the PO file:
```
cd toxic-src/translations/tools
./update_po.sh
Insert locale to update (for example "en"): de
..................................... done.
```
Then you need to translate new/changed strings and after you fully updated the PO file, create the MO file as described [above](#new_lang_2).

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
  * `DISABLE_LOCALIZATION=1` → build toxic without localization support

<a name="packaging" />
#### Packaging
* For packaging purpose, you can use `DESTDIR=""` to specify a directory where to store installed files
* `DESTDIR=""` can be used in addition to `PREFIX=""`:
  * `DESTDIR=""` is meant to specify a directory where to store installed files (ex: "/tmp/build/pkg")
  * `PREFIX=""` is meant to specify a prefix directory for binaries and data files (ex: "/usr/local")

