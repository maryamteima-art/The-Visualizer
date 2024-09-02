#pragma once

#include "ofMain.h"
#include "AudioAnalyser.h"
#include<string>
#include <vector>

//PARSING LYRICS - STRUCT

struct lyricline {
	float time;
	string line;
	float ypos;
	ofColor col;
	bool active;
	float alpha;
	float activeTime;

	//Constructor
	lyricline(float t, string l, float y, ofColor c)
		: time(t), line(l), ypos(y), col(c), active(false), alpha(0), activeTime(0) {}

};

//CHANGING BG COLOR - STRUCT

struct colorChange {
	float timestamp; 
	ofColor color;
};

//PARTICLES CLASS
class Particle {

public:
	ofVec2f pos;
	ofVec2f vel;
	float lifespan;
	float scale;

	// Adjust size based on frequency amplitude
	float baseSize = 0.03; 
	float maxSize = 0.08;

	// Pointer to the image
	ofImage* img; 

	//Contructor
	Particle::Particle(ofVec2f pos, ofImage* img) {
		// Start above the screen. 200 is just buffer that allowed particles to spawn even higher than top of screen (looked better)
		this->pos = ofVec2f(ofRandomWidth(), -200);

		//Downward velocity. 1-3 is vel ranges
		this->vel = ofVec2f(0, ofRandom(1, 3));

		lifespan = 1000;
		scale = ofRandom(baseSize, maxSize); 
		this->img = img;
	}

	void update(float wind, float windForce_down);

	void draw();

	bool dead();
};

//STARS CLASS
class Star {
public:
	ofVec2f pos;
	float alpha;
	float size;
	//For fade-in and fade-ou during appearance
	float fade;

	//Frequency Response variables
	// Original size of the star
	float baseSize; 
	// Maximum size in response to frequency
	float maxSize;  

	Star(ofVec2f p, float baseSize, float maxSize) : pos(p), baseSize(baseSize), maxSize(maxSize) {
		// Start with random size and opacity
		size = baseSize;
		alpha = ofRandom(0.5, 1.0);
		fade = 0.0;
	}
	void fadeStar(float curr_time, float fade_in_time, float fade_out_time);
	void update(float freqAmp);
	void draw(ofColor color);
};


class ofApp : public ofBaseApp {

public:
	void setup();
	void update();
	void draw();
	void keyPressed(int key);

    AudioAnalyser m_audioAnalyser;

    float m_drumCircle;
    float m_synthCircle;
    float m_guitCircle;
    float m_snareCircle;
	
	//CUSTOM MEMBERS


	//Lyrics vector
	vector<lyricline>lyrics_vect;
	ofTrueTypeFont font;
	ofSoundPlayer audioFile;
	ofTrueTypeFont lyricFont;

	//Bins
	float drumAvg = 0;

	float syAvg = 0;

	float bellAvg = 0;


	//LYRIC PARALLAXING
	float fade_speed = 100;
	float fade_dur = 2.0;


	//Lyric Loop
	float prev_time = 0;
	//Ending lyrics fading before relooping
	float endFade_time = 175.0;
	//Duration
	float endfade_dur = 3.0; 


	//How long text moves up
	float move_speed = 50;
	float song_dur = 180.0;

	//Toggling Controls
	bool ctrl_hide = true;
	
	//Parallax Assets drawn
	ofImage bg_effect, word_before, word_after,mirror;
	ofVec2f word_before_pos, word_after_pos, mirror_pos;
	float word_before_alpha = 0, word_after_alpha = 0, mirror_alpha = 0;


	//BG Changing
	vector<colorChange> colorChange_vect;
	ofColor bgColor;

	vector<ofImage> bg_vect;
	int curr_bg_index = 0;

	//SUN & MOON
	ofVec2f sun_pos;
	ofVec2f moon_pos;
	ofVec2f sun_targ_pos;
	ofVec2f moon_targ_pos;
	float sunRad, moonRad;
	ofColor moonCol, sunCol;
	bool night = false;
	float sun_alpha = 255, moon_alpha = 0;

	//Particles
	vector<Particle> particles_vect;
	//Vedctor to store different particle images
	vector<ofImage> images_vect; 
	int curr_img_index = 0;
	float windForce = 0.0;
	float windForce_down = 1.0;

	//Stars
	std::vector<Star> stars_vect;
	ofImage star_img;


	std::vector<Star> redStars_vect;
	std::vector<Star> endStars_vect;

	//CUSTOM FUNCTIONS
	
	//Lyrics
	void loadlyric(string& filename);
	void updatelyric(float curr_time);
	void resetlyric();
	void fadelyrics(float fadeSpeed);

	//Bins
	float binAvg(int binStart, int binEnd);
	
	//Color Change
	void bgChangeCol(float curr_time);
	
	//Particles
	void loadParticle();
	void updateParticle();

	//Lerp
	float lerp(float start, float end, float percent);
};
