/*
 * Copyright (c) 2010-2016 Stephane Poirier
 *
 * stephane.poirier@oifii.org
 *
 * Stephane Poirier
 * 3532 rue Ste-Famille, #3
 * Montreal, QC, H2X 2L1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
 * The text above constitutes the entire PortAudio license; however, 
 * the PortAudio community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also 
 * requested that these non-binding requests be included along with the 
 * license above.
 */

#include <stdio.h>
#include <math.h>
#include "portaudio.h"
#include "pa_asio.h"

#include <string>
#include <map>
using namespace std;
#include <windows.h>
#include <assert.h>

map<string,int> global_devicemap;


#define NUM_SECONDS   (10)
#define SAMPLE_RATE   (44100)
#define AMPLITUDE     (0.8)
#define FRAMES_PER_BUFFER  (64)
#define OUTPUT_DEVICE Pa_GetDefaultOutputDevice()

#ifndef M_PI
#define M_PI  (3.14159265)
#endif

#define TABLE_SIZE   (200)
typedef struct
{
    float sine[TABLE_SIZE];
    int phase;
}
paTestData;

// This routine will be called by the PortAudio engine when audio is needed.
// It may called at interrupt level on some machines so don't do anything
// that could mess up the system like calling malloc() or free().
static int patestCallback( const void *inputBuffer, void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData )
{
    paTestData *data = (paTestData*)userData;
    float *out = (float*)outputBuffer;
    unsigned long i;
    int finished = 0;
    //avoid unused variable warnings 
    (void) inputBuffer;
    (void) timeInfo;
    (void) statusFlags;
    for( i=0; i<framesPerBuffer; i++ )
    {
        *out++ = data->sine[data->phase];  // left 
        *out++ = data->sine[data->phase];  // right 
        data->phase += 1;
        if( data->phase >= TABLE_SIZE ) data->phase -= TABLE_SIZE;
    }
    return finished;
}

//////////////////////////////////////////////////////////////
int main(int argc, char **argv);
int main(int argc, char **argv)
{
    PaStreamParameters outputParameters;
    PaAsioStreamInfo asioOutputInfo;
    PaStream *stream;
    PaError err;
    paTestData data;
    int i;
    printf("PortAudio Test: output MONO sine wave. SR = %d, BufSize = %d\n", SAMPLE_RATE, FRAMES_PER_BUFFER);
    //initialise sinusoidal wavetable 
    for( i=0; i<TABLE_SIZE; i++ )
    {
        data.sine[i] = (float) (AMPLITUDE * sin( ((double)i/(double)TABLE_SIZE) * M_PI * 2. ));
    }
    data.phase = 0;
    
	string selecteddevicename="E-MU ASIO";
	if(argc>1)
	{
		selecteddevicename = argv[1]; //for spi, device name could be "E-MU ASIO", 
	}
    int outputChannelSelectors[2]; //int outputChannelSelectors[1];
	/*
	outputChannelSelectors[0] = 0; // on emu patchmix ASIO device channel 1 (left)
	outputChannelSelectors[1] = 1; // on emu patchmix ASIO device channel 2 (right)
	*/
	/*
	outputChannelSelectors[0] = 2; // on emu patchmix ASIO device channel 3 (left)
	outputChannelSelectors[1] = 3; // on emu patchmix ASIO device channel 4 (right)
	*/
	outputChannelSelectors[0] = 6; // on emu patchmix ASIO device channel 7 (left)
	outputChannelSelectors[1] = 7; // on emu patchmix ASIO device channel 8 (right)
	if(argc>2)
	{
		outputChannelSelectors[0]=atoi(argv[2]); //0 for first asio channel (left) or 2, 4, 6 and 8 for spi (maxed out at 10 asio output channel)
	}
	if(argc>3)
	{
		outputChannelSelectors[1]=atoi(argv[2]); //1 for second asio channel (right) or 3, 5, 7 and 9 for spi (maxed out at 10 asio output channel)
	}

    err = Pa_Initialize();
    if( err != paNoError ) 
	{
		//goto error;
		assert(false);
	}

	const PaDeviceInfo* deviceInfo;
    int numDevices = Pa_GetDeviceCount();
    for( i=0; i<numDevices; i++ )
    {
        deviceInfo = Pa_GetDeviceInfo( i );
		string devicenamestring = deviceInfo->name;
		global_devicemap.insert(pair<string,int>(devicenamestring,i));
	}

	map<string,int>::iterator it;
	it = global_devicemap.find(selecteddevicename);
	if(it!=global_devicemap.end())
	{
		int deviceid = (*it).second;
		printf("%s maps to %d\n", selecteddevicename.c_str(), deviceid);
		deviceInfo = Pa_GetDeviceInfo(deviceid);
		//deviceInfo->maxInputChannels
		assert(outputChannelSelectors[0]<deviceInfo->maxOutputChannels);
		assert(outputChannelSelectors[1]<deviceInfo->maxOutputChannels);
	}
	else
	{
		for(it=global_devicemap.begin(); it!=global_devicemap.end(); it++)
		{
			printf("%s maps to %d\n", (*it).first.c_str(), (*it).second);
		}
		Pa_Terminate();
		return -1;
	}

    outputParameters.device = (*it).second; //outputParameters.device = OUTPUT_DEVICE;
    outputParameters.channelCount = 2; //outputParameters.channelCount = 1; //MONO output
    outputParameters.sampleFormat = paFloat32; //32 bit floating point output
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;

	//Use an ASIO specific structure. WARNING - this is not portable. 
    asioOutputInfo.size = sizeof(PaAsioStreamInfo);
    asioOutputInfo.hostApiType = paASIO;
    asioOutputInfo.version = 1;
    asioOutputInfo.flags = paAsioUseChannelSelectors;
    //outputChannelSelectors[0] = 0; // ASIO device channel 1 (left)
    //outputChannelSelectors[1] = 1; // ASIO device channel 2 (right)
    asioOutputInfo.channelSelectors = outputChannelSelectors;
    outputParameters.hostApiSpecificStreamInfo = &asioOutputInfo;

    err = Pa_OpenStream(
              &stream,
              NULL, // no input 
              &outputParameters,
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      // we won't output out of range samples so don't bother clipping them
              patestCallback,
              &data );
    if( err != paNoError ) goto error;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;
    
    printf("Play for %d seconds.\n", NUM_SECONDS ); fflush(stdout);
    Pa_Sleep( NUM_SECONDS * 1000 );

    err = Pa_StopStream( stream );
    if( err != paNoError ) goto error;
    
    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto error;
    
    Pa_Terminate();
    printf("Test finished.\n");
    return err;
error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return err;
}
