#include "macos_audio.h"

#include <CoreMIDI/CoreMIDI.h>

OSStatus MacOSAudioUnitCallback(void* inRefCon,
	AudioUnitRenderActionFlags* ioActionFlags,
	const AudioTimeStamp* inTimeStamp,
	UInt32 inBusNumber, UInt32 inNumberFrames,
	AudioBufferList* ioData)
{
	#pragma unused(ioActionFlags)
	#pragma unused(inTimeStamp)
	#pragma unused(inBusNumber)

	MacOSAudio* audio = ((MacOSAudio*)inRefCon);

	int sampleCount = inNumberFrames;
	// TODO handle write cursor before read cursor wrap more efficiently
	int newSamples = audio->writeCursor - audio->readCursor;
	if (audio->writeCursor < audio->readCursor) {
		newSamples = audio->bufferSizeSamples - audio->readCursor
			+ audio->writeCursor;
	}
	if (newSamples < inNumberFrames) {
		sampleCount = newSamples;
	}
	//LOG_INFO("new samples: %d\n", sampleCount);

	int16* outBufferL = (int16*)ioData->mBuffers[0].mData;
	int16* outBufferR = (int16*)ioData->mBuffers[1].mData;
	for (int i = 0; i < sampleCount; i++) {
		int sample = (audio->readCursor + i) % audio->bufferSizeSamples;
		outBufferL[i] = audio->buffer[sample * audio->channels];
		outBufferR[i] = audio->buffer[sample * audio->channels + 1];
	}

	audio->readCursor = (audio->readCursor + sampleCount)
		% audio->bufferSizeSamples;

	for (int i = sampleCount; i < inNumberFrames; i++) {
		outBufferL[i] = 0;
		outBufferR[i] = 0;
	}

	return noErr;
}

void ProcessMidiPacket(const MIDIPacket* packet, MidiInput* midiIn)
{
	if (midiIn->numMessages >= MIDI_IN_QUEUE_SIZE) {
		return;
	}
	if (packet->length < 2) {
		return;
	}
	MidiMessage msg;
	msg.status = packet->data[0];
	msg.dataByte1 = packet->data[1];
	if (packet->length >= 3) {
		msg.dataByte2 = packet->data[2];
	}

	midiIn->messages[midiIn->numMessages] = msg;
	midiIn->numMessages++;
}

void MidiInputCallback(const MIDIPacketList* packetList,
	void* readProcRefContext, void* srcConnRefContext)
{
	MacOSAudio* macAudio = (MacOSAudio*)readProcRefContext;
	while (macAudio->midiInBusy) {
		usleep(0);
	}
	macAudio->midiInBusy = true;
	for (int i = 0; i < packetList->numPackets; i++) {
		const MIDIPacket* packet = &packetList->packet[i];
		ProcessMidiPacket(packet, &macAudio->midiIn);
	}
	macAudio->midiInBusy = false;
}

