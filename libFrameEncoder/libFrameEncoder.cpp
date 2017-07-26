//////////////////////////////////////////////////////////////////////////
// RGB framebuffer encoder class
// requires libTheora and boost (for threading) to work
//
// Version 1.0 14th November 2006
//	+ Basic RGB encoding of data
//////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "libFrameEncoder.hpp"

#include <cstdio>

namespace FrameEncoder
{
	TheoraEncoder::TheoraEncoder( std::string const& filename, int width, int height, int bpp, int buffercount ) : width(width), height(height), srcbbp(bpp)
	{
		openOutput(filename);
		TheoraSetUp();
		reserveBuffers(buffercount);
	}

	TheoraEncoder::TheoraEncoder( std::string const& filename, int width, int height, int buffercount ) : width(width), height(height), srcbbp(DEFAULT_SOURCE_COLOUR_DEPTH)
	{
		openOutput(filename);
		TheoraSetUp();	
		reserveBuffers(buffercount);
	}
	void TheoraEncoder::TheoraSetUp()
	{
		// Theora has a divisible-by-16 rule for encoded video sizes so we need to resize to make sure things are good
		video_width = ((width + 15) >> 4) << 4;
		video_height = ((height + 15) >> 4) << 4;

		// Setup even offsets so everything lines up
		frame_x_offset=((video_width-width)/2)&~1;
		frame_y_offset=((video_height-height)/2)&~1;

		ogg_stream_init(&to, rand());	// generate a random serial number

		// Initalise Theora
		theora_info_init(&ti);
		ti.width=video_width;
		ti.height=video_height;
		ti.frame_width=width;
		ti.frame_height=height;
		ti.offset_x=frame_x_offset;
		ti.offset_y=frame_y_offset;
		ti.fps_numerator=30000000;
		ti.fps_denominator=1000000;
		ti.aspect_numerator=0;
		ti.aspect_denominator=0;
		ti.colorspace=OC_CS_UNSPECIFIED;
		ti.pixelformat=OC_PF_420;
		ti.target_bitrate=2000000;
		ti.quality=5;

		ti.dropframes_p=0;
		ti.quick_p=1;
		ti.keyframe_auto_p=1;
		ti.keyframe_frequency=64;
		ti.keyframe_frequency_force=64;
		ti.keyframe_data_target_bitrate=2000000*1.5;	// big number = target_bitrate from above
		ti.keyframe_auto_threshold=80;
		ti.keyframe_mindistance=8;
		ti.noise_sensitivity=1;

		theora_encode_init(&td,&ti);
		theora_info_clear(&ti);

		/* write the bitstream header packets with proper page interleave */

		/* first packet will get its own page automatically */
		theora_encode_header(&td,&op);
		ogg_stream_packetin(&to,&op);
		if(ogg_stream_pageout(&to,&og)!=1){
			throw std::runtime_error("TheoraEncoder:: Internal Ogg library error");
			exit(1);
		}
		fwrite(og.header,1,og.header_len,outfile);
		fwrite(og.body,1,og.body_len,outfile);

		/* create the remaining theora headers */
		theora_comment_init(&tc);
		theora_encode_comment(&tc,&op);
		ogg_stream_packetin(&to,&op);
		theora_encode_tables(&td,&op);
		ogg_stream_packetin(&to,&op);
		
		// flush headers so that data stream will start on a new frame
		while(1)
		{
			int result = ogg_stream_flush(&to,&og);
			if(result == 0) break;
			fwrite(og.header,1,og.header_len,outfile);
			fwrite(og.body,1,og.body_len,outfile);
		}
	}

	TheoraEncoder::~TheoraEncoder()
	{
		while(!rgbfreebuffers.empty())
		{
			delete rgbfreebuffers.front();
			rgbfreebuffers.pop();
		}

		delete ybuffer;
		delete ubuffer;
		delete vbuffer;

		closeOutput();
	}

	void TheoraEncoder::reserveBuffers(int buffercount)
	{
		if(buffercount < MIN_BUFFERS) buffercount = MIN_BUFFERS;
		// preallocate buffers of the correct size
		for (int i = 0; i < buffercount; i++)
		{
			rgbfreebuffers.push(new unsigned char[width*height*srcbbp]);
		}

		ybuffer = new unsigned char[video_width*video_height];
		ubuffer = new unsigned char[video_width*video_height];
		vbuffer = new unsigned char[video_width*video_height];
	}

	void TheoraEncoder::openOutput(std::string const& filename)
	{
		// Open our target file
		// yay! C IO!
		outfile = fopen(filename.c_str(), "wb");
	}

