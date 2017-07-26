========================================================================
    STATIC LIBRARY : libFrameEncoder Project Overview
========================================================================

A library to allow drop in encoding of RGB data (framebuffer) to a
compressed ogg stream via Theora

Useage process:
- Initalise with a filename/stream to save to
-- version 1.0 just has a filename for now

Encoding:
- start encoder
-- encoder pre-allocates a series of buffers
-- spawns a worker thread to do the encoding work

per frame:
- request a buffer from encoder lib
-- this can block if the encoder is too far behind
- use supplied buffer to pass RGB/RGBA data to encoder

Encoder:
- Performs colour space conversion from RGB=>YUV via lookup table
-- this frees RGB buffer to be reused as it's copied into an internal buffer
-- UV data needs to be 1/4 that of Y data; need to resample
--- ? simple drop or do a proper filter on the image ?

- YUV is passed to lib Theora for encoding
- when completed write frame data out

End :
- system must request end of encoding
-- lib writes special frame data out and shuts down as required

Notes: 

* no sound support as yet...
* might want to cue a frame in advance before we being encoding, last frame
is flushed on shutdown request

basic API ideas;

void initialiseEncoder(std::string &filename);	// setup stream + internals
void beginEncoding(int width, int height);		// preallocate buffers
char * requestRGBBuffer();						// pass a pointer to a RGB buffer out (handle type maybe?)
void processRGBBuffer(char * buffer);			// process the buffer back again
void endEncodering();							// flush all buffers and close stream
void shutdownEncoder();							// or maybe this name?

class maybe?
Initialise becomes the constructor, the rest member functions...


/////////////////////////////////////////////////////////////////////////////

StdAfx.h, StdAfx.cpp
    These files are used to build a precompiled header (PCH) file
    named libFrameEncoder.pch and a precompiled types file named StdAfx.obj.

/////////////////////////////////////////////////////////////////////////////
Other notes:

AppWizard uses "TODO:" comments to indicate parts of the source code you
should add to or customize.

/////////////////////////////////////////////////////////////////////////////
