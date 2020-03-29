#!/bin/bash


if [ $# != 2 ]; then
	echo Wrong arguments.
	echo "${0} [Policy].c [Test_main].c"
	exit -1
fi

cp $1 mythreadlib.c
cp $2 main.c
make
echo -e "--- TEST ---\n\n"
./main
echo -e "\n\n--- END OF TEST ---\nTest ended with exit code $?\n\n"
make clean
rm -f mythreadlib.c main.c
