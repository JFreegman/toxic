#!/bin/bash

cd $(dirname $0)/..
v=$(grep TOXIC_VERSION ../cfg/global_vars.mk | head -1 | cut -d "=" -f 2 | tr -d " ")
xgettext --default-domain="toxic" \
	--from-code="UTF-8" \
	--copyright-holder="Toxic Team" \
	--msgid-bugs-address="JFreegman@tox.im" \
	--package-name="Toxic" \
	--package-version="$v" \
	--output="toxic.pot" \
	../src/*
