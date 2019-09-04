/*
 * optimopt.h - misc functions
 *
 * Copyright (C) 2002-2019 The EmuTOS development team
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */


#ifndef OPTIMOPT_H
#define OPTIMOPT_H

char *scasb(char *p, char b);
WORD expand_string(WORD *dest, char *src);
WORD mul_div_round(WORD mult1, WORD mult2, WORD divisor);

#endif
