#!/bin/sh
chmod +x configure && CFLAGS="-Wno-implicit-function-declaration -Wno-implicit-int" ./configure
make bash2py
ls -l bash2py
