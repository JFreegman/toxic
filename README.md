## Toxic - console client for [Tox](http://tox.im)

The client formerly resided in the [Tox core repository](https://github.com/irungentoo/ProjectTox-Core) and is now available as a standalone version.

To compile, first generate the configure script by running the ```autoreconf -i``` command.

Then execute the configure script (you'll probably have to pass it the location of your dependency libraries, i.e.):
```
./configure --prefix=/where/to/install --with-libtoxcore-headers=/path/to/ProjectTox-Core/core --with-libtoxcore-libs=/path/to/ProjectTox-Core/build/core --with-libsodium-headers=/path/to/libsodium/include/ --with-libsodium-libs=/path/to/sodiumtest/lib/
