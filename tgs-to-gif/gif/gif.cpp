//
// gif.h
// by Charlie Tangora
// Public domain.
// Email me : ctangora -at- gmail -dot- com
//
// This file offers a simple, very limited way to create animated GIFs directly in code.
//
// Those looking for particular cleverness are likely to be disappointed; it's pretty
// much a straight-ahead implementation of the GIF format with optional Floyd-Steinberg
// dithering. (It does at least use delta encoding - only the changed portions of each
// frame are saved.)
//
// So resulting files are often quite large. The hope is that it will be handy nonetheless
// as a quick and easily-integrated way for programs to spit out animations.
//
// Only RGBA8 is currently supported as an input format. (The alpha is ignored.)
//
// USAGE:
// Create a GifWriter struct. Pass it to GifBegin() to initialize and write the header.
// Pass subsequent frames to GifWriteFrame().
// Finally, call GifEnd() to close the file handle and free memory.
//

#include "gif.h"

const int kGifTransIndex = 0;

template<class T>
inline T GifIAbs(T i) { return i < 0 ? -i : i; }

void GifGetClosestPaletteColor(GifPalette* pPal, int r, int g, int b, int& bestInd, int& bestDiff, int treeRoot) {
	// base case, reached the bottom of the tree
	if (treeRoot > (1 << pPal->bitDepth) - 1) {
		int ind = treeRoot - (1 << pPal->bitDepth);
		if (ind == kGifTransIndex) return;

		// check whether this color is better than the current winner
		int r_err = r - ((int32_t) pPal->r[ind]);
		int g_err = g - ((int32_t) pPal->g[ind]);
		int b_err = b - ((int32_t) pPal->b[ind]);
		int diff = GifIAbs(r_err) + GifIAbs(g_err) + GifIAbs(b_err);

		if (diff < bestDiff) {
			bestInd = ind;
			bestDiff = diff;
		}

		return;
	}

	// take the appropriate color (r, g, or b) for this node of the k-d tree
	int comps[3];
	comps[0] = r;
	comps[1] = g;
	comps[2] = b;
	int splitComp = comps[pPal->treeSplitElt[treeRoot]];

	int splitPos = pPal->treeSplit[treeRoot];
	if (splitPos > splitComp) {
		// check the left subtree
		GifGetClosestPaletteColor(pPal, r, g, b, bestInd, bestDiff, treeRoot * 2);
		if (bestDiff > splitPos - splitComp) {
			// cannot prove there's not a better value in the right subtree, check that too
			GifGetClosestPaletteColor(pPal, r, g, b, bestInd, bestDiff, treeRoot * 2 + 1);
		}
	} else {
		GifGetClosestPaletteColor(pPal, r, g, b, bestInd, bestDiff, treeRoot * 2 + 1);
		if (bestDiff > splitComp - splitPos) {
			GifGetClosestPaletteColor(pPal, r, g, b, bestInd, bestDiff, treeRoot * 2);
		}
	}
}

void GifSwapPixels(uint8_t* image, int pixA, int pixB) {
	uint8_t rA = image[pixA * 4];
	uint8_t gA = image[pixA * 4 + 1];
	uint8_t bA = image[pixA * 4 + 2];
	uint8_t aA = image[pixA * 4 + 3];

	uint8_t rB = image[pixB * 4];
	uint8_t gB = image[pixB * 4 + 1];
	uint8_t bB = image[pixB * 4 + 2];
	uint8_t aB = image[pixA * 4 + 3];

	image[pixA * 4] = rB;
	image[pixA * 4 + 1] = gB;
	image[pixA * 4 + 2] = bB;
	image[pixA * 4 + 3] = aB;

	image[pixB * 4] = rA;
	image[pixB * 4 + 1] = gA;
	image[pixB * 4 + 2] = bA;
	image[pixB * 4 + 3] = aA;
}

// just the partition operation from quicksort
int GifPartition(uint8_t* image, const int left, const int right, const int elt, int pivotIndex) {
	const int pivotValue = image[(pivotIndex) * 4 + elt];
	GifSwapPixels(image, pivotIndex, right - 1);
	int storeIndex = left;
	bool split = 0;
	for (int ii = left; ii < right - 1; ++ii) {
		int arrayVal = image[ii * 4 + elt];
		if (arrayVal < pivotValue) {
			GifSwapPixels(image, ii, storeIndex);
			++storeIndex;
		} else if (arrayVal == pivotValue) {
			if (split) {
				GifSwapPixels(image, ii, storeIndex);
				++storeIndex;
			}
			split = !split;
		}
	}
	GifSwapPixels(image, storeIndex, right - 1);
	return storeIndex;
}

