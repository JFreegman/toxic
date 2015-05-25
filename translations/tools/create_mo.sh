#!/bin/bash

cd $(dirname $0)

loc="$1"
while [ -z "$1" -a -z "$loc" ]; do
   echo -n "Insert locale (for example \"en\"): "
   read loc
done

cd ..

if [ ! -e "$loc.po" ]; then
   echo "File \"$loc.po\" not found"
   echo "The translation file must exist"
   exit 1
else
   msgfmt -c -o $loc.mo $loc.po
fi
