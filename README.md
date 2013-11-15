## Toxic - console client for [Tox](http://tox.im)

The client formerly resided in the [Tox core repository](https://github.com/irungentoo/ProjectTox-Core) and is now available as a standalone version.

To compile, first generate the configure script by running the ```autoreconf -i``` command.

Then execute the configure script (you'll probably have to pass it the location of your dependency libraries, i.e.):
```
./configure --prefix=/where/to/install --with-libtoxcore-headers=/path/to/ProjectTox-Core/core --with-libtoxcore-libs=/path/to/ProjectTox-Core/build/core --with-libsodium-headers=/path/to/libsodium/include/ --with-libsodium-libs=/path/to/sodiumtest/lib/

```
*Note:* If your default prefix is /usr/local and you happen to get an error that says "error while loading shared libraries: libtoxcore.so.0: cannot open shared object file: No such file or directory", then you can try running ```sudo ldconfig```. If that doesn't fix it, run:
```
echo '/usr/local/lib/' | sudo tee -a /etc/ld.so.conf.d/locallib.conf
sudo ldconfig
```
