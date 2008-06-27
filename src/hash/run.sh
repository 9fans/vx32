#!/bin/sh
# Simple script to run benchmarks on all the hash functions
# and append the results to a file called results.txt

echo >>results.txt
echo "Test on "`uname -psr`" at "`date` >>results.txt
for hash in md5 sha1 sha2 ripemd whirlpool; do
	echo "$hash"
	echo "$hash" >results.txt.tmp

	# Native execution timings
	echo "native	" >>results.txt.tmp
	(dd if=/dev/zero bs=32768 count=16384 2>/dev/null | \
			time -p ./h$hash >/dev/null) \
		2>>results.txt.tmp

	# VX32 execution timings
	echo "vx32	" >>results.txt.tmp
	(dd if=/dev/zero bs=32768 count=16384 2>/dev/null | \
			time -p ../vxrun/vxrun ./v$hash >/dev/null) \
		2>>results.txt.tmp

	tr -s " \n" "\t\t" <results.txt.tmp >>results.txt
	echo >>results.txt
done
rm -f results.txt.tmp

