#!/bin/bash


if [ $# != 1 ]; then
	echo Wrong arguments.
	echo "${0} [Policy].c"
	exit -1
fi

cp $1 mythreadlib.c
make
echo -e "--- TEST ---\n\n"
./main
echo -e "\nTest ended with exit code $?\n--- END OF TEST ---\n\n"
make clean
rm -f mythreadlib.c
