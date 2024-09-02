#include "ofFmodSoundPlayer_MOD.h"
#ifdef OF_SOUND_PLAYER_FMOD

#include "ofUtils.h"
#include "ofMath.h"
#include "ofLog.h"


//Waveform stuff (need custom DSP) ....

typedef struct
{
	float *buffer;
	float volume_linear;
	int   length_samples;
	int   channels;
} mydsp_data_t;

FMOD_RESULT F_CALLBACK myDSPCallback(FMOD_DSP_STATE *dsp_state, float *inbuffer, float *outbuffer, unsigned int length, int inchannels, int *outchannels)
{
	mydsp_data_t *data = (mydsp_data_t *)dsp_state->plugindata;

	/*
		This loop assumes inchannels = outchannels, which it will be if the DSP is created with '0'
		as the number of channels in FMOD_DSP_DESCRIPTION.
		Specifying an actual channel count will mean you have to take care of any number of channels coming in,
		but outputting the number of channels specified. Generally it is best to keep the channel
		count at 0 for maximum compatibility.
	*/
for (unsigned int samp = 0; samp < length; samp++)
{
	/*
		Feel free to unroll this.
	*/
	for (int chan = 0; chan < *outchannels; chan++)
	{
		/*
			This DSP filter just halves the volume!
			Input is modified, and sent to output.
		*/
		data->buffer[(samp * *outchannels) + chan] = outbuffer[(samp * inchannels) + chan] = inbuffer[(samp * inchannels) + chan] * data->volume_linear;
	}
}

data->channels = inchannels;

return FMOD_OK;
}

/*
	Callback called when DSP is created.   This implementation creates a structure which is attached to the dsp state's 'plugindata' member.
*/
FMOD_RESULT F_CALLBACK myDSPCreateCallback(FMOD_DSP_STATE *dsp_state)
{
	unsigned int blocksize;
	FMOD_RESULT result;

	result = dsp_state->functions->getblocksize(dsp_state, &blocksize);
	ERRCHECK(result, "myDSPCreateCallback");

	mydsp_data_t *data = (mydsp_data_t *)calloc(sizeof(mydsp_data_t), 1);
	if (!data)
	{
		return FMOD_ERR_MEMORY;
	}
	dsp_state->plugindata = data;
	data->volume_linear = 1.0f;
	data->length_samples = blocksize;

	data->buffer = (float *)malloc(blocksize * 8 * sizeof(float));      // *8 = maximum size allowing room for 7.1.   Could ask dsp_state->functions->getspeakermode for the right speakermode to get real speaker count.
	if (!data->buffer)
	{
		return FMOD_ERR_MEMORY;
	}

	return FMOD_OK;
}

/*
	Callback called when DSP is destroyed.   The memory allocated in the create callback can be freed here.
*/
FMOD_RESULT F_CALLBACK myDSPReleaseCallback(FMOD_DSP_STATE *dsp_state)
{
	if (dsp_state->plugindata)
	{
		mydsp_data_t *data = (mydsp_data_t *)dsp_state->plugindata;

		if (data->buffer)
		{
			free(data->buffer);
		}

		free(data);
	}

	return FMOD_OK;
}

/*
	Callback called when DSP::getParameterData is called.   This returns a pointer to the raw floating point PCM data.
	We have set up 'parameter 0' to be the data parameter, so it checks to make sure the passed in index is 0, and nothing else.
*/
FMOD_RESULT F_CALLBACK myDSPGetParameterDataCallback(FMOD_DSP_STATE *dsp_state, int index, void **data, unsigned int *length, char *)
{
	if (index == 0)
	{
		unsigned int blocksize;
		FMOD_RESULT result;
		mydsp_data_t *mydata = (mydsp_data_t *)dsp_state->plugindata;

		result = dsp_state->functions->getblocksize(dsp_state, &blocksize);
		ERRCHECK(result);

		*data = (void *)mydata;
		*length = blocksize * 2 * sizeof(float);

		return FMOD_OK;
	}

	return FMOD_ERR_INVALID_PARAM;
}

/*
	Callback called when DSP::setParameterFloat is called.   This accepts a floating point 0 to 1 volume value, and stores it.
	We have set up 'parameter 1' to be the volume parameter, so it checks to make sure the passed in index is 1, and nothing else.
*/
FMOD_RESULT F_CALLBACK myDSPSetParameterFloatCallback(FMOD_DSP_STATE *dsp_state, int index, float value)
{
	if (index == 1)
	{
		mydsp_data_t *mydata = (mydsp_data_t *)dsp_state->plugindata;

		mydata->volume_linear = value;

		return FMOD_OK;
	}

	return FMOD_ERR_INVALID_PARAM;
}