// Perform an incomplete sort, finding all elements above and below the desired median
void GifPartitionByMedian(uint8_t* image, int left, int right, int com, int neededCenter) {
	if (left < right - 1) {
		int pivotIndex = left + (right - left) / 2;

		pivotIndex = GifPartition(image, left, right, com, pivotIndex);

		// Only "sort" the section of the array that contains the median
		if (pivotIndex > neededCenter)
			GifPartitionByMedian(image, left, pivotIndex, com, neededCenter);

		if (pivotIndex < neededCenter)
			GifPartitionByMedian(image, pivotIndex + 1, right, com, neededCenter);
	}
}

// Builds a palette by creating a balanced k-d tree of all pixels in the image
void GifSplitPalette(
	uint8_t* image,
	int numPixels,
	int firstElt,
	int lastElt,
	int splitElt,
	int splitDist,
	int treeNode,
	GifPalette* pal
) {
	if (lastElt <= firstElt || numPixels == 0)
		return;

	// base case, bottom of the tree
	if (lastElt == firstElt + 1) {
		// otherwise, take the average of all colors in this subcube
		uint64_t r = 0, g = 0, b = 0;
		for (int ii = 0; ii < numPixels; ++ii) {
			r += image[ii * 4 + 0];
			g += image[ii * 4 + 1];
			b += image[ii * 4 + 2];
		}

		r += (uint64_t) numPixels / 2;  // round to nearest
		g += (uint64_t) numPixels / 2;
		b += (uint64_t) numPixels / 2;

		r /= (uint64_t) numPixels;
		g /= (uint64_t) numPixels;
		b /= (uint64_t) numPixels;

		pal->r[firstElt] = (uint8_t) r;
		pal->g[firstElt] = (uint8_t) g;
		pal->b[firstElt] = (uint8_t) b;

		return;
	}

	// Find the axis with the largest range
	int minR = 255, maxR = 0;
	int minG = 255, maxG = 0;
	int minB = 255, maxB = 0;
	for (int ii = 0; ii < numPixels; ++ii) {
		int r = image[ii * 4 + 0];
		int g = image[ii * 4 + 1];
		int b = image[ii * 4 + 2];

		if (r > maxR) maxR = r;
		if (r < minR) minR = r;

		if (g > maxG) maxG = g;
		if (g < minG) minG = g;

		if (b > maxB) maxB = b;
		if (b < minB) minB = b;
	}

	int rRange = maxR - minR;
	int gRange = maxG - minG;
	int bRange = maxB - minB;

	// and split along that axis. (incidentally, this means this isn't a "proper" k-d tree but I don't know what else to call it)
	int splitCom = 1;
	if (bRange > gRange) splitCom = 2;
	if (rRange > bRange && rRange > gRange) splitCom = 0;

	int subPixelsA = numPixels * (splitElt - firstElt) / (lastElt - firstElt);
	int subPixelsB = numPixels - subPixelsA;

	GifPartitionByMedian(image, 0, numPixels, splitCom, subPixelsA);

	pal->treeSplitElt[treeNode] = (uint8_t) splitCom;
	pal->treeSplit[treeNode] = image[subPixelsA * 4 + splitCom];

	GifSplitPalette(
		image,
		subPixelsA,
		firstElt,
		splitElt,
		splitElt - splitDist,
		splitDist / 2,
		treeNode * 2,
		pal
	);
	GifSplitPalette(
		image + subPixelsA * 4,
		subPixelsB,
		splitElt,
		lastElt,
		splitElt + splitDist,
		splitDist / 2,
		treeNode * 2 + 1,
		pal
	);
}

