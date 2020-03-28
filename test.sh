#!/bin/bash
set -e


if [ $# != 1 ]; then
	echo Wrong arguments.
	echo "${0} [Policy].c"
fi

cp $1 mythreadlib.c
make
echo -e "--- TEST ---\n\n"
./main
echo -e "--- END OF TEST ---\n\n"
make clean
rm -f mythreadlib.c