void MacOSInitCoreAudio(MacOSAudio* macAudio,
	int sampleRate, int channels, int bufferSizeSamples)
{
	macAudio->sampleRate = sampleRate;
	macAudio->channels = channels;
	macAudio->bufferSizeSamples = bufferSizeSamples;
	macAudio->buffer = (int16*)mmap(0,
		bufferSizeSamples * channels * sizeof(int16),
		PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	macAudio->readCursor = 0;
	macAudio->writeCursor = 0;

	AudioComponentDescription acd;
	acd.componentType         = kAudioUnitType_Output;
	acd.componentSubType      = kAudioUnitSubType_DefaultOutput;
	acd.componentManufacturer = kAudioUnitManufacturer_Apple;

	AudioComponent outputComponent = AudioComponentFindNext(NULL, &acd);

	AudioComponentInstanceNew(outputComponent, &macAudio->audioUnit);
	AudioUnitInitialize(macAudio->audioUnit);

#if 1 // int16
	//AudioStreamBasicDescription asbd;
	macAudio->audioDescriptor.mSampleRate       = sampleRate;
	macAudio->audioDescriptor.mFormatID         = kAudioFormatLinearPCM;
	macAudio->audioDescriptor.mFormatFlags      =
		kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsNonInterleaved
		| kAudioFormatFlagIsPacked; // TODO request float format
	macAudio->audioDescriptor.mFramesPerPacket  = 1;
	macAudio->audioDescriptor.mChannelsPerFrame = channels;
	macAudio->audioDescriptor.mBitsPerChannel   = sizeof(int16) * 8;
	// don't multiply by channel count with non-interleaved!
	macAudio->audioDescriptor.mBytesPerFrame    = sizeof(int16);
	macAudio->audioDescriptor.mBytesPerPacket   =
		macAudio->audioDescriptor.mFramesPerPacket
		* macAudio->audioDescriptor.mBytesPerFrame;
#else // floating point - this is the "native" format on the Mac
	AudioStreamBasicDescription asbd;
	SoundOutput->AudioDescriptor.mSampleRate       = SoundOutput->SamplesPerSecond;
	SoundOutput->AudioDescriptor.mFormatID         = kAudioFormatLinearPCM;
	SoundOutput->AudioDescriptor.mFormatFlags      = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
	SoundOutput->AudioDescriptor.mFramesPerPacket  = 1;
	SoundOutput->AudioDescriptor.mChannelsPerFrame = 2;
	SoundOutput->AudioDescriptor.mBitsPerChannel   = sizeof(Float32) * 8; // 1 * sizeof(Float32) * 8;
	SoundOutput->AudioDescriptor.mBytesPerFrame    = sizeof(Float32);
	SoundOutput->AudioDescriptor.mBytesPerPacket   = SoundOutput->AudioDescriptor.mFramesPerPacket * SoundOutput->AudioDescriptor.mBytesPerFrame;
#endif


	// TODO Add some error checking...
	// But this is a mess. OSStatus is what? what is success? sigh
	AudioUnitSetProperty(macAudio->audioUnit,
		kAudioUnitProperty_StreamFormat,
		kAudioUnitScope_Input,
		0,
		&macAudio->audioDescriptor,
		sizeof(macAudio->audioDescriptor)
	);

	AURenderCallbackStruct cb;
	cb.inputProc = MacOSAudioUnitCallback;
	cb.inputProcRefCon = macAudio;

	AudioUnitSetProperty(macAudio->audioUnit,
		kAudioUnitProperty_SetRenderCallback,
		kAudioUnitScope_Global,
		0,
		&cb,
		sizeof(cb)
	);

	AudioOutputUnitStart(macAudio->audioUnit);

	// Setup MIDI input
	macAudio->midiInBusy = false;
	OSStatus res;
	// TODO again, error checking. but WTF IS THE SUCCESS VALUE?!
	MIDIClientRef midiClient;
	res = MIDIClientCreate(CFSTR("315MIDI_client"), NULL, NULL, &midiClient);
	MIDIPortRef midiInputPort;
	res = MIDIInputPortCreate(midiClient, CFSTR("315MIDI_input"),
		MidiInputCallback, (void*)macAudio, &midiInputPort);

	int midiSources = MIDIGetNumberOfSources();
	LOG_INFO("midi sources: %d\n", midiSources);
	MIDIEndpointRef midiSrc;
	for (int i = 0; i < midiSources; i++) {
		midiSrc = MIDIGetSource(i);
	}
	midiSrc = MIDIGetSource(midiSources - 1);
	res = MIDIPortConnectSource(midiInputPort, midiSrc, NULL);
}


void MacOSStopCoreAudio(MacOSAudio* macAudio)
{
	AudioOutputUnitStop(macAudio->audioUnit);
	AudioUnitUninitialize(macAudio->audioUnit);
	AudioComponentInstanceDispose(macAudio->audioUnit);
}

void MacOSWriteSamples(MacOSAudio* macAudio,
	const GameAudio* gameAudio, int numSamples)
{
	for (int i = 0; i < numSamples; i++) {
		int ind = (macAudio->writeCursor + i) % macAudio->bufferSizeSamples;
		macAudio->buffer[ind * macAudio->channels] = (int16)(INT16_MAXVAL *
			gameAudio->buffer[i * gameAudio->channels]);
		macAudio->buffer[ind * macAudio->channels + 1] = (int16)(INT16_MAXVAL *
			gameAudio->buffer[i * gameAudio->channels + 1]);
	}

	macAudio->writeCursor = (macAudio->writeCursor + numSamples)
		% macAudio->bufferSizeSamples;
}
