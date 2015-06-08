#!/bin/sh
sox $1 --bits 16 --encoding signed-integer --endian little $1.raw
