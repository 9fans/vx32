
# Build datasets for benchmarking
all:	gccbinu.tar.zlib gccbinu.tar.bz2 \
	mountains2.jp2 \
	gently.flac gently.ogg

gccbinu.tar.zlib:
	tar cf - ../../gcc-4.1.2-vx32 ../../binutils-2.18-vx32 \
		| ../../vxrun/vxrun ../zlib/ezlib >$@

gccbinu.tar.bz2:
	tar cf - ../../gcc-4.1.2-vx32 ../../binutils-2.18-vx32 \
		| bzip2 >$@

mountains2.jp2: mountains2.jpg
	jasper --input $^ --output $@

gently.wav: gently.mp3
	mpg123 -w $@ $^

gently.flac: gently.wav
	flac $^

gently.ogg: gently.wav
	oggenc $^

# Clean up automatically-built datasets
clean:
	rm -f gently.wav gently.flac gently.ogg

