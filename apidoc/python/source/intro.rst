=========================
Toxic Scripting Interface
=========================

A Python scripting interface to `Toxic <https://github.com/JFreegman/toxic>`_.


Getting Started
===============
Toxic is not compiled with Python support by default. To use the scripting interface, first compile toxic with the ENABLE_PYTHON=1 make option. You can then access it by importing "toxic_api" in your Python script.

Python scripts can be both executed and registered in Toxic by issuing "/run <path>". You can also place any number of Python scripts in the "autorun_path" directory in your toxic configuration file to automatically run the scripts when Toxic starts (see the toxic.conf man page for more info).