// Finds all pixels that have changed from the previous image and
// moves them to the front of th buffer.
// This allows us to build a palette optimized for the colors of the
// changed pixels only.
int GifPickChangedPixels(const uint8_t* lastFrame, uint8_t* frame, int numPixels) {
	int numChanged = 0;
	uint8_t* writeIter = frame;

	for (int ii = 0; ii < numPixels; ++ii) {
		if (lastFrame[0] != frame[0] ||
		    lastFrame[1] != frame[1] ||
		    lastFrame[2] != frame[2]) {
			writeIter[0] = frame[0];
			writeIter[1] = frame[1];
			writeIter[2] = frame[2];
			++numChanged;
			writeIter += 4;
		}
		lastFrame += 4;
		frame += 4;
	}

	return numChanged;
}

// Creates a palette by placing all the image pixels in a k-d tree and then averaging the blocks at the bottom.
// This is known as the "modified median split" technique
void GifMakePalette(
	const uint8_t* lastFrame,
	const uint8_t* nextFrame,
	uint32_t width,
	uint32_t height,
	int bitDepth,
	GifPalette* pPal
) {
	pPal->bitDepth = bitDepth;

	// SplitPalette is destructive (it sorts the pixels by color) so
	// we must create a copy of the image for it to destroy
	size_t imageSize = (size_t) (width * height * 4 * sizeof(uint8_t));
	uint8_t* destroyableImage = (uint8_t*) GIF_TEMP_MALLOC(imageSize);
	memcpy(destroyableImage, nextFrame, imageSize);

	int numPixels = (int) (width * height);
	if (lastFrame) {
		numPixels = GifPickChangedPixels(lastFrame, destroyableImage, numPixels);
	}

	const int lastElt = 1 << bitDepth;
	const int splitElt = lastElt / 2;
	const int splitDist = splitElt / 2;

	GifSplitPalette(destroyableImage, numPixels, 1, lastElt, splitElt, splitDist, 1, pPal);

	GIF_TEMP_FREE(destroyableImage);

	// add the bottom node for the transparency index
	pPal->treeSplit[1 << (bitDepth - 1)] = 0;
	pPal->treeSplitElt[1 << (bitDepth - 1)] = 0;

	pPal->r[0] = pPal->g[0] = pPal->b[0] = 0;
}

// Picks palette colors for the image using simple thresholding, no dithering
void GifThresholdImage(
	const uint8_t* lastFrame,
	const uint8_t* nextFrame,
	uint8_t* outFrame,
	uint32_t width,
	uint32_t height,
	GifPalette* pPal,
	uint8_t alphaThreshold
) {
	uint32_t numPixels = width * height;
	for (uint32_t ii = 0; ii < numPixels; ++ii) {
		uint8_t result[4];
		if (nextFrame[3] >= alphaThreshold) {
			// palettize the pixel
			int32_t bestDiff = 1000000;
			int32_t bestInd = 1;
			GifGetClosestPaletteColor(pPal, nextFrame[0], nextFrame[1], nextFrame[2], bestInd, bestDiff);

			// Write the resulting color to the output buffer
			result[0] = pPal->r[bestInd];
			result[1] = pPal->g[bestInd];
			result[2] = pPal->b[bestInd];
			result[3] = (uint8_t) bestInd;
		} else {
			result[0] = 0;
			result[1] = 0;
			result[2] = 0;
			result[3] = kGifTransIndex;
		}
		// if a previous color is available, and it matches the current color,
		// set the pixel to transparent
		if (alphaThreshold == 0 && lastFrame &&
		    lastFrame[0] == result[0] &&
		    lastFrame[1] == result[1] &&
		    lastFrame[2] == result[2]) {
			outFrame[0] = lastFrame[0];
			outFrame[1] = lastFrame[1];
			outFrame[2] = lastFrame[2];
			outFrame[3] = kGifTransIndex;
		} else {
			outFrame[0] = result[0];
			outFrame[1] = result[1];
			outFrame[2] = result[2];
			outFrame[3] = result[3];
		}

		if (lastFrame) lastFrame += 4;
		outFrame += 4;
		nextFrame += 4;
	}
}

// insert a single bit
void GifWriteBit(GifBitStatus& stat, uint32_t bit) {
	bit = bit & 1;
	bit = bit << stat.bitIndex;
	stat.byte |= bit;

	++stat.bitIndex;
	if (stat.bitIndex > 7) {
		// move the newly-finished byte to the chunk buffer
		stat.chunk[stat.chunkIndex++] = stat.byte;
		// and start a new byte
		stat.bitIndex = 0;
		stat.byte = 0;
	}
}