/*
	Callback called when DSP::getParameterFloat is called.   This returns a floating point 0 to 1 volume value.
	We have set up 'parameter 1' to be the volume parameter, so it checks to make sure the passed in index is 1, and nothing else.
	An alternate way of displaying the data is provided, as a string, so the main app can use it.
*/
FMOD_RESULT F_CALLBACK myDSPGetParameterFloatCallback(FMOD_DSP_STATE *dsp_state, int index, float *value, char *valstr)
{
	if (index == 1)
	{
		mydsp_data_t *mydata = (mydsp_data_t *)dsp_state->plugindata;

		*value = mydata->volume_linear;
		if (valstr)
		{
			sprintf(valstr, "%d", (int)((*value * 100.0f) + 0.5f));
		}

		return FMOD_OK;
	}

	return FMOD_ERR_INVALID_PARAM;
}

//end custom dsp stuff


static bool bFmodInitialized_ = false;
static float fftInterpValues_[8192];			//
static float fftSpectrum_[8192];		// maximum #ofFmodSoundPlayer is 8192, in fmod....
static unsigned int buffersize = 1024;
static FMOD_DSP* fftDSP = NULL;
static FMOD_DSP* waveformDSP = NULL;

// ---------------------  static vars
static FMOD_CHANNELGROUP * channelgroup = NULL;
static FMOD_SYSTEM       * sys = NULL;

// these are global functions, that affect every sound / channel:
// ------------------------------------------------------------
// ------------------------------------------------------------

//--------------------
void ofFmodSoundStopAll_MOD() {
	ofFmodSoundPlayer_MOD::initializeFmod();
	FMOD_ChannelGroup_Stop(channelgroup);
}

//--------------------
void ofFmodSoundSetVolume_MOD(float vol) {
	ofFmodSoundPlayer_MOD::initializeFmod();
	FMOD_ChannelGroup_SetVolume(channelgroup, vol);
}

//--------------------
void ofFmodSoundUpdate_MOD() {
	if (bFmodInitialized_) {
		FMOD_System_Update(sys);
	}
}

float * ofFmodSoundGetWaveData_MOD(int nBands, int channel) {

	ofFmodSoundPlayer_MOD::initializeFmod();
	FMOD_RESULT result;


	// 	set to 0
	for (int i = 0; i < 8192; i++) {
		fftInterpValues_[i] = 0;
		fftSpectrum_[i] = 0;
	}

	// 	check what the user wants vs. what we can do:
	if (nBands > 8192) {
		ofLogWarning("ofFmodSoundPlayer") << "ofFmodGetSpectrum(): requested number of bands " << nBands << ", using maximum of 8192";
		nBands = 8192;
	}
	else if (nBands <= 0) {
		ofLogWarning("ofFmodSoundPlayer") << "ofFmodGetSpectrum(): requested number of bands " << nBands << ", using minimum of 1";
		nBands = 1;
		return fftInterpValues_;
	}

	//  get the fft
	//  useful info here: https://www.parallelcube.com/2018/03/10/frequency-spectrum-using-fmod-and-ue4/
	if (waveformDSP == NULL) {
		/*
		Create the DSP effect.
	*/
		{
			FMOD_DSP_DESCRIPTION dspdesc;
			memset(&dspdesc, 0, sizeof(dspdesc));
			FMOD_DSP_PARAMETER_DESC wavedata_desc;
			FMOD_DSP_PARAMETER_DESC volume_desc;
			FMOD_DSP_PARAMETER_DESC *paramdesc[2] =
			{
				&wavedata_desc,
				&volume_desc
			};

			FMOD_DSP_INIT_PARAMDESC_DATA(wavedata_desc, "wave data", "", "wave data", FMOD_DSP_PARAMETER_DATA_TYPE_USER);
			FMOD_DSP_INIT_PARAMDESC_FLOAT(volume_desc, "volume", "%", "linear volume in percent", 0, 1, 1);

			strncpy(dspdesc.name, "My first DSP unit", sizeof(dspdesc.name));
			dspdesc.version = 0x00010000;
			dspdesc.numinputbuffers = 1;
			dspdesc.numoutputbuffers = 1;
			dspdesc.read = myDSPCallback;
			dspdesc.create = myDSPCreateCallback;
			dspdesc.release = myDSPReleaseCallback;
			dspdesc.getparameterdata = myDSPGetParameterDataCallback;
			dspdesc.setparameterfloat = myDSPSetParameterFloatCallback;
			dspdesc.getparameterfloat = myDSPGetParameterFloatCallback;
			dspdesc.numparameters = 2;
			dspdesc.paramdesc = paramdesc;

			result = FMOD_System_CreateDSP(sys, &dspdesc, &waveformDSP);
			ERRCHECK(result, "created DSP");
		}

		result = FMOD_ChannelGroup_AddDSP(channelgroup, 0, waveformDSP);
		ERRCHECK(result, "added dsp to channel group");
	}

	if (waveformDSP != NULL) {
		char                     volstr[32] = { 0 };
		FMOD_DSP_PARAMETER_DESC *desc;
		mydsp_data_t            *data;

		result = FMOD_DSP_GetParameterInfo(waveformDSP, 1, &desc);
		ERRCHECK(result, "FMOD_DSP_GetParameterInfo");
		result = FMOD_DSP_GetParameterFloat(waveformDSP, 1, 0, volstr, 32);
		ERRCHECK(result, "FMOD_DSP_GetParameterFloat");
		result = FMOD_DSP_GetParameterData(waveformDSP, 0, (void **)&data, 0, 0, 0);
		ERRCHECK(result, "FMOD_DSP_GetParameterData");

		if (result == FMOD_OK) {
			for (int i = channel, j = 0; i < data->length_samples * data->channels; i+=data->channels, j++) {
				fftInterpValues_[j] = data->buffer[i];
			}
		}
	}

	return fftInterpValues_;
}

