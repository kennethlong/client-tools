//
// WriteTGA.h
// asommers 11-15-2000
//
// copyright 2000, verant interactive
//
// Phase 11 Plan 11-02 -- local copy under Direct3d11.
//

//-------------------------------------------------------------------

#ifndef WRITETGA_H
#define WRITETGA_H

//-------------------------------------------------------------------

class WriteTGA
{
public:

	static void write (const char* sharedFilename, int width, int height, const uint8* data, bool hasAlpha, int pitch);
};

//-------------------------------------------------------------------

#endif
