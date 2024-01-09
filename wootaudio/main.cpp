#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <portaudio.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <propvarutil.h>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <csignal>
#include "wooting-rgb-sdk.h"
#include "wooting-usb.h"


#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 512

static void fcheckErr(PaError err) {
	if (err != paNoError) {
		printf("Portaudio err: %s\n", Pa_GetErrorText(err));
		exit(EXIT_FAILURE);
	}
}

static float xmax(float a, float b) {
	return a > b ? a : b;
}

BOOL WINAPI resetAll(DWORD dwCtrlType) {
	switch (dwCtrlType) {
	case CTRL_CLOSE_EVENT:
		wooting_rgb_reset();
		wooting_rgb_close();
		exit(EXIT_SUCCESS);

	default:
		return FALSE;
	}
}

void closeAll() {
	wooting_rgb_reset();
	wooting_rgb_close();
}

static int rgbAudioCallback(
	const void* inputBuffer, 
	void* outputBuffer,
	unsigned long framesPerBuffer, 
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void* userData
) {
	float* inBuff = (float*)inputBuffer;
	(void)outputBuffer;
	//float* outBuff = (float*)outputBuffer;
	//(void)inputBuffer;
	int dispSize = 13;
	printf("\r");

	float volLeft = 0;
	float volRight = 0;

	for (unsigned long i = 0; i < framesPerBuffer * 2; i += 2) {
		volLeft = xmax(volLeft, std::abs(inBuff[i])); // volume is the max abs value of wave
		volRight = xmax(volRight, std::abs(inBuff[i + 1]));
		//printf("%f", volLeft);
	}

	for (int i = 0; i < dispSize - 1; i++) {
		float barProp = i / ((float)dispSize);
		// display logic
		if (barProp <= volLeft && barProp <= volRight) { // does not meet volume req
			printf("?");
			wooting_rgb_direct_set_key(2, i, 10, 189, 198); // set rgbs as last 3 values
			wooting_rgb_direct_set_key(3, i, 10, 189, 198);
			wooting_rgb_direct_set_key(4, i, 10, 189, 198);
		}
		else if (barProp <= volLeft) { // volLeft has not been reached
			printf("a");
		}
		else if (barProp <= volRight) {
			printf("b");
		}
		else {
			printf(" ");
			wooting_rgb_direct_set_key(2, i, 0, 0, 0);
			wooting_rgb_direct_set_key(3, i, 0, 0, 0);
			wooting_rgb_direct_set_key(4, i, 0, 0, 0);
		}
		
	}

	fflush(stdout);
	
	return 0;
} 

int main() {
	if (!SetConsoleCtrlHandler(resetAll, TRUE)) {
		return EXIT_FAILURE;
	}

	HMODULE hLib = LoadLibrary(L"wooting-rgb-sdk64");
	if (!hLib || hLib == INVALID_HANDLE_VALUE) {
		printf("error loading wooting lib");
		return 1;
	}

	PaError err;
	err = Pa_Initialize();
	fcheckErr(err); 

	int numDevices = Pa_GetDeviceCount();
	printf("Number of available devices: %d\n", numDevices);
	if (numDevices < 0) {
		printf("Error getting device count");
		exit(EXIT_FAILURE);
	}
	else if (numDevices == 0) {
		printf("No devices found");
		exit(EXIT_SUCCESS);
	}

	const PaDeviceInfo* deviceInfo;
	for (int i = 0; i < numDevices; i++) {
		deviceInfo = Pa_GetDeviceInfo(i);
		//deviceInfo = Pa_GetDeviceInfo(Pa_GetDefaultOutputDevice());
		printf("Device %d:\n", i);
		printf("   name: %s\n", deviceInfo->name);
		printf("   max input channels: %d\n", deviceInfo->maxInputChannels);
		printf("   max output channels: %d\n", deviceInfo->maxOutputChannels);
		printf("   type: %s\n", Pa_GetHostApiInfo(Pa_GetDeviceInfo(i)->hostApi)->type == paWASAPI ? "paWasapi" : "idk");
	}
	printf("wooting connected: %d", wooting_rgb_kbd_connected());

	
	int device = 12; // 12 -> primary sound cap driver, 
	int device2 = 16; // related output driver, may vary between systems
	int deviceIn = Pa_GetDefaultInputDevice();
	int deviceOut = Pa_GetDefaultOutputDevice();


	// debug info
	const PaDeviceInfo* deviceInfo2 = Pa_GetDeviceInfo(deviceIn);
	printf("Default Output Device Channels: %d input, %d output\n", deviceInfo2->maxInputChannels, deviceInfo2->maxOutputChannels);
	printf("   name: %s\n", deviceInfo2->name);

	// stream info setup
	PaStreamParameters inputParameters;
	PaStreamParameters outputParameters;
	memset(&inputParameters, 0, sizeof(inputParameters));
	inputParameters.channelCount = 2;
	inputParameters.device = deviceIn;
	inputParameters.hostApiSpecificStreamInfo = NULL;
	inputParameters.sampleFormat = paFloat32;
	inputParameters.suggestedLatency = Pa_GetDeviceInfo(deviceIn)->defaultLowInputLatency; // to run realtime

	memset(&outputParameters, 0, sizeof(outputParameters));
	outputParameters.channelCount = 2; // keep non zero to prevent error
	outputParameters.device = deviceOut;
	outputParameters.hostApiSpecificStreamInfo = NULL;
	outputParameters.sampleFormat = paFloat32;
	outputParameters.suggestedLatency = Pa_GetDeviceInfo(deviceOut)->defaultLowInputLatency;

	PaHostApiTypeId hostApiType = paWASAPI;

	PaStream* stream;
	err = Pa_OpenStream(
		&stream,
		&inputParameters,
		&outputParameters,
		SAMPLE_RATE,
		FRAMES_PER_BUFFER,
		paNoFlag,
		rgbAudioCallback,
		NULL
	);
	fcheckErr(err);

	err = Pa_StartStream(stream);
	fcheckErr(err);

	Pa_Sleep(20000); // how long to listen for in ms


	// end audio streams
	err = Pa_StopStream(stream);
	fcheckErr(err);

	err = Pa_CloseStream(stream);
	fcheckErr(err);

	err = Pa_Terminate();
	fcheckErr(err);

	std::atexit(closeAll); // reset rgb on exit
	;
	return EXIT_SUCCESS;
}
