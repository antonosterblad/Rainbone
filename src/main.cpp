#include "Level.h"
#include "portaudio.h"

void render();
void preSync();
void initialize();
void encode();
void decode();	
void keyCallback(int key, int action);
void cleanUp();
void initAudio();
static int audioCallback(const void *inputbuffer, void *outputbuffer,
	unsigned long framesperbuffer,
	const PaStreamCallbackTimeInfo* timeinfo,
	PaStreamCallbackFlags statusflags,
	void *userdata);
// Pointer to the sgct engine
sgct::Engine * gEngine;
// Container for the levels
std::vector<Level *> mLevels;
// Variables to share across cluster
sgct::SharedDouble curr_time(0.0);
// Track which level we want to rotate
unsigned int mLevelIndex = 0;


int main(int argc, char* argv[]) {
	PaStream *stream;
	PaError err;
	static float data;

    gEngine = new sgct::Engine(argc, argv);

    gEngine->setInitOGLFunction(initialize);
    gEngine->setDrawFunction(render);
    gEngine->setPreSyncFunction(preSync);
    gEngine->setKeyboardCallbackFunction(keyCallback);
    gEngine->setCleanUpFunction(cleanUp);

    if(!gEngine->init(sgct::Engine::OpenGL_3_3_Core_Profile)) {

        delete gEngine;
        return EXIT_FAILURE;
    }

    sgct::SharedData::instance()->setEncodeFunction(encode);
    sgct::SharedData::instance()->setDecodeFunction(decode);

	//init audio
	err = Pa_Initialize();
	if (err != paNoError) {
		std::cout << Pa_GetErrorText(err) << std::endl;
		err = Pa_Terminate();
		if (err != paNoError)
			std::cout << "error message: " << Pa_GetErrorText(err) << std::endl;
		return err;
	}

	/* open an audio i/o stream. */
	err = Pa_OpenDefaultStream(&stream,
		1,          /* one input channel */
		1,          /* one output channel */
		paFloat32,  /* 32 bit floating point output */
		44100,
		256,        /* frames per buffer, i.e. the number
					of sample frames that portaudio will
					request from the callback. many apps
					may want to use
					paframesperbufferunspecified, which
					tells portaudio to pick the best,
					possibly changing, buffer size.*/
		audioCallback, /* this is your callback function */
		&data);

	if (err != paNoError) {
		std::cout << Pa_GetErrorText(err) << std::endl;
		err = Pa_Terminate();
		return err;
	}

	err = Pa_StartStream(stream);
	if (err != paNoError) {
		std::cout << Pa_GetErrorText(err) << std::endl;
		err = Pa_Terminate();
		return err;
	}

    // Main loop
    gEngine->render();

    // Clean up
    delete gEngine;

	// Stop the stream
	err = Pa_StopStream(stream);
	if (err != paNoError) {
		std::cout << Pa_GetErrorText(err) << std::endl;
		err = Pa_Terminate();
		return err;
	}
	//Close the stream
	err = Pa_CloseStream(stream);
	if (err != paNoError) {
		std::cout << Pa_GetErrorText(err) << std::endl;
		err = Pa_Terminate();
		return err;
	}

    // Exit program
    exit( EXIT_SUCCESS );
}

void initAudio() {

}

static int audioCallback(const void *inputbuffer, void *outputbuffer,
	unsigned long framesperbuffer,
	const PaStreamCallbackTimeInfo* timeinfo,
	PaStreamCallbackFlags statusflags,
	void *userdata) {

	const float *in = (const float*) inputbuffer;
	float *out = (float*)outputbuffer;
	for( int i=0; i<framesperbuffer; i++ )
	{
		*out++ = *in++;
	}
	return 0;
}

//static int audioCallback(const void *inputbuffer, void *outputbuffer,
//	unsigned long framesperbuffer,
//	const pastreamcallbacktimeinfo* timeinfo,
//	pastreamcallbackflags statusflags,
//	void *userdata)
//{
//	const float *in = (const float*)inputbuffer;
//	float *out = (float*)outputbuffer;
//	for (int i = 0; i<framesperbuffer; i++)
//	{
//		*out++ = *in++;
//	}
//	return 0;
//}
void render() {

    std::vector<glm::mat4> sceneMatrices;

    sceneMatrices.push_back(gEngine->getCurrentModelViewProjectionMatrix());
    sceneMatrices.push_back(gEngine->getCurrentModelViewMatrix());
    sceneMatrices.push_back(gEngine->getCurrentModelViewMatrix());
    sceneMatrices.push_back(glm::inverseTranspose(glm::mat4(gEngine->getCurrentModelViewMatrix())));

    for(std::vector<Level *>::iterator it = mLevels.begin(); it != mLevels.end(); ++it)
        (*it)->render(sceneMatrices, static_cast<float>(curr_time.getVal()));
}

void preSync() {

    if(gEngine->isMaster())
        curr_time.setVal(sgct::Engine::getTime());
}

void initialize() {

    mLevels.push_back(new Level("../assets/level1.obj", glm::vec4(0.8f, 0.2f, 0.2f, 1.0f)));
    mLevels.push_back(new Level("../assets/level2.obj", glm::vec4(0.2f, 0.8f, 0.2f, 1.0f)));
    mLevels.push_back(new Level("../assets/level3.obj", glm::vec4(0.2f, 0.2f, 0.8f, 1.0f)));
    mLevels.push_back(new Level("../assets/level4.obj", glm::vec4(0.8f, 0.8f, 0.2f, 1.0f)));

    glm::vec3 lightPosition = glm::vec3(0.0f, 0.0f, 0.0f);

    for(std::vector<Level *>::iterator it = mLevels.begin(); it != mLevels.end(); ++it)
        (*it)->initialize(lightPosition);
}


void encode() {

    sgct::SharedData::instance()->writeDouble(&curr_time);
}

void decode() {

    sgct::SharedData::instance()->readDouble(&curr_time);
}


void keyCallback(int key, int action) {

    if( gEngine->isMaster() ) {

        switch( key ) {
            case SGCT_KEY_RIGHT:
                mLevels[mLevelIndex]->incrementAngle(1.0f);
            break;

            case SGCT_KEY_LEFT:
                mLevels[mLevelIndex]->incrementAngle(-1.0f);
            break;

            case SGCT_KEY_UP:
                if(action == SGCT_PRESS) {
                    if(mLevelIndex == mLevels.size() - 1)
                        mLevelIndex = 0;
                    else
                        mLevelIndex++;
                }
            break;

            case SGCT_KEY_DOWN:
                if(action == SGCT_PRESS) {
                    if(mLevelIndex == 0)
                        mLevelIndex = mLevels.size() - 1;
                    else
                        mLevelIndex--;
                }
            break;

        }
    }
}

void cleanUp() {
    mLevels.clear();
    mLevels.shrink_to_fit();
}
