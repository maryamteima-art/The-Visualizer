#pragma once

#include "ofConstants.h"

#ifdef OF_SOUND_PLAYER_FMOD

#include "ofSoundBaseTypes.h"


extern "C" {
#include "fmod.h"
#include "fmod_errors.h"
}

//		TO DO :
//		---------------------------
// 		-fft via fmod, as in the last time...
// 		-close fmod if it's up
//		-loadSoundForStreaming(char * fileName);
//		---------------------------

// 		interesting:
//		http://www.compuphase.com/mp3/mp3loops.htm


// ---------------------------------------------------------------------------- SOUND SYSTEM FMOD

// --------------------- global functions:
void ofFmodSoundStopAll_MOD();
void ofFmodSoundSetVolume_MOD(float vol);
void ofFmodSoundUpdate_MOD();						// calls FMOD update.
float * ofFmodSoundGetSpectrum_MOD(int nBands);		// max 512...
float * ofFmodSoundGetWaveData_MOD(int nBands, int channel);
void ofFmodSetBuffersize_MOD(unsigned int bs);
void ERRCHECK(FMOD_RESULT result, char * extraMessage = "");

// --------------------- player functions:
class ofFmodSoundPlayer_MOD : public ofBaseSoundPlayer {

public:

	ofFmodSoundPlayer_MOD();
	virtual ~ofFmodSoundPlayer_MOD();

	bool load(const std::filesystem::path& fileName, bool stream = false);
	void unload();
	void play();
	void stop();

	void setVolume(float vol);
	void setPan(float vol);
	void setSpeed(float spd);
	void setPaused(bool bP);
	void setLoop(bool bLp);
	void setMultiPlay(bool bMp);
	void setPosition(float pct); // 0 = start, 1 = end;
	void setPositionMS(int ms);

	float getPosition() const;
	int getPositionMS() const;
	bool isPlaying() const;
	float getSpeed() const;
	float getPan() const;
	float getVolume() const;
	bool isLoaded() const;

	static void initializeFmod();
	static void closeFmod();


	bool isStreaming;
	bool bMultiPlay;
	bool bLoop;
	bool bLoadedOk;
	bool bPaused;
	float pan; // -1 to 1
	float volume; // 0 - 1
	float internalFreq; // 44100 ?
	float speed; // -n to n, 1 = normal, -1 backwards
	unsigned int length; // in samples;

	FMOD_RESULT result;
	FMOD_CHANNEL * channel = NULL;
	FMOD_SOUND * sound = NULL;
};

#endif //OF_SOUND_PLAYER_FMOD
