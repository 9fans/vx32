#!/usr/bin/perl

open(SYS, "/usr/include/asm/unistd.h") || die "open /usr/include/asm/unistd.h: $!";

print <<'EOF';
#include <stdio.h>
#include <asm/unistd.h>

EOF

print "static char *syscall_names[] = {\n";
while(<SYS>){
	if(/#define __NR_(\S+)\s+\d+\s*$/) {
		print "\t[__NR_$1]= \"$1\",\n";
	}
}
print "};\n\n";

print <<'EOF';

#define nelem(x) (sizeof(x)/sizeof((x)[0]))

char *strsyscall(int n) {
	if (0 <= n && n < nelem(syscall_names) && syscall_names[n])
		return syscall_names[n];
	static char buf[40];
	snprintf(buf, sizeof buf, "syscall%#x", n);
	return buf;
}

EOF

