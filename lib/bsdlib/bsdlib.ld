SECTION_DATA_PROLOGUE(.bsdlib,,)
{
	/* This must be kept at top of SRAM2 for BSDlib to be working */
	__bsdlib_start = .;
	KEEP(*(".bsdlib_memory.*"));
	__bsdlib_end = .;
} GROUP_DATA_LINK_IN(SRAM2, ROMABLE_REGION)
