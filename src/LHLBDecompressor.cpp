/* Copyright (C) Teemu Suutari */

#include "LHLBDecompressor.hpp"

#include "DynamicHuffmanDecoder.hpp"

bool LHLBDecompressor::detectHeaderXPK(uint32_t hdr) noexcept
{
	return hdr==FourCC('LHLB');
}

std::unique_ptr<XPKDecompressor> LHLBDecompressor::create(uint32_t hdr,uint32_t recursionLevel,const Buffer &packedData,std::unique_ptr<XPKDecompressor::State> &state,bool verify)
{
	return std::make_unique<LHLBDecompressor>(hdr,recursionLevel,packedData,state,verify);
}

LHLBDecompressor::LHLBDecompressor(uint32_t hdr,uint32_t recursionLevel,const Buffer &packedData,std::unique_ptr<XPKDecompressor::State> &state,bool verify) :
	XPKDecompressor(recursionLevel),
	_packedData(packedData)
{
	if (!detectHeaderXPK(hdr)) throw Decompressor::InvalidFormatError();
}

LHLBDecompressor::~LHLBDecompressor()
{
	// nothing needed
}

const std::string &LHLBDecompressor::getSubName() const noexcept
{
	static std::string name="XPK-LHLB: LZRW-compressor";
	return name;
}

void LHLBDecompressor::decompressImpl(Buffer &rawData,const Buffer &previousData,bool verify)
{
	// Stream reading
	size_t packedSize=_packedData.size();
	const uint8_t *bufPtr=_packedData.data();
	size_t bufOffset=0;
	uint8_t bufBitsContent=0;
	uint8_t bufBitsLength=0;

	auto readBit=[&]()->uint8_t
	{
		if (!bufBitsLength)
		{
			if (bufOffset>=packedSize) throw Decompressor::DecompressionError();
			bufBitsContent=bufPtr[bufOffset++];
			bufBitsLength=8;
		}
		uint8_t ret=bufBitsContent>>7;
		bufBitsContent<<=1;
		bufBitsLength--;
		return ret;
	};

	auto readBits=[&](uint32_t bits)->uint32_t
	{
		uint32_t ret=0;
		for (uint32_t i=0;i<bits;i++) ret=(ret<<1)|readBit();
		return ret;
	};

	uint8_t *dest=rawData.data();
	size_t destOffset=0;
	size_t rawSize=rawData.size();

	// Same logic as in Choloks pascal implementation
	// In his books LHLB is "almost" -lh1- (I'd assume the difference is in the metadata)

	DynamicHuffmanDecoder<317> decoder;

	while (destOffset!=rawSize)
	{
		uint32_t code=decoder.decode(readBit);
		if (code==316) break;
		if (decoder.getMaxFrequency()<0x8000U) decoder.update(code);

		if (code<256)
		{
			dest[destOffset++]=code;
		} else {
			static const uint8_t distanceHighBits[256]={
				 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
				 0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
				 1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
				 2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2,
				 3, 3, 3, 3, 3, 3, 3, 3,  3, 3, 3, 3, 3, 3, 3, 3,
				 4, 4, 4, 4, 4, 4, 4, 4,  5, 5, 5, 5, 5, 5, 5, 5,
				 6, 6, 6, 6, 6, 6, 6, 6,  7, 7, 7, 7, 7, 7, 7, 7,
				 8, 8, 8, 8, 8, 8, 8, 8,  9, 9, 9, 9, 9, 9, 9, 9,

				10,10,10,10,10,10,10,10, 11,11,11,11,11,11,11,11,
				12,12,12,12,13,13,13,13, 14,14,14,14,15,15,15,15,
				16,16,16,16,17,17,17,17, 18,18,18,18,19,19,19,19,
				20,20,20,20,21,21,21,21, 22,22,22,22,23,23,23,23,
				24,24,25,25,26,26,27,27, 28,28,29,29,30,30,31,31,
				32,32,33,33,34,34,35,35, 36,36,37,37,38,38,39,39,
				40,40,41,41,42,42,43,43, 44,44,45,45,46,46,47,47,
				48,49,50,51,52,53,54,55, 56,57,58,59,60,61,62,63};
			static const uint8_t distanceBits[16]={1,1,2,2,2,3,3,3,3,4,4,4,5,5,5,6};
				
			uint32_t tmp=readBits(8);
			uint32_t distance=uint32_t(distanceHighBits[tmp])<<6;
			uint32_t bits=distanceBits[tmp>>4];
			tmp=(tmp<<bits)|readBits(bits);
			distance|=tmp&63;
			uint32_t count=code-255;

			if (!distance || distance>destOffset || destOffset+count>rawSize) throw Decompressor::DecompressionError();
			for (uint32_t i=0;i<count;i++,destOffset++)
				dest[destOffset]=dest[destOffset-distance];
		}
	}
}

XPKDecompressor::Registry<LHLBDecompressor> LHLBDecompressor::_XPKregistration;
