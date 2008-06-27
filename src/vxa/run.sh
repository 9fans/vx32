#!/bin/sh
# Simple script to run benchmarks on all the hash functions
# and append the results to a file called results.txt

runtest()
{
	prog=$1
	data=$2

	echo "$prog"
	echo "$prog" >results.txt.tmp

	# Native execution timings
	echo "native	" >>results.txt.tmp
	(time -p $prog/hd$prog <$data >/dev/null ) \
		2>>results.txt.tmp

	# VX32 execution timings
	echo "vx32	" >>results.txt.tmp
	(time -p ../vxrun/vxrun $prog/d$prog <$data >/dev/null ) \
		2>>results.txt.tmp

	tr -s " \n" "\t\t" <results.txt.tmp >>results.txt
	echo >>results.txt
}

echo >>results.txt
echo "Test on "`uname -psr`" at "`date` >>results.txt

runtest zlib data/gccbinu.tar.zlib
runtest bz2 data/gccbinu.tar.bz2
runtest jpeg data/mountains2.jpg
runtest jp2 data/mountains2.jp2
runtest vorbis data/gently.ogg
runtest flac data/gently.flac

rm -f results.txt.tmp