	void TheoraEncoder::closeOutput()
	{
		fclose(outfile);
	}

	// This will start the thread off when we want to start our encoding off
	void TheoraEncoder::beginEncoding()
	{
		
	}

	// In MT mode this will flag to end encoding and wait until all the buffers
	// have been processed and flushed before returning
	void TheoraEncoder::endEncoding()
	{
		// end encoding and flush final buffers
		while(rgbusedbuffers.size() > 1)
		{
			Process(false);
			WriteData();
		}
		Process(true);
		WriteData();

		// shut down everything
		ogg_stream_clear(&to);
		theora_clear(&td);
		
	}

	void TheoraEncoder::processBuffer(unsigned char * buffer)
	{
		// queue this buffer for processing
		rgbusedbuffers.push(buffer);
	}

	unsigned char * TheoraEncoder::requestBuffer()
	{
		unsigned char * nextbuffer = rgbfreebuffers.front();
		rgbfreebuffers.pop();
		return nextbuffer;
	}

	void TheoraEncoder::Process()
	{
		if(rgbusedbuffers.size() >= 2)
		{
			Process(false);
			WriteData();
		}
	}

	void TheoraEncoder::WriteData()
	{
		if(!ogg_stream_pageout(&to,&videopage))
			return;	// we don't have enough packet information to flush yet

		fwrite(videopage.header,1,videopage.header_len,outfile);
		fwrite(videopage.body,1,videopage.body_len,outfile);
	}

	void TheoraEncoder::Process(bool lastframe)
	{
		
		// Convert RGB buffer from source to dest
		/*
		Y = ( (  66 * R + 129 * G +  25 * B + 128) >> 8) +  16
		U = ( ( -38 * R -  74 * G + 112 * B + 128) >> 8) + 128
		V = ( ( 112 * R -  94 * G -  18 * B + 128) >> 8) + 128
		*/
		unsigned char * rgbbuffer = rgbusedbuffers.front();
		rgbusedbuffers.pop();
		// We need to copy RGB data from bottom to top as it's flipped
		//for(int i = 0, rgb = 0; i < width*height; i++, rgb+=3)
		int rgb = (width*srcbbp)*(height-1);
		const int rowsize = width*srcbbp;
		for(int i = 0, offset = 0; i < width*height; i++, offset+=srcbbp)
		{
			if(offset == rowsize)
			{
				offset = 0;
				rgb -= rowsize;
			}
			unsigned char R = rgbbuffer[rgb+offset+0]; 
			unsigned char G = rgbbuffer[rgb+offset+1]; 
			unsigned char B = rgbbuffer[rgb+offset+2]; 
			ybuffer[i] = ( (  66 * R + 129 * G +  25 * B + 128) >> 8) +  16;
			ubuffer[i] = ( ( -38 * R -  74 * G + 112 * B + 128) >> 8) + 128;
			vbuffer[i] = ( ( 112 * R -  94 * G -  18 * B + 128) >> 8) + 128;
		}
		// Give the rgb buffer back as free
		rgbfreebuffers.push(rgbbuffer);
		// Half U & V dimensions and filter
		unsigned char * udest = ubuffer;
		unsigned char * usrc = ubuffer;
		unsigned char * vdest = vbuffer;
		unsigned char * vsrc = vbuffer;
		for(int row = 0; row < height; row+=2)
		{
			for (int col = 0; col < width; col+= 2)
			{
				*udest = (usrc[0] + usrc[1] + usrc[width] + usrc[width+1])/4;
				*vdest = (vsrc[0] + vsrc[1] + vsrc[width] + vsrc[width+1])/4;
				
				udest++;	// move on to next dest pixel
				vdest++;
				usrc += 2;	// move on two source pixels
				vsrc += 2;
			}
			// having completed a row we need to skip a row and carry on
			usrc += width;
			vsrc += width;
		}

		// then pass to Theora to encode
		yuv_buffer          yuv;
		yuv.y_width = video_width;
		yuv.y_height = video_height;
		yuv.y_stride = video_width;

		yuv.uv_width =video_width/2;
		yuv.uv_height = video_height/2;
		yuv.uv_stride = video_width/2;

		yuv.y = ybuffer;
		yuv.u = ubuffer;
		yuv.v = vbuffer;

		theora_encode_YUVin(&td, &yuv);
		if(lastframe)
			theora_encode_packetout(&td,1,&op);
		else
			theora_encode_packetout(&td,0,&op);

		ogg_stream_packetin(&to,&op);
	}
}