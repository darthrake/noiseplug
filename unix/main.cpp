#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "binary.h"
#include "make_wav.h"
#include <math.h>

static const int kNumberBuffers = 1;     

static inline short THREEQUARTERS(short x)
{
	return (x >> 2) + (x >> 1);
}

const uint16_t notes[] = {
	-1, 134, 159, 179, 201, 213, 239, 268, 301, 319, 358, 401, 425, 451, 477, 536, 601, 637, 715
};
#define arpnotes (notes + 5)

const uint16_t arpeggio[][2] = {
	{ 0x24, 0x6A },
	{ 0x46, 0x9C },
	{ 0x13, 0x59 },
	{ 0x02, 0x47 },
	{ 0x24, 0x59 },
	{ 0x24, 0x58 },
	{ 0x57, 0xAD },
	{ 0x35, 0x9B }
};

#define ARPSIZE 76

#if 0
uint8_t arpseq1[4][8] = {
	{ 0, 0, 1, 2, 0, 0, 6, 2, },
	{ 0, 0, 1, 2, 0, 0, 1, 7, },
	{ 0, 0, 1, 2, 0, 0, 1, 2, },
	{ 3, 3, 2, 2, 0, 0, 4, 5, },
};
#else
const uint8_t arpseq1[4][4] = {
	{ 0x00, 0x12, 0x00, 0x62 },
	{ 0x00, 0x12, 0x00, 0x17 },
	{ 0x00, 0x12, 0x00, 0x12 },
	{ 0x33, 0x22, 0x00, 0x45 },
};
#endif
const int arpseq2[] = { 0, 1, 0, 1, 0, 1, 0, 2, 3, 3 };

//const uint32_t arptiming = B32(00001100,00110000,11111011,00001100);
const uint8_t arptiming[4] = { 0x0C, 0x30, 0xFB, 0x0C };

const int bassbeat[8] = { 0, 0, 1, 0, 0, 1, 0, 1 };
const int bassline[] = {
	7, 7, 9, 6, 7, 7, 10, 6, 7, 7, 9, 4, 5, 5, 2, 4,
	5, 5, 6, 6, 7, 7, 3, 3, 5, 5, 6, 6
};

const uint8_t leadtimes[] = {
	1, 2, 3, 4, 5, 6, 28, 14
};
const uint8_t leaddata[] = {
	0x67, 0x24, 0x20, 0x27, 0x20, 0x28, 0x89, 0x0, 0x28, 0x20, 0x27, 0x20, 0x28, 0x89, 0x0, 0x28,
	0x20, 0x27, 0x20, 0x28,	0x86, 0x0, 0x44, 0x0, 0x63, 0x24, 0x62, 0xA1, 0xE0, 0xE0, 0xE0, 0xE0,
	0x20, 0x29, 0x20, 0x2A, 0x8B, 0x0, 0x4E, 0x0, 0x6F, 0x30, 0x71, 0xAF, 0xE0, 0xE0, 0xE0, 0xE0,
	0x20, 0x29, 0x20, 0x2A, 0x8B, 0x0, 0x4E, 0x0, 0x6F, 0x30, 0x6F, 0xAC, 0xE0, 0xE0, 0xE0, 0xE0,
	0x65, 0x22, 0x20, 0x65, 0x26, 0x87, 0x0, 0x68, 0x69, 0x2B, 0xAA, 0xC0, 0x67, 0x24, 0x20, 0x67,
	0x28, 0x89, 0x0, 0x68, 0x69, 0x2B, 0xAA, 0xC0, 0x65, 0x22, 0x20, 0x65, 0x26, 0xA7, 0x28, 0x20,
	0x69, 0x2B, 0xAA, 0x29, 0x20, 0x68, 0x29, 0xAA, 0x2B, 0x20, 0x69, 0x28, 0x69, 0x67,
};
const uint8_t leadseq[] = { 0, 1, 0, 2, 0, 1, 0, 3, 4, 5, 6 };
#define LEADSIZE 174

struct leadvoice_s {
	uint8_t ptr, timer;
	uint16_t osc;
} leads[3] = {
	{ LEADSIZE, 0, 0 },
	{ LEADSIZE, 0, 1601 },
	{ LEADSIZE, 0, 3571 },
};

