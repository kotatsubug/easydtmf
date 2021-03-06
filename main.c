#include <stdio.h>

#include "easydtmf.h"

int main(int argc, char* argv[])
{
	// Validate argument count
	if (argc != 4) 
	{
		printf("Improper number of arguments.\n");
		return (-1);
	}
	
	// Initialize vars from CLI arguments
	char wavefileName[64];
	strcpy(wavefileName, argv[1]);
	double toneLength = (atof(argv[2]));
	char phoneNumber[64]; 
	strcpy(phoneNumber, argv[3]);
	
	// Create WAV file
	int result = create_dtmf(wavefileName, toneLength, phoneNumber);
	
	return (result ? 0 : (-1));
}