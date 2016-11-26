/*
 *  getsizes.c  --  get sizeof() various data types and print it.
 *
 *  Copyright (C) 1999-2000 by Massimiliano Ghilardi
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "autoconf.h"

#ifdef TW_HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

/* This mess was adapted from the GNU getpagesize.h.  */
#ifdef TW_HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifndef TW_HAVE_GETPAGESIZE

# ifdef _SC_PAGESIZE
#  define getpagesize() sysconf(_SC_PAGESIZE)
# else /* no _SC_PAGESIZE */
#  ifdef TW_HAVE_ASM_PAGE_H
#   include <asm/page.h>
#  endif
#  ifdef TW_HAVE_SYS_PARAM_H
#   include <sys/param.h>
#  endif
#  ifdef EXEC_PAGESIZE
#   define getpagesize() EXEC_PAGESIZE
#  else /* no EXEC_PAGESIZE */
#   ifdef NBPG
#    define getpagesize() NBPG * CLSIZE
#    ifndef CLSIZE
#     define CLSIZE 1
#    endif /* no CLSIZE */
#   else /* no NBPG */
#    ifdef NBPC
#     define getpagesize() NBPC
#    else /* no NBPC */
#     ifdef PAGE_SIZE
#      define getpagesize() PAGE_SIZE
#     else /* no PAGE_SIZE */
#      ifdef PAGESIZE      
#	define getpagesize() PAGESIZE
#      else
#	error cannot detect mmap() page size
#      endif /* no PAGESIZE */
#     endif /* no PAGE_SIZE */
#    endif /* no NBPC */
#   endif /* no NBPG */
#  endif /* no EXEC_PAGESIZE */
# endif /* no _SC_PAGESIZE */
#endif /* no TW_HAVE_GETPAGESIZE */


static int my_memcmp(char *m1, char *m2, int len) {
    int c = 0;
    while (len--) {
	if ((c = (int)*m1++ - (int)*m2++))
	    return c;
    }
    return c;
}

static void endianity_error(unsigned data, char *str) {
    int i;
    fprintf(stderr,
	    "Fatal: cannot determine byte order: not little endian, not big endian!\n"
	    "       endianity test on data 0x%8X returned string \"", data);
    for (i = 0; i < sizeof(unsigned); i++)
	fprintf(stderr, "\\x%2X", (unsigned)(unsigned char)*str++);
    fprintf(stderr, "\" !\n");
}

int main(void) {
    unsigned endian_data = 0x04030201;
    char *endian_str = (char *)&endian_data;
    char *byte_order;
    
    if (!my_memcmp(endian_str, "\1\2\3\4", 4))
	byte_order = "1234"; /* little endian */
    else if (!my_memcmp(endian_str + (sizeof(unsigned) - 4), "\4\3\2\1", 4))
	byte_order = "4321"; /* big endian */
    else {
	endianity_error(endian_data, endian_str);
	return 1;
    }
    
    printf("\n"
	   "/*\n"
	   " * This file was automatically generated by 'scripts/Endianity'. Do no edit!\n"
	   " */\n"
	   "\n"
	   "#ifndef _TW_ENDIANITY_H\n"
	   "#define _TW_ENDIANITY_H\n"
	   "\n"
	   "#define TW_PAGE_SIZE        %d\n"
	   "\n"
	   "#define TW_BYTE_ORDER       %s\n"
	   "#define TW_LITTLE_ENDIAN    1234\n"
	   "#define TW_BIG_ENDIAN       4321\n"
	   "\n"
	   "#endif /* _TW_ENDIANITY_H */\n",

	   (int)getpagesize(),
	   byte_order
	   );
    
    return 0;
}