//--------------------
float * ofFmodSoundGetSpectrum_MOD(int nBands) {

	ofFmodSoundPlayer_MOD::initializeFmod();


	// 	set to 0
	for (int i = 0; i < 8192; i++) {
		fftInterpValues_[i] = 0;
		fftSpectrum_[i] = 0;
	}

	// 	check what the user wants vs. what we can do:
	if (nBands > 8192) {
		ofLogWarning("ofFmodSoundPlayer") << "ofFmodGetSpectrum(): requested number of bands " << nBands << ", using maximum of 8192";
		nBands = 8192;
	}
	else if (nBands <= 0) {
		ofLogWarning("ofFmodSoundPlayer") << "ofFmodGetSpectrum(): requested number of bands " << nBands << ", using minimum of 1";
		nBands = 1;
		return fftInterpValues_;
	}

	//  get the fft
	//  useful info here: https://www.parallelcube.com/2018/03/10/frequency-spectrum-using-fmod-and-ue4/
	if (fftDSP == NULL) {
		FMOD_System_CreateDSPByType(sys, FMOD_DSP_TYPE_FFT, &fftDSP);
		FMOD_ChannelGroup_AddDSP(channelgroup, 0, fftDSP);
		FMOD_DSP_SetParameterInt(fftDSP, FMOD_DSP_FFT_WINDOWTYPE, FMOD_DSP_FFT_WINDOW_HANNING);
	}

	if (fftDSP != NULL) {
		FMOD_DSP_PARAMETER_FFT *fft;
		auto result = FMOD_DSP_GetParameterData(fftDSP, FMOD_DSP_FFT_SPECTRUMDATA, (void **)&fft, 0, 0, 0);
		if (result == 0) {

			// Only read / display half of the buffer typically for analysis
			// as the 2nd half is usually the same data reversed due to the nature of the way FFT works. ( comment from link above )
			int length = fft->length / 2;
			if (length > 0) {

				std::vector <float> avgValCount;
				avgValCount.assign(nBands, 0.0);

				float normalizedBand = 0;
				float normStep = 1.0 / (float)length;

				for (int bin = 0; bin < length; bin++) {
					//should map 0 to nBands but accounting for lower frequency bands being more important
					int logIndexBand = log10(1.0 + normalizedBand * 9.0) * nBands;

					//get both channels as that is what the old FMOD call did
					for (int channel = 0; channel < fft->numchannels; channel++) {
						fftSpectrum_[logIndexBand] += fft->spectrum[channel][bin];
						avgValCount[logIndexBand] += 1.0;
					}

					normalizedBand += normStep;
				}

				//average the remapped bands based on how many times we added to each bin
				for (int i = 0; i < nBands; i++) {
					if (avgValCount[i] > 1.0) {
						fftSpectrum_[i] /= avgValCount[i];
					}
				}
			}
		}
	}

	// 	convert to db scale
	for (int i = 0; i < nBands; i++) {
		fftInterpValues_[i] = 10.0f * (float)log10(1 + fftSpectrum_[i]) * 2.0f;
	}

	return fftInterpValues_;
}

