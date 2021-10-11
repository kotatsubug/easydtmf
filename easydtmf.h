/**
 * easydtmf - version 1.0
 * --------------------------------------------------------------------------
 * Copyright (c) 2020 Alexander Bogatyrev
 * This work is distributed under the MIT License.
 *
 * Single-header C library that generates a .WAV file with DTMF tones based on a user input phone number.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define BYTES_PER_SAMPLE 2
#define SAMPLE_RATE 44100
#define NUM_CHANNELS 1
#define AMPLITUDE 16382
#define PCM_FORMAT 1

#pragma pack (1)
typedef struct
{
	uint8_t ChunkID[4];	
	uint32_t ChunkSize;	// 4 + (8 + SubChunk1Size) + (8 + SubChunk2Size)
	uint8_t Format[4];
	
	uint8_t SubChunk1ID[4];
	uint32_t SubChunk1Size;
	
	uint16_t AudioFormat;
	uint16_t NumChannels;
	
	uint32_t SampleRate;
	uint32_t ByteRate; // SampleRate * NumChannels * BitsPerSample / 8
	uint16_t BlockAlign; // NumChannels * BitsPerSample / 8
	uint16_t BitsPerSample;
	
	uint8_t SubChunk2ID[4];
	uint32_t SubChunk2Size;	// NumSamples * NumChannels * BitsPerSample / 8
} WAVEFILE;
#pragma pack ()

// Function prototypes
void build_header(WAVEFILE *header, const size_t numSamples);
int get_dtmf_upper(const char* digit);
int get_dtmf_lower(const char* digit);

int create_dtmf(const char* wavefileName, const double toneLength, const char* phoneNumber)
{
	// Validate phone number
	for (size_t i = 0; i < strlen(phoneNumber); ++i)
	{
		switch(phoneNumber[i])
		{
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '#':
			case '*':
			case '-':
				break;
			default:
				printf("Invalid phone number.\n");
				return 0;
		}
	}
	
	// Validate tone length
	if (toneLength > 1.0 || toneLength < 0.1)
	{
		printf("Tone length must be within range [0.1, 1.0].\n");
		return 0;
	}

	// ------------ Generation of header and samples ------------

	// Total number of samples overall needed
	size_t numSamples = SAMPLE_RATE * strlen(phoneNumber); 

	// Create wavefile
	FILE *oFile;
	
	// Build the wavefile header struct
	WAVEFILE *header;
	header = malloc(sizeof(WAVEFILE));
	build_header(header, numSamples);

	
	int16_t *soundData;
	
	// Allocate enough memory to hold all audio samples
	soundData = malloc(numSamples * NUM_CHANNELS * BYTES_PER_SAMPLE * toneLength);
	
	// Calculate sample values for current DTMF tone
	// Then, move on to the next tone in the phone number until all tones are generated
	
	double freq1;
	double freq2;
	size_t moffset = 0; // Memory offset for writing to *soundData
	
	for (size_t toneIndex = 0; toneIndex < strlen(phoneNumber); ++toneIndex)
	{
		// Get the frequencies from current phone digit...
		freq1 = get_dtmf_upper(&phoneNumber[toneIndex]);
		freq2 = get_dtmf_lower(&phoneNumber[toneIndex]);
		
		// ...then generate samples for that tone
		for (size_t sampleNum = 0; sampleNum <= ((SAMPLE_RATE * toneLength) - 1); ++sampleNum)
		{
			int16_t temp = AMPLITUDE * (
				sin(sampleNum * freq1 * 2 * 3.14159 / SAMPLE_RATE) +
				sin(sampleNum * freq2 * 2 * 3.14159 / SAMPLE_RATE)
			);
			
			*(soundData + moffset) = temp;
			
			moffset++;
		}
	}
	
	// ------------ Writing to file ------------
	
	// Open/create the file for editing
	oFile = fopen(wavefileName, "wb");
	if (oFile == NULL || header == NULL)
	{
		// Something has gone horribly wrong
		printf("Cannot create and/or write to file!\n");
		return 0;
	}

	// Write the header
	if ((fwrite(header, sizeof(WAVEFILE), 1, oFile)) != 1)
	{
		printf("Error writing WAVE header to file!\n");
		return 0;
	}

	// Write the sound data after subchunk 2
	if ((fwrite(soundData, (numSamples * NUM_CHANNELS * BYTES_PER_SAMPLE * toneLength), 1, oFile)) != 1)
	{
		printf("Error writing sound data to file!\n");
		return 0;
	}
	
	fclose(oFile);
	
	free(header);
	free(soundData);
	
	return 1;
}

/* Purpose: Instantiates all elements of the wavefile header.
 * Arguments: header - the wavefile header
 *			 numSamples - total number of samples needed in SubChunk2
 *						  e.g. (sample rate * number of phone number digits)
 * Returns: none
 */
