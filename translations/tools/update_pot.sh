#!/bin/bash

cd $(dirname $0)/..
xgettext -d toxic -o toxic.pot ../src/*
