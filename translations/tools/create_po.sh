#!/bin/bash

cd $(dirname $0)

loc="$1"
while [ -z "$1" -a -z "$loc" ]; do
   echo -n "Insert locale to create (for example \"en\"): "
   read loc
done

cd ..

if [ -e "$loc.po" ]; then
   echo "File \"$loc.po\" found"
   echo "The translation file must not exist"
   exit 1
else
   msginit --no-translator -l $loc -o $loc.po -i toxic.pot
   v=$(grep TOXIC_VERSION ../cfg/global_vars.mk | head -1 | cut -d "=" -f 2 | tr -d " ")
   sed -e '1,5{/^#/d;}' $loc.po > $loc.po.tmp && mv $loc.po.tmp $loc.po
   sed -e 's/.*Project-Id-Version:.*/"Project-Id-Version: Toxic '"$v"'\\n"/' $loc.po > $loc.po.tmp && mv $loc.po.tmp $loc.po
fi