// write all bytes so far to the file
void GifWriteChunk(FILE* f, GifBitStatus& stat) {
	fputc((int) stat.chunkIndex, f);
	fwrite(stat.chunk, 1, stat.chunkIndex, f);

	stat.bitIndex = 0;
	stat.byte = 0;
	stat.chunkIndex = 0;
}

void GifWriteCode(FILE* f, GifBitStatus& stat, uint32_t code, uint32_t length) {
	for (uint32_t ii = 0; ii < length; ++ii) {
		GifWriteBit(stat, code);
		code = code >> 1;

		if (stat.chunkIndex == 255) {
			GifWriteChunk(f, stat);
		}
	}
}

void GifWritePalette(const GifPalette* pPal, FILE* f) {
	fputc(0, f);  // first color: transparency
	fputc(0, f);
	fputc(0, f);

	for (int ii = 1; ii < (1 << pPal->bitDepth); ++ii) {
		uint32_t r = pPal->r[ii];
		uint32_t g = pPal->g[ii];
		uint32_t b = pPal->b[ii];

		fputc((int) r, f);
		fputc((int) g, f);
		fputc((int) b, f);
	}
}

// write the image header, LZW-compress and write out the image
void GifWriteLzwImage(
	FILE* f,
	uint8_t* image,
	uint32_t left,
	uint32_t top,
	uint32_t width,
	uint32_t height,
	uint32_t delay,
	GifPalette* pPal,
	uint8_t transparent
) {
	// graphics control extension
	fputc(0x21, f);
	fputc(0xf9, f);
	fputc(0x04, f);
	if (transparent) {
		fputc((0x02 << 2) | 0x1, f); // clear prev frame for transparent images
	} else {
		fputc(0x05, f); // leave prev frame in place, this frame has transparency
	}
	fputc(delay & 0xff, f);
	fputc((delay >> 8) & 0xff, f);
	fputc(kGifTransIndex, f); // transparent color index
	fputc(0, f);

	fputc(0x2c, f); // image descriptor block

	fputc(left & 0xff, f);           // corner of image in canvas space
	fputc((left >> 8) & 0xff, f);
	fputc(top & 0xff, f);
	fputc((top >> 8) & 0xff, f);

	fputc(width & 0xff, f);          // width and height of image
	fputc((width >> 8) & 0xff, f);
	fputc(height & 0xff, f);
	fputc((height >> 8) & 0xff, f);

	//fputc(0, f); // no local color table, no transparency
	//fputc(0x80, f); // no local color table, but transparency

	fputc(0x80 + pPal->bitDepth - 1, f); // local color table present, 2 ^ bitDepth entries
	GifWritePalette(pPal, f);

	const int minCodeSize = pPal->bitDepth;
	const uint32_t clearCode = 1 << pPal->bitDepth;

	fputc(minCodeSize, f); // min code size 8 bits

	GifLzwNode* codetree = (GifLzwNode*) GIF_TEMP_MALLOC(sizeof(GifLzwNode) * 4096);

	memset(codetree, 0, sizeof(GifLzwNode) * 4096);
	int32_t curCode = -1;
	uint32_t codeSize = (uint32_t) minCodeSize + 1;
	uint32_t maxCode = clearCode + 1;

	GifBitStatus stat;
	stat.byte = 0;
	stat.bitIndex = 0;
	stat.chunkIndex = 0;

	GifWriteCode(f, stat, clearCode, codeSize);  // start with a fresh LZW dictionary

	for (uint32_t yy = 0; yy < height; ++yy) {
		for (uint32_t xx = 0; xx < width; ++xx) {
			uint8_t nextValue = image[(yy * width + xx) * 4 + 3];

			// "loser mode" - no compression, every single code is followed immediately by a clear
			//WriteCode( f, stat, nextValue, codeSize );
			//WriteCode( f, stat, 256, codeSize );

			if (curCode < 0) {
				// first value in a new run
				curCode = nextValue;
			} else if (codetree[curCode].m_next[nextValue]) {
				// current run already in the dictionary
				curCode = codetree[curCode].m_next[nextValue];
			} else {
				// finish the current run, write a code
				GifWriteCode(f, stat, (uint32_t) curCode, codeSize);

				// insert the new run into the dictionary
				codetree[curCode].m_next[nextValue] = (uint16_t) ++maxCode;

				if (maxCode >= (1ul << codeSize)) {
					// dictionary entry count has broken a size barrier,
					// we need more bits for codes
					codeSize++;
				}
				if (maxCode == 4095) {
					// the dictionary is full, clear it out and begin anew
					GifWriteCode(f, stat, clearCode, codeSize); // clear tree

					memset(codetree, 0, sizeof(GifLzwNode) * 4096);
					codeSize = (uint32_t) (minCodeSize + 1);
					maxCode = clearCode + 1;
				}

				curCode = nextValue;
			}
		}
	}

	// compression footer
	GifWriteCode(f, stat, (uint32_t) curCode, codeSize);
	GifWriteCode(f, stat, clearCode, codeSize);
	GifWriteCode(f, stat, clearCode + 1, (uint32_t) minCodeSize + 1);

	// write out the last partial chunk
	while (stat.bitIndex) GifWriteBit(stat, 0);
	if (stat.chunkIndex) GifWriteChunk(f, stat);

	fputc(0, f); // image block terminator

	GIF_TEMP_FREE(codetree);
}