void build_header(WAVEFILE *header, const size_t numSamples)
{
	// Subchunk 1 (fmt): the header of the wavefile, describing the sound data's format
	header->ChunkID[0] = 'R';
	header->ChunkID[1] = 'I';
	header->ChunkID[2] = 'F';
	header->ChunkID[3] = 'F';
	
	header->Format[0] = 'W';
	header->Format[1] = 'A';
	header->Format[2] = 'V';
	header->Format[3] = 'E';
	
	header->SubChunk1ID[0]	= 'f';
	header->SubChunk1ID[1]	= 'm';
	header->SubChunk1ID[2]	= 't';
	header->SubChunk1ID[3]	= ' ';
	
	header->SubChunk1Size	= 16;
	
	header->AudioFormat		= 1;
	header->NumChannels		= NUM_CHANNELS; // 1 for Mono
	header->SampleRate		= SAMPLE_RATE; // 44100
	header->ByteRate		= SAMPLE_RATE * NUM_CHANNELS * BYTES_PER_SAMPLE;
	header->BlockAlign		= NUM_CHANNELS * BYTES_PER_SAMPLE; // Should be 2.
	header->BitsPerSample	= BYTES_PER_SAMPLE * 8; // Should be 16.
	
	// Subchunk 2 (data): size of the data and the actual sound
	header->SubChunk2ID[0]	= 'd';
	header->SubChunk2ID[1]	= 'a';
	header->SubChunk2ID[2]	= 't';
	header->SubChunk2ID[3]	= 'a';
	
	header->SubChunk2Size = numSamples * NUM_CHANNELS * BYTES_PER_SAMPLE;

	// Total size calculation
	header->ChunkSize = 4 + (8 + header->SubChunk1Size) + (8 + header->SubChunk2Size);
}

/* Purpose: Calculates the first DTMF keypad frequency
 * Arguments: digit - pointer to char of symbol to dial
 * Returns: Respective keypad frequency as int
 *		   1209 Hz, 1336 Hz, 1477 Hz, else 0 Hz if digit is a dash
 */
int get_dtmf_upper(const char* digit)
{
	switch(*digit)
	{
		case '1':
		case '4':
		case '7':
		case '*':
			return 1209;
		case '2':
		case '5':
		case '8':
		case '0':
			return 1336;
		case '3':
		case '6':
		case '9':
		case '#':
			return 1477;
		default:
			// Assume '-'
			return 0;
	}
}

/*
 * Purpose: Calculates the second DTMF keypad frequency
 * Arguments: digit - pointer to char of symbol to dial
 * Returns: Respective keypad frequency as int
 *		   697 Hz, 770 Hz, 852 Hz, 941 Hz, else 0 Hz if digit is a dash
 */
int get_dtmf_lower(const char* digit)
{
	switch(*digit)
	{
		case '1':
		case '2':
		case '3':
			return 697;
		case '4':
		case '5':
		case '6':
			return 770;
		case '7':
		case '8':
		case '9':
			return 852;
		case '*':
		case '0':
		case '#':
			return 941;
		default:
			// Assume '-'
			return 0;
	}
}





