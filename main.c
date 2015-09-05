#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <portaudio.h>
#include <curl/curl.h>

#define SAMPLE_RATE  (44100)
#define FRAMES_PER_BUFFER (1024)
#define NUM_SECONDS     (1)
#define NUM_CHANNELS    (2)
#define DITHER_FLAG     (0)

/*** Sample format ***/
#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"

SAMPLE *recordedSamples;


int main(void);
void writeToFile(int totalFrames);
int postcall(int idInput, int a, int b);



int main(void)
{
    PaStreamParameters inputParameters, outputParameters;
    PaStream *stream;
    PaError err;
    
    int i;
    int totalFrames;
    int numSamples;
    int numBytes;
    SAMPLE max, average, val, level;

    totalFrames = NUM_SECONDS * SAMPLE_RATE;
    numSamples = totalFrames * NUM_CHANNELS;

    numBytes = numSamples * sizeof(SAMPLE);
    recordedSamples = (SAMPLE *) malloc( numBytes );
    if( recordedSamples == NULL )
    {
        printf("Could not allocate record array.\n");
        exit(1);
    }
    for( i=0; i<numSamples; i++ ) recordedSamples[i] = 0;

    err = Pa_Initialize();
    if( err != paNoError ) goto error;

    /*** Select input device ***/
    inputParameters.device = Pa_GetDefaultInputDevice();
    if (inputParameters.device == paNoDevice) {
      fprintf(stderr,"Error: No default input device.\n");
      goto error;
    }
    inputParameters.channelCount = NUM_CHANNELS;
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

  while(1){
    /*** Audio record ***/
    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              NULL,                
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,     
              NULL, 
              NULL );
    if( err != paNoError ) goto error;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;
    printf("Recording.\n"); 
    fflush(stdout);

    err = Pa_ReadStream( stream, recordedSamples, totalFrames );
    if( err != paNoError ) goto error;
    
    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto error;

    /*** Measure maximum peak amplitude ***/
    max = 0;
    average = 0;
    for( i=0; i<numSamples; i++ )
    {
        val = recordedSamples[i];
        if( val < 0 ) val = -val; /* ABS */
        if( val > max )
        {
            max = val;
        }
        average += val;
    }

    average = average / numSamples;

    printf("Sample max amplitude = "PRINTF_S_FORMAT"\n", max );
    printf("Sample average = "PRINTF_S_FORMAT"\n", average );


    /*** Playback recorded data ***/
    // use it to test recorded data

    outputParameters.device = Pa_GetDefaultOutputDevice(); 
    if (outputParameters.device == paNoDevice) {
      fprintf(stderr,"Error: No default output device.\n");
      goto error;
    }
    outputParameters.channelCount = NUM_CHANNELS;
    outputParameters.sampleFormat =  PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    printf("Begin playback.\n"); fflush(stdout);
    err = Pa_OpenStream(
              &stream,
              NULL, 
              &outputParameters,
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      
              NULL, 
              NULL ); 
    if( err != paNoError ) goto error;

    if( stream )
    {
        err = Pa_StartStream( stream );
        if( err != paNoError ) goto error;
        printf("Waiting for playback to finish.\n"); fflush(stdout);

        err = Pa_WriteStream( stream, recordedSamples, totalFrames );
        if( err != paNoError ) goto error;

        err = Pa_CloseStream( stream );
        if( err != paNoError ) goto error;
        printf("Done.\n"); fflush(stdout);
    }
    /*** Playback end  ***/



    level = 0.2;     // set level ratio for writing and analysis

    if (max >= level)		// also you can use `average` value instead of `max`
    {
       writeToFile(totalFrames);
       //
       //   sample analysis call
       //
       postcall(3, 100, 200);   //  data that should be sent to server. 3 as a device ID. values 100 and 200 for example only.
    }
  }

    free( recordedSamples );

    Pa_Terminate();
    return 0;

error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return -1;
}

void writeToFile(int totalFrames)
{
        FILE  *fid;
        fid = fopen("sample.wav", "wb");
        if( fid == NULL )
        {
            printf("Could not open file.");
        }
        else
        {
            fwrite( recordedSamples, NUM_CHANNELS * sizeof(SAMPLE), totalFrames, fid );
            fclose( fid );
            printf("Wrote data to 'sample.wav'\n");
        }
}


int postcall(int idInput, int a, int b)
{
	int exitStatus = 0;
	const char* Url = "http:// 52.10.87.246:3000/pie/create"; 
	// you can use http://jsonplaceholder.typicode.com/posts for query testing

	// Parameters 
	char* id[3];
	char* firstAngle[3];
	char* secondAngle[3];
	sprintf(id, "%d", idInput);
	sprintf(firstAngle, "%d", a);
	sprintf(secondAngle, "%d", b);
	
	int postDataLen = strlen(Url) + strlen(id) + strlen(firstAngle) + strlen(secondAngle);
	char* postData = (char*)malloc(sizeof(char) * (postDataLen));
	if(postData == NULL) {
		printf("Error: unable to allocate memory for POST data\n");
	}
	strcpy(postData, "id=");
	strncat(postData, id, strlen(id));
	strncat(postData, "&firstAngle=", 12);
	strncat(postData, firstAngle, strlen(firstAngle));
        strncat(postData, "&secondAngle=", 13);
	strncat(postData, secondAngle, strlen(secondAngle));

	//  Initialisation 
	curl_global_init(CURL_GLOBAL_ALL);
	char* curlErrStr = (char*)malloc(CURL_ERROR_SIZE);
	CURL* curlHandle = curl_easy_init();
	if(curlHandle) {
		curl_easy_setopt(curlHandle, CURLOPT_ERRORBUFFER, curlErrStr);
		curl_easy_setopt(curlHandle, CURLOPT_URL, Url);
		curl_easy_setopt(curlHandle, CURLOPT_POSTFIELDS, postData);
		CURLcode curlErr = curl_easy_perform(curlHandle);
		if(curlErr) {
			printf("%s\n", curl_easy_strerror(curlErr));
		}
		curl_global_cleanup();
	}
	else { exitStatus = 1; }
	free(postData);
	return exitStatus;
}