bool GifBegin(
	GifWriter* writer,
	const char* filename,
	uint32_t width,
	uint32_t height,
	uint32_t delay,
	int32_t bitDepth
) {
	(void) bitDepth;
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
	writer->f = 0;
	fopen_s(&writer->f, filename, "wb");
#else
	writer->f = fopen(filename, "wb");
#endif
	if (!writer->f) return false;

	writer->firstFrame = true;

	// allocate
	writer->oldImage = (uint8_t*) GIF_MALLOC(width * height * 4);

	fputs("GIF89a", writer->f);

	// screen descriptor
	fputc(width & 0xff, writer->f);
	fputc((width >> 8) & 0xff, writer->f);
	fputc(height & 0xff, writer->f);
	fputc((height >> 8) & 0xff, writer->f);

	fputc(0xf0, writer->f);  // there is an unsorted global color table of 2 entries
	fputc(0, writer->f);     // background color
	fputc(0, writer->f);     // pixels are square (we need to specify this because it's 1989)

	// now the "global" palette (really just a dummy palette)
	// color 0: black
	fputc(0, writer->f);
	fputc(0, writer->f);
	fputc(0, writer->f);
	// color 1: also black
	fputc(0, writer->f);
	fputc(0, writer->f);
	fputc(0, writer->f);

	if (delay != 0) {
		// animation header
		fputc(0x21, writer->f); // extension
		fputc(0xff, writer->f); // application specific
		fputc(11, writer->f); // length 11
		fputs("NETSCAPE2.0", writer->f); // yes, really
		fputc(3, writer->f); // 3 bytes of NETSCAPE2.0 data

		fputc(1, writer->f); // JUST BECAUSE
		fputc(0, writer->f); // loop infinitely (byte 0)
		fputc(0, writer->f); // loop infinitely (byte 1)

		fputc(0, writer->f); // block terminator
	}

	return true;
}

bool GifWriteFrame(
	GifWriter* writer,
	const uint8_t* image,
	uint32_t width,
	uint32_t height,
	uint32_t delay,
	int bitDepth,
	uint8_t alphaThreshold
) {
	if (!writer->f) return false;

	const uint8_t* oldImage = writer->firstFrame ? NULL : writer->oldImage;
	writer->firstFrame = false;

	GifPalette pal;
	GifMakePalette(oldImage, image, width, height, bitDepth, &pal);

	GifThresholdImage(oldImage, image, writer->oldImage, width, height, &pal, alphaThreshold);

	GifWriteLzwImage(writer->f, writer->oldImage, 0, 0, width, height, delay, &pal, alphaThreshold > 0);

	return true;
}

// Writes the EOF code, closes the file handle, and frees temp memory used by a GIF.
// Many if not most viewers will still display a GIF properly if the EOF code is missing,
// but it's still a good idea to write it out.
bool GifEnd(GifWriter* writer) {
	if (!writer->f) return false;

	fputc(0x3b, writer->f); // end of file
	fclose(writer->f);
	GIF_FREE(writer->oldImage);

	writer->f = NULL;
	writer->oldImage = NULL;

	return true;
}