void ofFmodSetBuffersize_MOD(unsigned int bs) {
	buffersize = bs;
}

void ERRCHECK(FMOD_RESULT result, char * extraMessage) {
	//commenting out for now (lazy)
	//std::cout << FMOD_ErrorString(result) << " " << extraMessage << std::endl;
}

// ------------------------------------------------------------
// ------------------------------------------------------------


// now, the individual sound player:
//------------------------------------------------------------
ofFmodSoundPlayer_MOD::ofFmodSoundPlayer_MOD() {
	bLoop = false;
	bLoadedOk = false;
	pan = 0.0; // range for oF is -1 to 1
	volume = 1.0f;
	internalFreq = 44100;
	speed = 1;
	bPaused = false;
	isStreaming = false;
}

ofFmodSoundPlayer_MOD::~ofFmodSoundPlayer_MOD() {
	unload();
}



//---------------------------------------
// this should only be called once
void ofFmodSoundPlayer_MOD::initializeFmod() {
	if (!bFmodInitialized_) {

		FMOD_System_Create(&sys);

		// set buffersize, keep number of buffers
		unsigned int bsTmp;
		int nbTmp;
		FMOD_System_GetDSPBufferSize(sys, &bsTmp, &nbTmp);
		FMOD_System_SetDSPBufferSize(sys, buffersize, nbTmp);

#ifdef TARGET_LINUX
		FMOD_System_SetOutput(sys, FMOD_OUTPUTTYPE_ALSA);
#endif
		FMOD_System_Init(sys, 512, FMOD_INIT_NORMAL, nullptr);  //do we want just 512 channels?
		FMOD_System_GetMasterChannelGroup(sys, &channelgroup);
		bFmodInitialized_ = true;
	}
}




//---------------------------------------
void ofFmodSoundPlayer_MOD::closeFmod() {
	if (bFmodInitialized_) {
		FMOD_System_Close(sys);
		bFmodInitialized_ = false;
	}
}

//------------------------------------------------------------
bool ofFmodSoundPlayer_MOD::load(const std::filesystem::path& _fileName, bool stream) {

	auto fileName = ofToDataPath(_fileName);

	// fmod uses IO posix internally, might have trouble
	// with unicode paths...
	// says this code:
	// http://66.102.9.104/search?q=cache:LM47mq8hytwJ:www.cleeker.com/doxygen/audioengine__fmod_8cpp-source.html+FSOUND_Sample_Load+cpp&hl=en&ct=clnk&cd=18&client=firefox-a
	// for now we use FMODs way, but we could switch if
	// there are problems:

	bMultiPlay = false;

	// [1] init fmod, if necessary

	initializeFmod();

	// [2] try to unload any previously loaded sounds
	// & prevent user-created memory leaks
	// if they call "loadSound" repeatedly, for example

	unload();

	// [3] load sound

	//choose if we want streaming
	int fmodFlags = FMOD_DEFAULT;
	if (stream)fmodFlags = FMOD_DEFAULT | FMOD_CREATESTREAM;

	result = FMOD_System_CreateSound(sys, fileName.data(), fmodFlags, nullptr, &sound);

	if (result != FMOD_OK) {
		bLoadedOk = false;
		ofLogError("ofFmodSoundPlayer") << "loadSound(): could not load \"" << fileName << "\"";
	}
	else {
		bLoadedOk = true;
		FMOD_Sound_GetLength(sound, &length, FMOD_TIMEUNIT_PCM);
		isStreaming = stream;
	}

	return bLoadedOk;
}

//------------------------------------------------------------
void ofFmodSoundPlayer_MOD::unload() {
	if (bLoadedOk) {
		stop();						// try to stop the sound
		FMOD_Sound_Release(sound);
		FMOD_DSP_Release(fftDSP);
		FMOD_DSP_Release(waveformDSP);
		FMOD_System_Release(sys);
		bLoadedOk = false;
	}
}

//------------------------------------------------------------
bool ofFmodSoundPlayer_MOD::isPlaying() const {

	if (!bLoadedOk) return false;
	if (channel == NULL) return false;
	int playing = 0;
	FMOD_Channel_IsPlaying(channel, &playing);
	return (playing != 0 ? true : false);
}

