//////////////////////////////////////////////////////////////////////////
// libFrameEncoder
//
// RGB framebuffer encoder class
// requires libTheora and boost (for threading) to work
//
// Version 1.0 14th November 2006
//	+ Basic RGB encoding of data
//////////////////////////////////////////////////////////////////////////
#ifndef LIBFRAMEENCODER_HPP
#define LIBFRAMEENCODER_HPP
#include <string>
#include <queue>
#include <cstdio>

#include "theora/theora.h"

namespace FrameEncoder
{

	class TheoraEncoder
	{
	public:
		TheoraEncoder(std::string const& filename, int width, int height, int buffercount = 2);
		TheoraEncoder(std::string const& filename, int width, int height, int bpp, int buffercount = 2);
		~TheoraEncoder();

		void beginEncoding();
		unsigned char * requestBuffer();
		void processBuffer(unsigned char * buffer);
		void endEncoding();

		void Process();	// TEMP!!!! this will be a threaded section!

	protected:
	private:
		void openOutput(std::string const& filename);
		void TheoraSetUp();
		void reserveBuffers(int buffercount);
		void Process(bool lastframe);
		void WriteData();
		void closeOutput();

		std::queue<unsigned char *> rgbfreebuffers;
		std::queue<unsigned char *> rgbusedbuffers;

		unsigned char * ybuffer;
		unsigned char * vbuffer;
		unsigned char * ubuffer;

		int height, width, video_height, video_width, frame_x_offset, frame_y_offset;

		ogg_page videopage;
		
		ogg_stream_state to; /* take physical pages, weld into a logical
							 stream of packets */
		ogg_page         og; /* one Ogg bitstream page.  Vorbis packets are inside */
		ogg_packet       op; /* one raw packet of data for decode */

		theora_state     td;
		theora_info      ti;
		theora_comment   tc;

		FILE * outfile;

		int srcbbp;			// bytes per pixel for the source data buffer
		static const int MIN_BUFFERS = 2;
		static const int DEFAULT_SOURCE_COLOUR_DEPTH = 3;	// default bytes per pixel

	};

}

#endif