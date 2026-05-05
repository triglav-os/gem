/*
 * Defines resource identifiers and constants for desktop information and
 * preference dialogs. The numeric mappings must stay synchronized with the
 * desktop resources.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */

#define FCB struct fcb

FCB
{
	BYTE		fcb_reserved[21];
	BYTE		fcb_attr;
	WORD		fcb_time;
	WORD		fcb_date;
	LONG		fcb_size;
	BYTE		fcb_name[13];
};

#define ARROW 0
#define HGLASS 2

#define FLOPPY 0
#define HARD 1