short boosts;

static unsigned char voice_lead(unsigned long i, int voice_nr)
{
#define leadptr leads[voice_nr].ptr
#define lead_osc leads[voice_nr].osc
#define leadtimer leads[voice_nr].timer

	if (leadptr == LEADSIZE)
	{
		if (i == (0x40000 + 0x400 * voice_nr))
		{
			leadptr = -1;
			leadtimer = 1;
		}
		else
			return 0;
	}

	if (0 == (i & 0x0FF))
		boosts &= ~(1 << voice_nr);
	if (0 == (i & 0x1FF))
		leadtimer--;

	if (0 == leadtimer)
		leadptr++;

	short data = leaddata[(leadseq[leadptr >> 4] << 4) | (leadptr & 0xF)];

	if (0 == leadtimer)
	{
		leadtimer = leadtimes[data >> 5];
		boosts |= 1 << voice_nr;
	}

	short melody = data & 0x1F;
	lead_osc += notes[melody];
	short sample = ((lead_osc >> 7) & 0x3F) + ((lead_osc >> 7) & 0x1F);
	return (0 == melody) ? 0 : ((boosts & (1 << voice_nr)) ? sample : THREEQUARTERS(sample));
}
                     
static inline unsigned char voice_arp(unsigned long i)
{
	static uint16_t arp_osc = 0;
	short arpptr = arpseq1[arpseq2[i >> 16]][(i >> 14) & 3];
	if (!(i & (1 << 13)))
		arpptr >>= 4;
	arpptr = arpeggio[arpptr & 0xF][(i >> 8) & 1];
	if (!(i & 0x80))
		arpptr >>= 4;

	int note = arpnotes[arpptr & 0xF];
	arp_osc += note;

	if ((i >> 13) <= 15)
		return 0;

	short timing = arptiming[(i >> 12) & 3];
	if (!((timing << ((i >> 9) & 7)) & 0x80))
		return 0;

	return (arp_osc & (1 << 12)) ? 0 : 35;
}

static inline unsigned char voice_bass(unsigned long i)
{
	static uint16_t bassosc = 0, flangeosc = 0;
	short bassptr = (i >> 13) & 0xF;
	if (i >> 19)
		bassptr |= 0x10;
	int note = notes[bassline[bassptr]];
	if (bassbeat[(i >> 10) & 7])
		note <<= 1;
	bassosc += note;
	flangeosc += note + 1;
	unsigned char ret = ((bassosc >> 8) & 0x7F) + ((flangeosc >> 8) & 0x7F);
	return ((i >> 6) & 0xF) == 0xF ? 0 : ret;
}

static inline short next_sample()
{
	static unsigned long i = 0;//x40000;
	short ret = voice_lead(i, 0) + THREEQUARTERS(voice_lead(i, 1) >> 1) + (voice_lead(i, 2) >> 2) + (voice_bass(i) >> 2) + voice_arp(i);
	i++;
	if ((i >> 13) == ARPSIZE)
		i = 16 << 13;
	return ret;
}

void fill(short *data)
{
	for (int j = 0; j < 4096*256; j++)
	{
		data[j] = (next_sample()*16*8)-0x7FFF;
	}
}

void decodeLeadArray(short *array, uint8_t size, uint8_t *times, uint8_t *notes) {
	for (int i = 0; i < size; i++)
	{
		times[i] = array[i] >> 5;
		notes[i] = array[i] & 0x1F;
	}
}

void encodeLeadArray(short *array, uint8_t size, uint8_t *times, uint8_t *notes) {
	for (int i = 0; i < size; ++i)
	{
		array[i] = (times[i] << 5) | (notes[i] & 0x1F);
	}
}

int main(int argc, char *argv[]) {

	/* sample decode of leaddata array

	uint8_t times[110];
	uint8_t noteeeees[110];
	decodeLeadArray((short *)leaddata, 110, times, noteeeees);

	for (int i = 0; i < 110; ++i)
	{
		printf("time %x value %x\n", times[i], noteeeees[i]);
	}

	*/

	short *data = (short *)calloc(4096*256, sizeof(short));
	fill(data);

	write_wav((char *)"test.wav", 4096*256, data, 8000);
}
