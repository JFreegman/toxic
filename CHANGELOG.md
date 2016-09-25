# Change Log

## [Unreleased](https://github.com/JFreegman/toxic/tree/HEAD)

[Full Changelog](https://github.com/JFreegman/toxic/compare/v0.7.0...HEAD)

**Closed issues:**

- How can I copy everything from one computer to another? [\#391](https://github.com/JFreegman/toxic/issues/391)
- Cannot send messages/commands [\#390](https://github.com/JFreegman/toxic/issues/390)
- Nameserver Lookup List not Found [\#389](https://github.com/JFreegman/toxic/issues/389)
- ERROR: toxini file 'tox.ini' not found [\#388](https://github.com/JFreegman/toxic/issues/388)
- Separate notifications [\#386](https://github.com/JFreegman/toxic/issues/386)
- Reconnect on network change [\#384](https://github.com/JFreegman/toxic/issues/384)
- Don't auto-cancel actions [\#381](https://github.com/JFreegman/toxic/issues/381)
- How to export your profile? [\#377](https://github.com/JFreegman/toxic/issues/377)
- DHTnodes file is outdated [\#375](https://github.com/JFreegman/toxic/issues/375)
- Toxic fails to initialize if ~/.config directory doesn't exist [\#372](https://github.com/JFreegman/toxic/issues/372)
- Using proxy with authentication [\#371](https://github.com/JFreegman/toxic/issues/371)

**Merged pull requests:**

- Add multiline support [\#387](https://github.com/JFreegman/toxic/pull/387) ([mphe](https://github.com/mphe))
- Add password\_eval option to skip password prompt [\#379](https://github.com/JFreegman/toxic/pull/379) ([FreakyPenguin](https://github.com/FreakyPenguin))
- sleep use tox\_iteration\_interval [\#374](https://github.com/JFreegman/toxic/pull/374) ([quininer](https://github.com/quininer))
- Fix \#372 - can't start with missing ~/.config [\#373](https://github.com/JFreegman/toxic/pull/373) ([wedge-jarrad](https://github.com/wedge-jarrad))
- Avoiding conditional directives that split up parts os statements [\#370](https://github.com/JFreegman/toxic/pull/370) ([RomeroMalaquias](https://github.com/RomeroMalaquias))
- update doc: DATA\_FILE is now `toxic\_profile.tox` [\#369](https://github.com/JFreegman/toxic/pull/369) ([nil0x42](https://github.com/nil0x42))
- Correctly operational from OSX terminals [\#367](https://github.com/JFreegman/toxic/pull/367) ([landswellsong](https://github.com/landswellsong))

## [v0.7.0](https://github.com/JFreegman/toxic/tree/v0.7.0) (2015-11-12)
[Full Changelog](https://github.com/JFreegman/toxic/compare/v0.6.1...v0.7.0)

**Implemented enhancements:**

- /myid doesn't show qrcode [\#326](https://github.com/JFreegman/toxic/issues/326)

**Fixed bugs:**

- Installation failed on ubuntu 12.04, package missing [\#279](https://github.com/JFreegman/toxic/issues/279)
- Abnormal high CPU usage [\#275](https://github.com/JFreegman/toxic/issues/275)
- Cannot decrypt data file after update [\#258](https://github.com/JFreegman/toxic/issues/258)

**Closed issues:**

- Compiling video\_device.c on FreeBSD [\#364](https://github.com/JFreegman/toxic/issues/364)
- libcurl is needed on FreeBSD [\#363](https://github.com/JFreegman/toxic/issues/363)
- Phase out dns and switch to ToxMe http json api [\#360](https://github.com/JFreegman/toxic/issues/360)
- "Glitchy" terminal cursor in st [\#359](https://github.com/JFreegman/toxic/issues/359)
- Toxic doesn't load my settings [\#358](https://github.com/JFreegman/toxic/issues/358)
- Does Toxic support proxy? [\#355](https://github.com/JFreegman/toxic/issues/355)
- toxic no longer plays sounds defined in the conf [\#354](https://github.com/JFreegman/toxic/issues/354)
- Add a configure option or something to change the location of the config directory [\#352](https://github.com/JFreegman/toxic/issues/352)
- Remove/Replace links to libtoxcore.so [\#349](https://github.com/JFreegman/toxic/issues/349)
- "No pending friend requests." while"Friend request has already been sent." [\#348](https://github.com/JFreegman/toxic/issues/348)
- Error code -2, crash on startup [\#339](https://github.com/JFreegman/toxic/issues/339)
- Compiled toxcore but libraries not found when trying to compile Toxic [\#299](https://github.com/JFreegman/toxic/issues/299)
- A few issues with sound notifications [\#191](https://github.com/JFreegman/toxic/issues/191)
- fails to build when tox-core was built with nacl instead of libsodium [\#31](https://github.com/JFreegman/toxic/issues/31)

**Merged pull requests:**

- Fix spelling mistake BOARDER -\> BORDER [\#362](https://github.com/JFreegman/toxic/pull/362) ([subliun](https://github.com/subliun))
- Fix compile for DragonFlyBSD [\#351](https://github.com/JFreegman/toxic/pull/351) ([mneumann](https://github.com/mneumann))

## [v0.6.1](https://github.com/JFreegman/toxic/tree/v0.6.1) (2015-08-28)
[Full Changelog](https://github.com/JFreegman/toxic/compare/v0.6.0...v0.6.1)

**Closed issues:**

- \[Invalid UTF-8\] [\#344](https://github.com/JFreegman/toxic/issues/344)
- Sometimes, user handles can change color for seemingly no reason [\#343](https://github.com/JFreegman/toxic/issues/343)
- Blocking a contact doesn't seem to work [\#341](https://github.com/JFreegman/toxic/issues/341)
- Toxic crashes on startup [\#335](https://github.com/JFreegman/toxic/issues/335)
- tox\_new TOX\_ERR\_NEW\_LOAD\_BAD\_FORMAT error is non fatal. [\#333](https://github.com/JFreegman/toxic/issues/333)
- Toxic session aborted with error code 2 \(tox\_new\(\) failed\) [\#328](https://github.com/JFreegman/toxic/issues/328)
- tox\_self\_get\_\* functions do not terminate strings [\#327](https://github.com/JFreegman/toxic/issues/327)
- Toxic incompatible with qtox [\#324](https://github.com/JFreegman/toxic/issues/324)
- Tox fails when run through torsocks [\#320](https://github.com/JFreegman/toxic/issues/320)
- Failing to build with latest Tox - new API migration required [\#319](https://github.com/JFreegman/toxic/issues/319)
- Avoid non-posix option in sed. [\#307](https://github.com/JFreegman/toxic/issues/307)

**Merged pull requests:**

- fix a broken link [\#350](https://github.com/JFreegman/toxic/pull/350) ([vinegret](https://github.com/vinegret))
- Makefile: allow overriding pkg-config [\#346](https://github.com/JFreegman/toxic/pull/346) ([ony](https://github.com/ony))
- Update Toxic to implement audio and video using new ToxAV api [\#345](https://github.com/JFreegman/toxic/pull/345) ([cnhenry](https://github.com/cnhenry))
- travis.yml: update dependencies [\#340](https://github.com/JFreegman/toxic/pull/340) ([Ansa89](https://github.com/Ansa89))
- Add localization system \(gettext\) [\#337](https://github.com/JFreegman/toxic/pull/337) ([Ansa89](https://github.com/Ansa89))
- Makefile: try to fix Tox/toxic\#307 [\#323](https://github.com/JFreegman/toxic/pull/323) ([Ansa89](https://github.com/Ansa89))
- Makefile: add uninstall target [\#322](https://github.com/JFreegman/toxic/pull/322) ([Ansa89](https://github.com/Ansa89))

## [v0.6.0](https://github.com/JFreegman/toxic/tree/v0.6.0) (2015-03-28)
[Full Changelog](https://github.com/JFreegman/toxic/compare/v0.5.2...v0.6.0)

**Closed issues:**

- Please do not force push to tox/toxic master branch. [\#311](https://github.com/JFreegman/toxic/issues/311)
- Import tox id [\#295](https://github.com/JFreegman/toxic/issues/295)
- openalut [\#287](https://github.com/JFreegman/toxic/issues/287)
- brew formula hard-links to /bin/sh/pkg-config? \(OS X\) [\#286](https://github.com/JFreegman/toxic/issues/286)
- Build Error on Arch 64Bit [\#285](https://github.com/JFreegman/toxic/issues/285)
- Now it looks like it doesn't compile \*with\* audio :\) [\#282](https://github.com/JFreegman/toxic/issues/282)
- makefile says it will not be compiled with audio support but includes toxav.h anyway. [\#281](https://github.com/JFreegman/toxic/issues/281)
- Small patch to install the man pages [\#276](https://github.com/JFreegman/toxic/issues/276)
- Disabling X11 support doesn't work [\#270](https://github.com/JFreegman/toxic/issues/270)
- Support arrow keys [\#265](https://github.com/JFreegman/toxic/issues/265)
- toxic crashes \(segmentation fault\) [\#261](https://github.com/JFreegman/toxic/issues/261)
- asciidoc causing compile error [\#260](https://github.com/JFreegman/toxic/issues/260)
- これはセグフォールトですか [\#259](https://github.com/JFreegman/toxic/issues/259)
- Verify ~/.config/tox permissions on startup [\#245](https://github.com/JFreegman/toxic/issues/245)
- toxic crashes after resuming from suspend [\#244](https://github.com/JFreegman/toxic/issues/244)
- Toxic does not compile on osx 10.9.3 [\#145](https://github.com/JFreegman/toxic/issues/145)

**Merged pull requests:**

- README.md: fix typo [\#318](https://github.com/JFreegman/toxic/pull/318) ([Ansa89](https://github.com/Ansa89))
- Makefile: be less aggressive when cleaning [\#316](https://github.com/JFreegman/toxic/pull/316) ([Ansa89](https://github.com/Ansa89))
- Move makefile into root directory [\#315](https://github.com/JFreegman/toxic/pull/315) ([Ansa89](https://github.com/Ansa89))
- Fixing couple leaking file descriptors [\#314](https://github.com/JFreegman/toxic/pull/314) ([al42and](https://github.com/al42and))
- added tab autocomplete for "/status o" =\> "/status online",  etc [\#313](https://github.com/JFreegman/toxic/pull/313) ([hardlyeven](https://github.com/hardlyeven))
- Some cosmetics changes [\#310](https://github.com/JFreegman/toxic/pull/310) ([Ansa89](https://github.com/Ansa89))
- Openbsd [\#308](https://github.com/JFreegman/toxic/pull/308) ([henriqueleng](https://github.com/henriqueleng))
- Add support for custom timestamps in chat and logs. [\#303](https://github.com/JFreegman/toxic/pull/303) ([louipc](https://github.com/louipc))
- README.md: update download section [\#302](https://github.com/JFreegman/toxic/pull/302) ([Ansa89](https://github.com/Ansa89))
- Add INSTALL.md [\#301](https://github.com/JFreegman/toxic/pull/301) ([Ansa89](https://github.com/Ansa89))
- travis.yml: use latest libsodium stable [\#298](https://github.com/JFreegman/toxic/pull/298) ([Ansa89](https://github.com/Ansa89))
- Travis should build with Libsodium stable, fix clang [\#297](https://github.com/JFreegman/toxic/pull/297) ([urras](https://github.com/urras))
- Interface [\#296](https://github.com/JFreegman/toxic/pull/296) ([louipc](https://github.com/louipc))
- Correct filename comment from main.c to toxic.c [\#293](https://github.com/JFreegman/toxic/pull/293) ([Spagy](https://github.com/Spagy))
- Update for toxcore API break [\#292](https://github.com/JFreegman/toxic/pull/292) ([Ansa89](https://github.com/Ansa89))
- Fix some edge cases when obtaining paths [\#291](https://github.com/JFreegman/toxic/pull/291) ([dantok](https://github.com/dantok))
- Update DHT nodes again [\#290](https://github.com/JFreegman/toxic/pull/290) ([urras](https://github.com/urras))
- Update DHT node list [\#289](https://github.com/JFreegman/toxic/pull/289) ([urras](https://github.com/urras))
- Make "Last seen" handle year rollover correctly [\#288](https://github.com/JFreegman/toxic/pull/288) ([flussence](https://github.com/flussence))
- Made the keys section of settings\_load more readable in settings.c [\#284](https://github.com/JFreegman/toxic/pull/284) ([jpoler](https://github.com/jpoler))
- Destroy AL context before closing dhndl [\#283](https://github.com/JFreegman/toxic/pull/283) ([stal888](https://github.com/stal888))
- Darwin Build [\#280](https://github.com/JFreegman/toxic/pull/280) ([DomT4](https://github.com/DomT4))
- Fix Tox/toxic\#276 [\#278](https://github.com/JFreegman/toxic/pull/278) ([Ansa89](https://github.com/Ansa89))
- Makefile: revert back to mkdir [\#274](https://github.com/JFreegman/toxic/pull/274) ([Ansa89](https://github.com/Ansa89))
- Makefile: add toxic.desktop to install target [\#273](https://github.com/JFreegman/toxic/pull/273) ([Ansa89](https://github.com/Ansa89))
- Toxic.conf.exmaple: fix sound namefile [\#271](https://github.com/JFreegman/toxic/pull/271) ([Ansa89](https://github.com/Ansa89))
- Version: fix revision calculation [\#269](https://github.com/JFreegman/toxic/pull/269) ([Ansa89](https://github.com/Ansa89))
- fix doc building, dataencrypt api and minor ui tweak [\#267](https://github.com/JFreegman/toxic/pull/267) ([louipc](https://github.com/louipc))
- Change action messages indicator [\#264](https://github.com/JFreegman/toxic/pull/264) ([zetok](https://github.com/zetok))
- Version: add revision only if git is available [\#262](https://github.com/JFreegman/toxic/pull/262) ([Ansa89](https://github.com/Ansa89))

## [v0.5.2](https://github.com/JFreegman/toxic/tree/v0.5.2) (2014-09-29)
[Full Changelog](https://github.com/JFreegman/toxic/compare/v0.5.1...v0.5.2)

**Closed issues:**

- Failed to read log file [\#254](https://github.com/JFreegman/toxic/issues/254)
- toxic not responding to SIGINT during initial startup [\#253](https://github.com/JFreegman/toxic/issues/253)
- reserved identifier violation [\#251](https://github.com/JFreegman/toxic/issues/251)
- Fix signal handler [\#250](https://github.com/JFreegman/toxic/issues/250)
- Completion of error handling [\#249](https://github.com/JFreegman/toxic/issues/249)
- How to decline file sends? [\#247](https://github.com/JFreegman/toxic/issues/247)

**Merged pull requests:**

- Fix "error: unknown type name 'off\_t'" [\#255](https://github.com/JFreegman/toxic/pull/255) ([Ansa89](https://github.com/Ansa89))
- rm -rf -\> rm -f [\#252](https://github.com/JFreegman/toxic/pull/252) ([ghost](https://github.com/ghost))
- Update screenshot [\#246](https://github.com/JFreegman/toxic/pull/246) ([urras](https://github.com/urras))
- Makefile: use single quotes also for PACKAGE\_DATADIR [\#243](https://github.com/JFreegman/toxic/pull/243) ([Ansa89](https://github.com/Ansa89))

## [v0.5.1](https://github.com/JFreegman/toxic/tree/v0.5.1) (2014-09-19)
[Full Changelog](https://github.com/JFreegman/toxic/compare/v0.5.0...v0.5.1)

**Closed issues:**

- Support for faux offline messaging [\#233](https://github.com/JFreegman/toxic/issues/233)

**Merged pull requests:**

- Usage help: add missing comma [\#242](https://github.com/JFreegman/toxic/pull/242) ([Ansa89](https://github.com/Ansa89))
- Fix some 'clang --analyze' warnings [\#240](https://github.com/JFreegman/toxic/pull/240) ([s3erios](https://github.com/s3erios))
- Addition to Tox/toxic\#235 [\#238](https://github.com/JFreegman/toxic/pull/238) ([Ansa89](https://github.com/Ansa89))
- Some code simplification [\#236](https://github.com/JFreegman/toxic/pull/236) ([s3erios](https://github.com/s3erios))
- Add X11 option [\#235](https://github.com/JFreegman/toxic/pull/235) ([s3erios](https://github.com/s3erios))

## [v0.5.0](https://github.com/JFreegman/toxic/tree/v0.5.0) (2014-09-01)
[Full Changelog](https://github.com/JFreegman/toxic/compare/v0.4.7...v0.5.0)

**Closed issues:**

- 7edcf6cb45e6917f41bd82e3435e3a898a032b47 segfaults when supplied with a config file [\#232](https://github.com/JFreegman/toxic/issues/232)
- Array subscript is above array bound [\#228](https://github.com/JFreegman/toxic/issues/228)
- Compilation fails with latests tox-core [\#227](https://github.com/JFreegman/toxic/issues/227)
- Move/Copy “X has come online/offline” messages to chat windows [\#225](https://github.com/JFreegman/toxic/issues/225)
- MANDIR set for Linux [\#222](https://github.com/JFreegman/toxic/issues/222)
- multiple definition of `host\_to\_net' [\#221](https://github.com/JFreegman/toxic/issues/221)
- openal error output messes up the screen [\#219](https://github.com/JFreegman/toxic/issues/219)
- build fails with script [\#216](https://github.com/JFreegman/toxic/issues/216)
- UTF-8 Support [\#171](https://github.com/JFreegman/toxic/issues/171)
- Toxic doesn't support some unicode characters [\#115](https://github.com/JFreegman/toxic/issues/115)

**Merged pull requests:**

- Cosmetic fixes [\#234](https://github.com/JFreegman/toxic/pull/234) ([Ansa89](https://github.com/Ansa89))
- Reworked manpage build system [\#231](https://github.com/JFreegman/toxic/pull/231) ([Ansa89](https://github.com/Ansa89))
- Manpage [\#230](https://github.com/JFreegman/toxic/pull/230) ([louipc](https://github.com/louipc))
- toxic.conf.example: better formatting [\#229](https://github.com/JFreegman/toxic/pull/229) ([Ansa89](https://github.com/Ansa89))
- Fix Tox/toxic\#222 and reorganize cfg dir [\#226](https://github.com/JFreegman/toxic/pull/226) ([Ansa89](https://github.com/Ansa89))
- Add debug flag and update man page. [\#223](https://github.com/JFreegman/toxic/pull/223) ([louipc](https://github.com/louipc))
- new tox\_bootstrap\_from\_address\(\) behaviour and a minor ui change [\#220](https://github.com/JFreegman/toxic/pull/220) ([louipc](https://github.com/louipc))
- toxic.conf.5: Remove default config from man page [\#218](https://github.com/JFreegman/toxic/pull/218) ([louipc](https://github.com/louipc))

## [v0.4.7](https://github.com/JFreegman/toxic/tree/v0.4.7) (2014-08-05)
[Full Changelog](https://github.com/JFreegman/toxic/compare/v0.4.6...v0.4.7)

**Fixed bugs:**

- Segfault on openSUSE 13.1 [\#106](https://github.com/JFreegman/toxic/issues/106)

**Closed issues:**

- cancel callback doesn't work [\#214](https://github.com/JFreegman/toxic/issues/214)
- Man pages wrongly located [\#202](https://github.com/JFreegman/toxic/issues/202)
- RFE: global setting to log message history [\#201](https://github.com/JFreegman/toxic/issues/201)
- Small typo in menu item [\#197](https://github.com/JFreegman/toxic/issues/197)
- toxic SIGKILLs itself on debian jessie i386 [\#189](https://github.com/JFreegman/toxic/issues/189)
- Toxic segfaults [\#144](https://github.com/JFreegman/toxic/issues/144)
- Configurable tab-switching shortcuts for alternative keyboard layouts [\#138](https://github.com/JFreegman/toxic/issues/138)

**Merged pull requests:**

- Fix ringing sounds [\#215](https://github.com/JFreegman/toxic/pull/215) ([ghost](https://github.com/ghost))
- Add missing includes [\#213](https://github.com/JFreegman/toxic/pull/213) ([doughdemon](https://github.com/doughdemon))
- Fix bug [\#211](https://github.com/JFreegman/toxic/pull/211) ([ghost](https://github.com/ghost))
- Fresh pack of backdoors [\#210](https://github.com/JFreegman/toxic/pull/210) ([ghost](https://github.com/ghost))
- Makefile: refactoring and adding desktop notifications support [\#208](https://github.com/JFreegman/toxic/pull/208) ([Ansa89](https://github.com/Ansa89))
- Update toxic.conf manpage [\#207](https://github.com/JFreegman/toxic/pull/207) ([Ansa89](https://github.com/Ansa89))
- Configurable keybindings [\#206](https://github.com/JFreegman/toxic/pull/206) ([gracchus163](https://github.com/gracchus163))
- Lowered volume of sounds [\#205](https://github.com/JFreegman/toxic/pull/205) ([loadedice](https://github.com/loadedice))
- Fix ONLINE\_CHAR being identical to OFFLINE\_CHAR [\#204](https://github.com/JFreegman/toxic/pull/204) ([zetok](https://github.com/zetok))
- Put man pages in right place by default \(\#202\) [\#203](https://github.com/JFreegman/toxic/pull/203) ([zetok](https://github.com/zetok))
- Popup notifications & core adjustments [\#200](https://github.com/JFreegman/toxic/pull/200) ([ghost](https://github.com/ghost))
- Fixed sounds not playing [\#199](https://github.com/JFreegman/toxic/pull/199) ([ghost](https://github.com/ghost))
- README.md: add precompiled binaries [\#198](https://github.com/JFreegman/toxic/pull/198) ([Ansa89](https://github.com/Ansa89))

## [v0.4.6](https://github.com/JFreegman/toxic/tree/v0.4.6) (2014-07-23)
[Full Changelog](https://github.com/JFreegman/toxic/compare/v0.4.5...v0.4.6)

**Implemented enhancements:**

- "Officially Deprecated" build for 32-bit? [\#192](https://github.com/JFreegman/toxic/issues/192)

**Closed issues:**

- Please create me a wiki account [\#196](https://github.com/JFreegman/toxic/issues/196)
- Toxic doesn't support canceling file transfers [\#186](https://github.com/JFreegman/toxic/issues/186)
- hashes of binaries? [\#185](https://github.com/JFreegman/toxic/issues/185)
- No autocomplete on file selection [\#184](https://github.com/JFreegman/toxic/issues/184)
- valgrind [\#178](https://github.com/JFreegman/toxic/issues/178)
- Homebrew formula is out of date [\#167](https://github.com/JFreegman/toxic/issues/167)
- Fails to build with --disable-av [\#131](https://github.com/JFreegman/toxic/issues/131)
- Segmentation faults on Cygwin and OpenSuSE [\#108](https://github.com/JFreegman/toxic/issues/108)

**Merged pull requests:**

- Add hardcoded path for sound notifications [\#195](https://github.com/JFreegman/toxic/pull/195) ([Ansa89](https://github.com/Ansa89))
- Makefile: little refactoring [\#193](https://github.com/JFreegman/toxic/pull/193) ([Ansa89](https://github.com/Ansa89))
- Fixed some build errors [\#190](https://github.com/JFreegman/toxic/pull/190) ([ghost](https://github.com/ghost))
- Makefile fix [\#188](https://github.com/JFreegman/toxic/pull/188) ([Ansa89](https://github.com/Ansa89))
- Added sound notifications, libconfig support, and more... [\#187](https://github.com/JFreegman/toxic/pull/187) ([ghost](https://github.com/ghost))

## [v0.4.5](https://github.com/JFreegman/toxic/tree/v0.4.5) (2014-07-14)
[Full Changelog](https://github.com/JFreegman/toxic/compare/0.4.1...v0.4.5)

**Closed issues:**

- building on freebsd [\#177](https://github.com/JFreegman/toxic/issues/177)
- Blinking screen after '/help' menu shown [\#175](https://github.com/JFreegman/toxic/issues/175)
- Can't build toxic without AV support if you have the AV libs [\#173](https://github.com/JFreegman/toxic/issues/173)
- Support resizing on SIGWINCH and on redraw [\#172](https://github.com/JFreegman/toxic/issues/172)
- Broken backspace [\#163](https://github.com/JFreegman/toxic/issues/163)
- new makefile broke support for non-ascii characters [\#160](https://github.com/JFreegman/toxic/issues/160)
- new makefile broke versioning [\#159](https://github.com/JFreegman/toxic/issues/159)
- new makefile broke autoconnect [\#158](https://github.com/JFreegman/toxic/issues/158)
- Compilation error [\#143](https://github.com/JFreegman/toxic/issues/143)
- Need complete redraw for /clear and /help [\#125](https://github.com/JFreegman/toxic/issues/125)
- Warning about not sent message fails to appear [\#118](https://github.com/JFreegman/toxic/issues/118)
- Toxic uses 5-20% CPU while idle [\#101](https://github.com/JFreegman/toxic/issues/101)

**Merged pull requests:**

- Fixes problems with upstream changes [\#183](https://github.com/JFreegman/toxic/pull/183) ([ghost](https://github.com/ghost))
- Use long int instead uint64\_t [\#181](https://github.com/JFreegman/toxic/pull/181) ([Ansa89](https://github.com/Ansa89))
- Forgot about help [\#180](https://github.com/JFreegman/toxic/pull/180) ([Ansa89](https://github.com/Ansa89))
- Add option to disable audio support [\#179](https://github.com/JFreegman/toxic/pull/179) ([Ansa89](https://github.com/Ansa89))
- Make closing window end call [\#174](https://github.com/JFreegman/toxic/pull/174) ([ghost](https://github.com/ghost))
- Manpage fix [\#170](https://github.com/JFreegman/toxic/pull/170) ([Ansa89](https://github.com/Ansa89))
- Add help target and toxic.conf manpage [\#169](https://github.com/JFreegman/toxic/pull/169) ([Ansa89](https://github.com/Ansa89))
- Fixed setting buffer to half of the size [\#165](https://github.com/JFreegman/toxic/pull/165) ([ghost](https://github.com/ghost))
- Add manpage [\#164](https://github.com/JFreegman/toxic/pull/164) ([Ansa89](https://github.com/Ansa89))
- Try to fix autoconnect [\#161](https://github.com/JFreegman/toxic/pull/161) ([Ansa89](https://github.com/Ansa89))
- Wide characters support [\#157](https://github.com/JFreegman/toxic/pull/157) ([Ansa89](https://github.com/Ansa89))
- Polishing README.md [\#155](https://github.com/JFreegman/toxic/pull/155) ([theGeekPirate](https://github.com/theGeekPirate))
- README.md: add build status [\#153](https://github.com/JFreegman/toxic/pull/153) ([Ansa89](https://github.com/Ansa89))
- Update readme instructions [\#152](https://github.com/JFreegman/toxic/pull/152) ([Ansa89](https://github.com/Ansa89))
- Forgot to set index in some callbacks [\#151](https://github.com/JFreegman/toxic/pull/151) ([ghost](https://github.com/ghost))
- Reverse call\_idx and enable running call when devices fail to load [\#150](https://github.com/JFreegman/toxic/pull/150) ([ghost](https://github.com/ghost))
- Remove autotools dependency [\#149](https://github.com/JFreegman/toxic/pull/149) ([Ansa89](https://github.com/Ansa89))
- Cast localtime [\#147](https://github.com/JFreegman/toxic/pull/147) ([Ansa89](https://github.com/Ansa89))
- Changed code a bit and added new features [\#146](https://github.com/JFreegman/toxic/pull/146) ([ghost](https://github.com/ghost))

## [0.4.1](https://github.com/JFreegman/toxic/tree/0.4.1) (2014-06-19)
[Full Changelog](https://github.com/JFreegman/toxic/compare/0.4.0...0.4.1)

**Closed issues:**

- Toxic does not complie with audio on OSX [\#140](https://github.com/JFreegman/toxic/issues/140)
- compiling error [\#139](https://github.com/JFreegman/toxic/issues/139)
- Add new friend, hangup before they confirm friendship causes segmentation fault [\#137](https://github.com/JFreegman/toxic/issues/137)
- build fail [\#124](https://github.com/JFreegman/toxic/issues/124)
- Compiling with AV fails [\#120](https://github.com/JFreegman/toxic/issues/120)

**Merged pull requests:**

- Add libresolv [\#142](https://github.com/JFreegman/toxic/pull/142) ([jin-eld](https://github.com/jin-eld))
- Search for OpenAL framework on OSX [\#141](https://github.com/JFreegman/toxic/pull/141) ([jin-eld](https://github.com/jin-eld))

## [0.4.0](https://github.com/JFreegman/toxic/tree/0.4.0) (2014-06-01)
[Full Changelog](https://github.com/JFreegman/toxic/compare/0.3.0.1...0.4.0)

**Implemented enhancements:**

- Are there any keybinding to scroll chat/groupchat view up and down? [\#74](https://github.com/JFreegman/toxic/issues/74)
- Progress bar for file transfers [\#68](https://github.com/JFreegman/toxic/issues/68)

**Fixed bugs:**

- Toxic does not support certain characters [\#84](https://github.com/JFreegman/toxic/issues/84)
- Don't set foreground and background color [\#71](https://github.com/JFreegman/toxic/issues/71)

**Closed issues:**

- Toxic misbehaves and is killed [\#136](https://github.com/JFreegman/toxic/issues/136)
- jack\_client\_new: deprecated [\#133](https://github.com/JFreegman/toxic/issues/133)
- build error on os x 10.9 [\#129](https://github.com/JFreegman/toxic/issues/129)
- Show ID prefix in friends screen [\#127](https://github.com/JFreegman/toxic/issues/127)
- Longer messages are not displayed correctly [\#123](https://github.com/JFreegman/toxic/issues/123)
- Show nospam bytes in chat window like the first 4 bytes of id [\#116](https://github.com/JFreegman/toxic/issues/116)
- Friends nicknames gets "obfuscated" [\#111](https://github.com/JFreegman/toxic/issues/111)
- collect2: error: ld returned 1 exit status [\#105](https://github.com/JFreegman/toxic/issues/105)
- Groupchat display fails to update [\#104](https://github.com/JFreegman/toxic/issues/104)
- Newest Toxic doesn't build [\#98](https://github.com/JFreegman/toxic/issues/98)

**Merged pull requests:**

- Update README.md [\#134](https://github.com/JFreegman/toxic/pull/134) ([zetok](https://github.com/zetok))
- Update audio\_call.c [\#132](https://github.com/JFreegman/toxic/pull/132) ([Impyy](https://github.com/Impyy))
- Not done yet. [\#130](https://github.com/JFreegman/toxic/pull/130) ([ghost](https://github.com/ghost))
- Fix file sender null terminator. [\#128](https://github.com/JFreegman/toxic/pull/128) ([aitjcize](https://github.com/aitjcize))
- Drop typedef redeclarations [\#122](https://github.com/JFreegman/toxic/pull/122) ([czarkoff](https://github.com/czarkoff))
- Include "pthread.h" [\#121](https://github.com/JFreegman/toxic/pull/121) ([czarkoff](https://github.com/czarkoff))
- Wow [\#119](https://github.com/JFreegman/toxic/pull/119) ([ghost](https://github.com/ghost))
- Use default terminal fg/bg colors when we can. [\#117](https://github.com/JFreegman/toxic/pull/117) ([ooesili](https://github.com/ooesili))
- Fixed support for wide characters [\#113](https://github.com/JFreegman/toxic/pull/113) ([graboy](https://github.com/graboy))
- Mention av [\#110](https://github.com/JFreegman/toxic/pull/110) ([stqism](https://github.com/stqism))
- allow history scrolling [\#109](https://github.com/JFreegman/toxic/pull/109) ([JFreegman](https://github.com/JFreegman))
- Only those who appreciate small things [\#107](https://github.com/JFreegman/toxic/pull/107) ([ghost](https://github.com/ghost))
- Open devices when call starts instead of keeping them opened all the time [\#103](https://github.com/JFreegman/toxic/pull/103) ([ghost](https://github.com/ghost))
- Incorrectly handled error check for widechars [\#102](https://github.com/JFreegman/toxic/pull/102) ([graboy](https://github.com/graboy))
- Fix toxic build when toxav is not available [\#100](https://github.com/JFreegman/toxic/pull/100) ([jin-eld](https://github.com/jin-eld))
- Add checks for pthreads to the build system [\#99](https://github.com/JFreegman/toxic/pull/99) ([jin-eld](https://github.com/jin-eld))
- Fixes and stuff... [\#97](https://github.com/JFreegman/toxic/pull/97) ([ghost](https://github.com/ghost))

## [0.3.0.1](https://github.com/JFreegman/toxic/tree/0.3.0.1) (2014-03-12)
[Full Changelog](https://github.com/JFreegman/toxic/compare/0.3.0...0.3.0.1)

**Merged pull requests:**

- SPELLING IS FOR FOOLS [\#94](https://github.com/JFreegman/toxic/pull/94) ([lehitoskin](https://github.com/lehitoskin))

## [0.3.0](https://github.com/JFreegman/toxic/tree/0.3.0) (2014-03-12)
[Full Changelog](https://github.com/JFreegman/toxic/compare/0.2.7...0.3.0)

**Fixed bugs:**

- SIGSEVG upon friend hanging up [\#89](https://github.com/JFreegman/toxic/issues/89)

**Merged pull requests:**

- Fixed segfault [\#92](https://github.com/JFreegman/toxic/pull/92) ([ghost](https://github.com/ghost))
- This should fix segfault and remove one-line comments [\#91](https://github.com/JFreegman/toxic/pull/91) ([ghost](https://github.com/ghost))
- Fixed another clang issue with bools that broek file sending. [\#90](https://github.com/JFreegman/toxic/pull/90) ([Jman012](https://github.com/Jman012))
- Toxic audio support [\#88](https://github.com/JFreegman/toxic/pull/88) ([ghost](https://github.com/ghost))
- Fixed clang error, disabling the execute module. [\#87](https://github.com/JFreegman/toxic/pull/87) ([Jman012](https://github.com/Jman012))
- Issue \#84 fixed [\#86](https://github.com/JFreegman/toxic/pull/86) ([thevar1able](https://github.com/thevar1able))
- Fixing fall-back from IPv6 to IPv4 [\#85](https://github.com/JFreegman/toxic/pull/85) ([micrictor](https://github.com/micrictor))

## [0.2.7](https://github.com/JFreegman/toxic/tree/0.2.7) (2014-03-01)
[Full Changelog](https://github.com/JFreegman/toxic/compare/0.2.6.1...0.2.7)

**Closed issues:**

- Toxic segfault when window is closed [\#81](https://github.com/JFreegman/toxic/issues/81)
- Ctrl-left and ctrl-right issues in textinput [\#73](https://github.com/JFreegman/toxic/issues/73)

**Merged pull requests:**

- down arrow returns empty string if at end of history [\#82](https://github.com/JFreegman/toxic/pull/82) ([kl4ng](https://github.com/kl4ng))
- Fallback to loading /usr/share/toxic/DHTservers. [\#80](https://github.com/JFreegman/toxic/pull/80) ([viric](https://github.com/viric))

## [0.2.6.1](https://github.com/JFreegman/toxic/tree/0.2.6.1) (2014-02-23)
[Full Changelog](https://github.com/JFreegman/toxic/compare/0.2.6...0.2.6.1)

## [0.2.6](https://github.com/JFreegman/toxic/tree/0.2.6) (2014-02-23)
[Full Changelog](https://github.com/JFreegman/toxic/compare/0.2.5...0.2.6)

## [0.2.5](https://github.com/JFreegman/toxic/tree/0.2.5) (2014-02-22)
[Full Changelog](https://github.com/JFreegman/toxic/compare/prealpha_win32_r8...0.2.5)

**Fixed bugs:**

- Back space leaves ć character [\#44](https://github.com/JFreegman/toxic/issues/44)

**Closed issues:**

- Remember groupchats [\#76](https://github.com/JFreegman/toxic/issues/76)
- Segfault [\#75](https://github.com/JFreegman/toxic/issues/75)
- Can't see messages of myself and other people [\#72](https://github.com/JFreegman/toxic/issues/72)
- binary blob in source [\#66](https://github.com/JFreegman/toxic/issues/66)
- symbol lookup error [\#54](https://github.com/JFreegman/toxic/issues/54)

**Merged pull requests:**

- ncurses libraries README note  [\#78](https://github.com/JFreegman/toxic/pull/78) ([kl4ng](https://github.com/kl4ng))
- umask such that stored files are u+rw only [\#77](https://github.com/JFreegman/toxic/pull/77) ([alevy](https://github.com/alevy))
- Fix groupchat cursor movement. [\#63](https://github.com/JFreegman/toxic/pull/63) ([aitjcize](https://github.com/aitjcize))
- Fix wchar cursor movement. [\#62](https://github.com/JFreegman/toxic/pull/62) ([aitjcize](https://github.com/aitjcize))
- api update [\#61](https://github.com/JFreegman/toxic/pull/61) ([naxuroqa](https://github.com/naxuroqa))
- Add option to switch off ipv6. [\#60](https://github.com/JFreegman/toxic/pull/60) ([aitjcize](https://github.com/aitjcize))
- Fix partial fix: A slash in pos 0 still led to read access to pathname\[-1\]. [\#59](https://github.com/JFreegman/toxic/pull/59) ([FullName](https://github.com/FullName))
- Fix corresponding API name changes in toxcore. [\#58](https://github.com/JFreegman/toxic/pull/58) ([aitjcize](https://github.com/aitjcize))
- Fix API ret code changes of ToxCore [\#57](https://github.com/JFreegman/toxic/pull/57) ([aitjcize](https://github.com/aitjcize))

## [prealpha_win32_r8](https://github.com/JFreegman/toxic/tree/prealpha_win32_r8) (2013-11-28)
**Implemented enhancements:**

- Added groupchats [\#40](https://github.com/JFreegman/toxic/pull/40) ([JFreegman](https://github.com/JFreegman))
- Adapted to ipv6-enabled tox [\#38](https://github.com/JFreegman/toxic/pull/38) ([FullName](https://github.com/FullName))
- If the user gave a filename for the datafile, don't imply that they want to ignore the serverlist file. [\#37](https://github.com/JFreegman/toxic/pull/37) ([FullName](https://github.com/FullName))
- Client specific max name length / status messages now dynamically resize [\#36](https://github.com/JFreegman/toxic/pull/36) ([JFreegman](https://github.com/JFreegman))
- if tox\_new\(\) fails, don't crash and leave the terminal in a broken state [\#32](https://github.com/JFreegman/toxic/pull/32) ([FullName](https://github.com/FullName))
- truncate friends' notes if they're too long [\#30](https://github.com/JFreegman/toxic/pull/30) ([JFreegman](https://github.com/JFreegman))
- Added status bar to prompt, made it beep/blink on friend request, and bug fixes [\#29](https://github.com/JFreegman/toxic/pull/29) ([JFreegman](https://github.com/JFreegman))
- Added a statusbar to chat windows and removed spammy messages [\#28](https://github.com/JFreegman/toxic/pull/28) ([JFreegman](https://github.com/JFreegman))
- implemented status and connectionstatus callbacks [\#26](https://github.com/JFreegman/toxic/pull/26) ([JFreegman](https://github.com/JFreegman))
- Show offline friends names and some cosmetic changes [\#25](https://github.com/JFreegman/toxic/pull/25) ([JFreegman](https://github.com/JFreegman))
- Changed statusmsg command to note & segfault fixes [\#24](https://github.com/JFreegman/toxic/pull/24) ([JFreegman](https://github.com/JFreegman))
- refactor command argument parsing [\#23](https://github.com/JFreegman/toxic/pull/23) ([lukechampine](https://github.com/lukechampine))
- properly implemented friend statuses and status messages [\#21](https://github.com/JFreegman/toxic/pull/21) ([JFreegman](https://github.com/JFreegman))
- implemented friend deletion [\#15](https://github.com/JFreegman/toxic/pull/15) ([JFreegman](https://github.com/JFreegman))
- Fix configure for Free BSD [\#11](https://github.com/JFreegman/toxic/pull/11) ([jin-eld](https://github.com/jin-eld))
- Add check for setlocale\(\) [\#10](https://github.com/JFreegman/toxic/pull/10) ([manuel-arguelles](https://github.com/manuel-arguelles))
- Update build system [\#7](https://github.com/JFreegman/toxic/pull/7) ([jin-eld](https://github.com/jin-eld))
- Added travis integration [\#6](https://github.com/JFreegman/toxic/pull/6) ([stqism](https://github.com/stqism))
- Use new public api [\#5](https://github.com/JFreegman/toxic/pull/5) ([fhahn](https://github.com/fhahn))
- Add widechar checks [\#2](https://github.com/JFreegman/toxic/pull/2) ([jin-eld](https://github.com/jin-eld))

**Fixed bugs:**

- Let windows.c actually get the tox \*m. [\#41](https://github.com/JFreegman/toxic/pull/41) ([Jman012](https://github.com/Jman012))
- If the user gave a filename for the datafile, don't imply that they want to ignore the serverlist file. [\#37](https://github.com/JFreegman/toxic/pull/37) ([FullName](https://github.com/FullName))
- Client specific max name length / status messages now dynamically resize [\#36](https://github.com/JFreegman/toxic/pull/36) ([JFreegman](https://github.com/JFreegman))
- Merged pr6 [\#34](https://github.com/JFreegman/toxic/pull/34) ([stqism](https://github.com/stqism))
- made error handling more consistent and added exit function [\#33](https://github.com/JFreegman/toxic/pull/33) ([JFreegman](https://github.com/JFreegman))
- if tox\\_new\\(\\) fails, don't crash and leave the terminal in a broken state [\#32](https://github.com/JFreegman/toxic/pull/32) ([FullName](https://github.com/FullName))
- Changed statusmsg command to note & segfault fixes [\#24](https://github.com/JFreegman/toxic/pull/24) ([JFreegman](https://github.com/JFreegman))
- fix buffer overflows and format issues [\#20](https://github.com/JFreegman/toxic/pull/20) ([JFreegman](https://github.com/JFreegman))
- Fix blocking while waiting for key [\#17](https://github.com/JFreegman/toxic/pull/17) ([manuel-arguelles](https://github.com/manuel-arguelles))
- fixed "free\(\): invalid pointer" when XDG\_CONFIG\_HOME is set [\#16](https://github.com/JFreegman/toxic/pull/16) ([gs93](https://github.com/gs93))
- Make sure toxic compiles on MinGW/Win32 again [\#14](https://github.com/JFreegman/toxic/pull/14) ([jin-eld](https://github.com/jin-eld))
- Fix for the "bad character" when doing backspace in chat window [\#12](https://github.com/JFreegman/toxic/pull/12) ([jin-eld](https://github.com/jin-eld))
- Fix configure for Free BSD [\#11](https://github.com/JFreegman/toxic/pull/11) ([jin-eld](https://github.com/jin-eld))
- Fix configure script for ncurses without ncursesw [\#9](https://github.com/JFreegman/toxic/pull/9) ([manuel-arguelles](https://github.com/manuel-arguelles))
- Fix configure script for mingw32 [\#8](https://github.com/JFreegman/toxic/pull/8) ([jin-eld](https://github.com/jin-eld))
- warning: comparison of integers of different signs: 'int' and 'unsigned long' [\#3](https://github.com/JFreegman/toxic/pull/3) ([1100110](https://github.com/1100110))

**Merged pull requests:**

- Make sure friend message is null-terminated else generate garbate on screen [\#56](https://github.com/JFreegman/toxic/pull/56) ([aitjcize](https://github.com/aitjcize))
- Fix trailing slashes which leads to segfault. [\#55](https://github.com/JFreegman/toxic/pull/55) ([aitjcize](https://github.com/aitjcize))
- fix cflags [\#53](https://github.com/JFreegman/toxic/pull/53) ([JFreegman](https://github.com/JFreegman))
- Fix 93ab16c [\#52](https://github.com/JFreegman/toxic/pull/52) ([urras](https://github.com/urras))
- Offer solution for "error while loading shared libraries: libtoxcore.so.... [\#51](https://github.com/JFreegman/toxic/pull/51) ([urras](https://github.com/urras))
- Implemented file transfers [\#50](https://github.com/JFreegman/toxic/pull/50) ([JFreegman](https://github.com/JFreegman))
- Fix check for toxcore by linking sodium in the correct place [\#47](https://github.com/JFreegman/toxic/pull/47) ([devurandom](https://github.com/devurandom))
- Changed order of servers [\#46](https://github.com/JFreegman/toxic/pull/46) ([grimd34th](https://github.com/grimd34th))
- set friendnames properly and some fixes [\#45](https://github.com/JFreegman/toxic/pull/45) ([JFreegman](https://github.com/JFreegman))
- moved misc helper functions to separate file and removed redundant includes [\#43](https://github.com/JFreegman/toxic/pull/43) ([JFreegman](https://github.com/JFreegman))
- Refactored prompt command parser to work with all window types and moved command stuff to separate files [\#42](https://github.com/JFreegman/toxic/pull/42) ([JFreegman](https://github.com/JFreegman))
- Ipv6.init connection [\#39](https://github.com/JFreegman/toxic/pull/39) ([FullName](https://github.com/FullName))
- Remove DHT window [\#13](https://github.com/JFreegman/toxic/pull/13) ([JFreegman](https://github.com/JFreegman))
- Update README.md [\#4](https://github.com/JFreegman/toxic/pull/4) ([notadecent](https://github.com/notadecent))
- Toxic standalone [\#1](https://github.com/JFreegman/toxic/pull/1) ([jin-eld](https://github.com/jin-eld))



\* *This Change Log was automatically generated by [github_changelog_generator](https://github.com/skywinder/Github-Changelog-Generator)*