//------------------------------------------------------------
float ofFmodSoundPlayer_MOD::getSpeed() const {
	return speed;
}

//------------------------------------------------------------
float ofFmodSoundPlayer_MOD::getPan() const {
	return pan;
}

//------------------------------------------------------------
float ofFmodSoundPlayer_MOD::getVolume() const {
	return volume;
}

//------------------------------------------------------------
bool ofFmodSoundPlayer_MOD::isLoaded() const {
	return bLoadedOk;
}

//------------------------------------------------------------
void ofFmodSoundPlayer_MOD::setVolume(float vol) {
	if (isPlaying()) {
		FMOD_Channel_SetVolume(channel, vol);
	}
	volume = vol;
}

//------------------------------------------------------------
void ofFmodSoundPlayer_MOD::setPosition(float pct) {
	if (isPlaying()) {
		int sampleToBeAt = (int)(length * pct);
		FMOD_Channel_SetPosition(channel, sampleToBeAt, FMOD_TIMEUNIT_PCM);
	}
}

void ofFmodSoundPlayer_MOD::setPositionMS(int ms) {
	if (isPlaying()) {
		FMOD_Channel_SetPosition(channel, ms, FMOD_TIMEUNIT_MS);
	}
}

//------------------------------------------------------------
float ofFmodSoundPlayer_MOD::getPosition() const {
	if (isPlaying()) {
		unsigned int sampleImAt;

		FMOD_Channel_GetPosition(channel, &sampleImAt, FMOD_TIMEUNIT_PCM);

		float pct = 0.0f;
		if (length > 0) {
			pct = sampleImAt / (float)length;
		}
		return pct;
	}
	else {
		return 0;
	}
}

//------------------------------------------------------------
int ofFmodSoundPlayer_MOD::getPositionMS() const {
	if (isPlaying()) {
		unsigned int sampleImAt;

		FMOD_Channel_GetPosition(channel, &sampleImAt, FMOD_TIMEUNIT_MS);

		return sampleImAt;
	}
	else {
		return 0;
	}
}

//------------------------------------------------------------
void ofFmodSoundPlayer_MOD::setPan(float p) {
	pan = p;
	p = ofClamp(p, -1, 1);
	if (isPlaying()) {
		FMOD_Channel_SetPan(channel, p);
	}
}


//------------------------------------------------------------
void ofFmodSoundPlayer_MOD::setPaused(bool bP) {
	if (isPlaying()) {
		FMOD_Channel_SetPaused(channel, bP);
		bPaused = bP;
	}
}


//------------------------------------------------------------
void ofFmodSoundPlayer_MOD::setSpeed(float spd) {
	if (isPlaying()) {
		FMOD_Channel_SetFrequency(channel, internalFreq * spd);
	}
	speed = spd;
}


//------------------------------------------------------------
void ofFmodSoundPlayer_MOD::setLoop(bool bLp) {
	if (isPlaying()) {
		FMOD_Channel_SetMode(channel, (bLp == true) ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
	}
	bLoop = bLp;
}

// ----------------------------------------------------------------------------
void ofFmodSoundPlayer_MOD::setMultiPlay(bool bMp) {
	bMultiPlay = bMp;		// be careful with this...
}

// ----------------------------------------------------------------------------
void ofFmodSoundPlayer_MOD::play() {

	// if it's a looping sound, we should try to kill it, no?
	// or else people will have orphan channels that are looping
	if (bLoop == true) {
		FMOD_Channel_Stop(channel);
	}

	// if the sound is not set to multiplay, then stop the current,
	// before we start another
	if (!bMultiPlay) {
		FMOD_Channel_Stop(channel);
	}

	FMOD_System_PlaySound(sys, sound, channelgroup, bPaused, &channel);

	FMOD_Channel_GetFrequency(channel, &internalFreq);
	FMOD_Channel_SetVolume(channel, volume);
	setPan(pan);
	FMOD_Channel_SetFrequency(channel, internalFreq * speed);
	FMOD_Channel_SetMode(channel, (bLoop == true) ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);

	//fmod update() should be called every frame - according to the docs.
	//we have been using fmod without calling it at all which resulted in channels not being able
	//to be reused.  we should have some sort of global update function but putting it here
	//solves the channel bug
	FMOD_System_Update(sys);

}

// ----------------------------------------------------------------------------
void ofFmodSoundPlayer_MOD::stop() {
	FMOD_Channel_Stop(channel);
}

#endif //OF_SOUND_PLAYER_FMOD
