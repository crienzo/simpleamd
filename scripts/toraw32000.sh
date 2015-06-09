#!/bin/sh
sox $1 -r 32k --bits 16 --encoding signed-integer --endian little $1.raw
