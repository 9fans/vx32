#!/bin/sh
# Simple script to run benchmarks on all the microbenchmarks
# and append the results to a file called results.txt

runprim()
{
	prog=$1
	hcmd=$2
	vcmd=$3

	#echo "hcmd $hcmd"
	#echo "vcmd $vcmd"

	echo "$prog"
	echo "$prog" >results.txt.tmp

	# Native execution timings
	echo "native	" >>results.txt.tmp
	(time -p $hcmd) 2>>results.txt.tmp

	# VX32 execution timings
	echo "vx32	" >>results.txt.tmp
	(time -p $vcmd) 2>>results.txt.tmp

	tr -s " \n" "\t\t" <results.txt.tmp >>results.txt
	echo >>results.txt
}

runtest()
{
	runprim $1 "./h$1" "../vxrun/vxrun ./v$1"
}

echo >>results.txt
echo "Test on "`uname -psr`" at "`date` >>results.txt

runtest jump
runtest jumpal
runtest jumpfar
runtest call
runtest callind
runtest syscall
runtest read
runtest write
runprim nullrun "./hrepeat 1000 ./hnull" "../vxrun/vxrun -r1000 ./vnull"
runtest 64add
runtest 64div
runtest 64mul

rm -f results.txt.tmp

