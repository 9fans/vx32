#
# Copyright (c) 1994, 1998 University of Utah and the Flux Group.
# All rights reserved.
# 
# This file is part of the Flux OSKit.  The OSKit is free software, also known
# as "open source;" you can redistribute it and/or modify it under the terms
# of the GNU General Public License (GPL), version 2, as published by the Free
# Software Foundation (FSF).  To explore alternate licensing terms, contact
# the University of Utah at csl-dist@cs.utah.edu or +1-801-585-3271.
# 
# The OSKit is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GPL for more details.  You should have
# received a copy of the GPL along with the OSKit; see the file COPYING.  If
# not, write to the FSF, 59 Temple Place #330, Boston, MA 02111-1307, USA.
#

# This awk program takes a `.sym' file and produces a `.c' file,
# which can then be compiled to assembly language
# and run through a magic sed transformation (see GNUmakerules),
# to produce a `.h' file containing absolute numeric symbol definitions
# suitable for inclusion in assembly language source files and such.
# This way, convenient symbols can be defined based on structure offsets,
# arbitrary C expressions including sizeof() expressions, etc.
#
# Since this mechanism doesn't depend on running a program
# on the machine the code is to run on (the host machine),
# things still work fine during cross-compilation.
#

BEGIN {
	bogus_printed = "no"

	# De-privatize all class members so we can take their offsets.
	print "#define class struct";
	print "#define private public";
}

# Start the bogus function just before the first sym directive,
# so that any #includes higher in the file don't get stuffed inside it.
/^[a-z]/ {
	if (bogus_printed == "no")
	{
		print "void bogus() {";
		bogus_printed = "yes";
	}
}

# Take an arbitrarily complex C symbol or expression
# and turn it into a simple #define'd constant symbol.
/^expr/ {
	printf "__asm (\"\\n#define %s mAgIc%%0\" : : \"i\" (%s));\n", $2, $3;
}

# Output a symbol defining the size of a C structure.
/^size/ {
	printf "__asm (\"\\n#define %s mAgIc%%0\" : : \"i\" (sizeof(struct %s)));\n",
		$2, $3;
}

# Output a symbol defining the byte offset of an element of a C structure.
/^offset/ {
	printf "__asm (\"\\n#define %s mAgIc%%0\" : : \"i\" (&((struct %s*)0)->%s));\n",
		$2, $3, $4;
}

# Copy through all preprocessor directives.
/^#/ {
	print
}

END {
	print "}"
}

