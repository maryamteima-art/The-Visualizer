#include "ofApp.h"

void ofApp::setup() 
{
	ofSetVerticalSync(true);
    ofSetWindowShape( 1280, 900 );
    
    //------ AUDIO SETUP ------------
    
    //FOR LYRICS 
    //Since lyrics syncs with audio file (but not the one used in audio analyser), I had to play and set its volume super low, hence duplication of load,play and loop
    string lyrics_path = "lyrics.txt";
    loadlyric(lyrics_path);
    lyricFont.load("font_bold.ttf", 24);
    audioFile.load("song.mp3");
    audioFile.play();
    //Looping Song
    audioFile.setLoop(true);
    audioFile.setVolume(0.01);

    
    // jump to a specific time in our song (good for testing)
    // m_audioAnalyser.setPositionMS( 30 * 1000 ); // 30 seconds in (30000 ms)
	
    // Set up the audio analyser:
    //     int _numBinsPerAverage: size in bins per average number from the full 513 bin spectrum
    //
    //     As an example, if _numBinsPerAverage is set to 10, every 10 bins from the full 513 bin spectrum
    //     will be averaged. You would have 513/10 averages in your set of linear averages.
    //     This would be 52 bins (51 + 1 bins as 10 does not go into 513 evenly).
    //     The reason for doing this is to get a smaller set of frequency ranges to work with. In the full 513
    //     bin spectrum, the each bin is 22,500hz/513 wide -> 43.8 Hz.
    //     If you average them into groups of 10, each bin is 438 Hz wide, which can be easier to work with

    m_audioAnalyser.init("song.mp3", 10 );
	m_audioAnalyser.setLoop(true);
	m_audioAnalyser.play();

    //----- BACKGROUND TRANSITION COLORS SETUP --------
    bg_effect.load("img/bg.png");
    //Morning
    colorChange_vect.push_back({ 0, ofColor::skyBlue });
    //Sunset
    colorChange_vect.push_back({50, ofColor::lightCoral});

    //Sunset-Dark
    colorChange_vect.push_back({ 67, ofColor(238,85,85)});
    
    //Nighttime
    colorChange_vect.push_back({80, ofColor(2,19,87)});
    //Dawn
    colorChange_vect.push_back({127, ofColor(251,111,195)});
    //Morning
    colorChange_vect.push_back({144, ofColor::skyBlue }); 

    //BACKGROUND FRAMES - Loading and pushing to vector array
    ofImage bg1, bg2, bg3, bg4;   
    if(bg1.load("img/bg_0.png")) bg_vect.push_back(bg1); ;
    if(bg2.load("img/bg_1.png")) bg_vect.push_back(bg2);;
    if(bg3.load("img/bg_2.png")) bg_vect.push_back(bg3);;
    if(bg4.load("img/bg_3.png")) bg_vect.push_back(bg4);;
    

    //PARTICLES SETUP
    ofImage img;
    if (img.load("img/p1.png")) images_vect.push_back(img);
    if (img.load("img/p2.png")) images_vect.push_back(img);
    if (img.load("img/p3.png")) images_vect.push_back(img);

    //DRAWN ASSETS
    word_before.load("img/word_before_new.png");
    word_after.load("img/word_after_new.png");
    mirror.load("img/mirror_new.png");
    
    //Centreing Images to centre of Screen (from its centre pivot, as opposed to left-corner)
    //All images are in same location, hence just copying data
    word_before_pos.set(ofGetWindowWidth() / 2 - word_before.getWidth() / 2, ofGetWindowHeight() / 2 - word_before.getHeight() / 2);
    word_after_pos = word_before_pos;
    mirror_pos = word_before_pos;
    
    //SUN AND MOON SETUP
    sunCol = ofColor::yellow;
    moonCol = ofColor::lightGray;
    //Setting positions via the pos vectors for objects
    sun_pos.set(ofGetWindowWidth()/2, -200);
    moon_pos.set(ofGetWindowWidth() / 2, -200);

    //WHITE STARS SETUP
    int numStars = 100;
    stars_vect.clear();
    for (int i = 0; i < numStars; i++) {
        ofVec2f randPos(ofRandom(ofGetWidth()), ofRandom(ofGetHeight()));
        // Random base size
        float baseSize = ofRandom(0.03, 0.06);  
        float maxSize = baseSize * 2;
        stars_vect.push_back(Star(randPos, baseSize, maxSize));
    }

    //RED STARS
    int rednumStars = 50;
    redStars_vect.clear();
    for (int i = 0; i < rednumStars; i++) {
        ofVec2f randPos(ofRandom(ofGetWidth()), ofRandom(ofGetHeight()));
        // Random base size
        float baseSize = ofRandom(0.05, 0.07);
        float maxSize = baseSize * 2;
        redStars_vect.push_back(Star(randPos, baseSize, maxSize));
    }

    //END STARS
    int endnumStars = 50;
    endStars_vect.clear();
    for (int i = 0; i < endnumStars; i++) {
        ofVec2f randPos(ofRandom(ofGetWidth()), ofRandom(ofGetHeight()));
        // Random base size
        float baseSize = ofRandom(0.03, 0.05);
        float maxSize = baseSize * 2;
        endStars_vect.push_back(Star(randPos, baseSize, maxSize));
    }
}

void ofApp::update() 
{
    //AUDIO ANALYSER
    m_audioAnalyser.update();


    //Setting currenttime to starting of the song normalized
    float curr_time = audioFile.getPosition() * song_dur;

    //HAVING THE LYRICS FADE OUT BEFORE LOOPING AT THE END
    if (curr_time > 176) {
        fadelyrics(2000);
    }
    
    //LOOPING LYRICS WITH SONG
    //If current time is set back to 0:00s (song over, thus relooped) 
    if (curr_time < prev_time) {
        //Reset lyrics
        resetlyric();
    }
    //set previous time to current time
    prev_time = curr_time;

    //UPDATING LYRICS
    updatelyric(curr_time);

    //MOVING TEXT UPWARDS
    for (auto& lyric : lyrics_vect) {
        if (lyric.active) {
            //Fade in
            lyric.activeTime += ofGetLastFrameTime();

            // Calculate new alpha based on active time
            float alphaRatio = lyric.activeTime / fade_dur;
            lyric.alpha = ofClamp(alphaRatio * 255, 0, 255);

            //Move Up
            lyric.ypos -= (move_speed * ofGetLastFrameTime());
        }
    }
    
    //SUN AND MOON PARALLAX
    // Frequency range for male's vocals (300Hz to 3400Hz)
    float vocalAmplitude = m_audioAnalyser.getAvgForFreqRange(300, 3400);

    //Radii Limits
    float maxRadius = 10; 
    float minRadius = 5; 

    // Target radius based on vocal amplitude, mapping it to 50-70
    float targetRadius = ofMap(vocalAmplitude, 0, 1, minRadius, maxRadius);

    // Smoothly interpolate towards the target radius 
    sunRad = ofLerp(sunRad, targetRadius, 0.05);
    moonRad = ofLerp(moonRad, targetRadius, 0.05);

    float sunsetTime = 67, sunriseTime = 85;

    //If timstamp is at 29sec move sun down (to make way for petals)
    if (curr_time>29.0) {
        // Move sun down
        sun_pos.y = ofLerp(sun_pos.y, 4000, 0.005);
    }

    //if current time exceeds sunset timestamp, and sunrise isn't here yet
    if (curr_time > sunsetTime && curr_time < sunriseTime) {
        //Nighttime
        night = true;
        // Move sun down
        sun_pos.y += 5; 
        // Move moon up (+100 is so that it's slightly below lyrcis)
        moon_pos.y = ofLerp(moon_pos.y, (ofGetWindowHeight() / 2)+100, 0.01); 
        //Sun Fades out
        sun_alpha = ofLerp(sun_alpha, 0, 0.01); 
        //Moon Fades in
        moon_alpha = ofLerp(moon_alpha, 255, 0.01); 
    }
    else {
        //Daytime
        night = false;
        // Move moon down
        moon_pos.y += 5; 
        // Move sun up
        sun_pos.y = ofLerp(sun_pos.y, (ofGetWindowHeight() / 2)+100, 0.01);
        sun_alpha = ofLerp(sun_alpha, 255, 0.01); 
        moon_alpha = ofLerp(moon_alpha, 0, 0.01);
    }

    //HANDRAWN ASSETS PARALLAX (ONLY FADING NOW)
     // Word_before 
    if (curr_time >= 123 && curr_time < 129) {
        // Fade in word_before
        word_before_alpha = ofLerp(word_before_alpha, 255, 0.05); 
    }
    else if (curr_time >= 129 && curr_time < 137) {
        // Fade out word_before
        word_before_alpha = ofLerp(word_before_alpha, 0, 0.05); 
        // Fade in word_after
        word_after_alpha = ofLerp(word_after_alpha, 255, 0.05); 
    }
    else if (curr_time >= 137) {
        // Fade out word_after
        word_after_alpha = ofLerp(word_after_alpha, 0, 0.05); 
    }

    // Mirror fade in and fade out logic
    if (curr_time >= 137 && curr_time < 142) {
        // Fade in
        mirror_alpha = ofLerp(mirror_alpha, 255, 0.05); 
    }
    else if (curr_time >= 142) {
        // Fade out
        mirror_alpha = ofLerp(mirror_alpha, 0, 0.05); 
    }

    //BACKGROUND COLOUR TRANSITIONS
    bgChangeCol(curr_time);

    //PARTICLES UPDATING
    //load particle based on Volume
    loadParticle();

    // Update particles
    updateParticle();

    //STARS TWINKLING
    float syAmp = m_audioAnalyser.getAvgForFreqRange(300, 3400); 
    for (auto& star : stars_vect) {
        star.update(syAmp);
    }

    //red stars
    float bellAmp = m_audioAnalyser.getAvgForFreqRange(42, 200);
    for (auto& star : redStars_vect) {
        star.update(bellAmp);
    }

    //end stars
    float drumAmp = m_audioAnalyser.getAvgForFreqRange(0, 10);
    for (auto& star : endStars_vect) {
        star.update(drumAmp);
    }
    
}


void ofApp::draw() 
{

    //NARRATIVE BG
    ofBackground(bgColor);
    
    //WHITE STARS
    //Setting currenttime to starting of the song normalized (need this here to specify when to draw)
    float curr_time = audioFile.getPosition() * song_dur;

    if (curr_time > 0 && curr_time < 175) {
        for (auto& star : stars_vect) {
            //fades-in stars when night occurs, then fades out otherwise
            star.fadeStar(curr_time, 10, 175);
            star.draw(ofColor::white);
        }
    }

    //SUNSET STARS - BELLS
    if (curr_time > 105 && curr_time < 175) {
        for (auto& star : redStars_vect) {
            //fades-in stars when night occurs, then fades out otherwise
            star.fadeStar(curr_time, 105, 175);
            star.draw(ofColor::peachPuff);
        }
    }

    //ENDING STARS 
    if (curr_time > 132 && curr_time < 175) {
        for (auto& star : endStars_vect) {
            //fades-in stars when night occurs, then fades out otherwise
            star.fadeStar(curr_time, 132, 175);
            star.draw(ofColor::blueViolet);
        }
    }

    //SUN AND MOON
    // Draw moon
    ofSetColor(moonCol, moon_alpha);
    ofDrawCircle(moon_pos, moonRad);
  
    // Draw sun
    ofSetColor(sunCol, sun_alpha);
    ofDrawCircle(sun_pos, sunRad);
    
    //HANDDRAWN IMAGES DRAW
    //Draw word_before-img
    ofSetColor(255, 255, 255, word_before_alpha);
    word_before.draw(word_before_pos.x, word_before_pos.y);

    //Draw word_after-img
    ofSetColor(255, 255, 255, word_after_alpha);
    word_after.draw(word_after_pos.x, word_after_pos.y);

    //Draw mirror-img
    ofSetColor(255, 255, 255, mirror_alpha);
    mirror.draw(mirror_pos.x, mirror_pos.y);
    
    // PARTICLES
    for (Particle& p : particles_vect) {
        p.draw();
    }
    
    //LYRICS
    //Text COlor
    ofSetColor(255, 255, 255);
    
    float y_start = 50;
    float space = 50;
    ofPushStyle();
    //DRAWING LYRICS
    for (auto& lyric : lyrics_vect) {
        if (lyric.active) {
            //Set font's color & alpha
            ofSetColor(lyric.col, lyric.alpha);

            //Drawing the text in a list
            //ofDrawBitmapString(lyric.line, ofGetWindowWidth() / 2.2, (-500) + lyric.ypos);
            lyricFont.drawString(lyric.line, ofGetWindowWidth() / 2.5, (-500) + lyric.ypos);
        }
        if (lyric.ypos <= 0) {
            lyric.active = false;
        }

    }
    ofPopStyle();


    //BG-FRAMES
    if (curr_bg_index < bg_vect.size()) {
        bg_vect[curr_bg_index].draw(0, 0, ofGetWidth(), ofGetHeight());
    }

    if (!ctrl_hide) {

        //ANALYSER
        ofBackground(ofColor::black);
        ofSetColor(255);

        // Volume Level
        ofSetColor(ofColor::white);

        //Text
        ofDrawBitmapString("Volume Level", 140, 50);

        //Volume Channels (R,L,Mix)
        ofDrawCircle(100, 100, m_audioAnalyser.getLeftLevel() * 100.0f);
        ofDrawCircle(200, 100, m_audioAnalyser.getRightLevel() * 100.0f);
        ofDrawCircle(300, 100, m_audioAnalyser.getMixLevel() * 100.0f);

        //Volume Channel - Texts
        ofDrawBitmapString("Left", 80, 170);
        ofDrawBitmapString("Right", 180, 170);
        ofDrawBitmapString("Mix", 290, 170);

        //Frequency-Intruments Texts
        ofDrawBitmapString("Toms/Kick Drums", 450, 170);

        ofDrawBitmapString("Snare Drums", 900, 170);


        ofDrawBitmapString("Synthesizer:\nBimp Sound", 600, 170);


        ofDrawBitmapString("Bells", 750, 170);


        ofDrawBitmapString("   SCENE CONTROLS ", 1050, 70);
        ofDrawBitmapString("----------------------", 1050, 80);

        ofDrawBitmapString("Wind: Left/Right Keys x2", 1050, 100);

        ofDrawBitmapString("Particles: Up Key", 1050, 130);

        ofDrawBitmapString("Frame: Down Key", 1050, 160);

        ofDrawBitmapString("Panel Toggle: Space Key", 1050, 190);

        // Frequency / FFT information
        m_audioAnalyser.drawWaveform(40, 300, 1200, 90);
        m_audioAnalyser.drawSpectrum(40, 460, 1200, 128);

        m_audioAnalyser.drawLinearAverages(40, 650, 1200, 128);

        //---- BINS OBTAIN -------
        // Get the decibel levels for the frequency bins we are interested in

        //---- BINS - DRUMS --------
        drumAvg = binAvg(0, 1);

        //---- BINS - SYNTHEZISER --------

        //Calculate Avg 
        syAvg = binAvg(2, 4);

        //float tone1 = m_audioAnalyser.getLinearAverage(12); //guitar


        //---- BINS - Snap  --------
        bellAvg = binAvg(0, 1);

        float snare = m_audioAnalyser.getLinearAverage(22); //Snare


        //--- BIN AMPLITUDES MAPPING TO BRIGHTNESS -------- 
        // Advanced: can also get a custom average for a frequency range if you know the frequencies (can get them from mousing over the full spectrum)
        // float customAverage = m_audioAnalyser.getAvgForFreqRange( 128.0f, 300.0f );

        //Frequency Circles Mapping - Brightnesses
        m_drumCircle = ofMap(drumAvg, 10.0f, 20.0f, 0.0f, 1.0f, true);  //Toms
        m_synthCircle = ofMap(syAvg, 36.0f, 54.0f, 0.0f, 1.0f, true); //Synthesizer
        m_guitCircle = ofMap(bellAvg, 240.0f, 270.0f, 0.0f, 1.0f, true); //Bells Sounds
        m_snareCircle = ofMap(snare, 16.0f, 23.0f, 0.0f, 1.0f, true); //Snare


        //---- DRAW FREQUENCY CIRCLES FROM BRIGHTNESS ---------
        // Draw circles to indicate activity in the frequency ranges we are interested in

        //RED
        ofSetColor(ofFloatColor(m_drumCircle, 0.0f, 0.0f));
        ofDrawCircle(500, 100, 50);

        //GREEN
        ofSetColor(ofFloatColor(0.0f, m_synthCircle, 0.0f));
        ofDrawCircle(650, 100, 50);

        //BLUE
        ofSetColor(ofFloatColor(0.0f, 0.0f, m_guitCircle));
        ofDrawCircle(800, 100, 50);

        //WHITE
        ofSetColor(ofFloatColor(m_snareCircle, m_snareCircle, m_snareCircle));
        ofDrawCircle(950, 100, 50);

        ofSetColor(ofColor::white);

        // song time in seconds. Can use m_soundPlayer.setPositionMS( time ) to jump to a specific time
        float songTimeSeconds = m_audioAnalyser.getPositionMS() / 1000.0f;
        ofDrawBitmapString("Song time: " + ofToString(songTimeSeconds), 40, 250);
    }


    //BG EFFECTS
    ofPushMatrix();
    ofScale(1.3, 1.2);
    bg_effect.draw(0, 0);
    ofPopMatrix();
}

void ofApp::keyPressed(int key) {
    
    //HIDE CONTROL PANEL 
    if (key == ' ') {
        //Visibility Toggle
        ctrl_hide = !ctrl_hide; 
    }

    //CONTROLLING "WIND" FOR PARTICLES
    //LEFT
    if (key == OF_KEY_LEFT) {
        
        windForce -= 2; 
    }
    //RIGHT
    else if (key == OF_KEY_RIGHT) {
        
        windForce += 2;
    }
    //Clamping windforce to ensure no extreme values
    //windForce = ofClamp(windForce, -0.5, 0.5);

    //CONTROLLING BG FRAME
    if (key == OF_KEY_DOWN) {
        curr_bg_index++;
        //if at end, loop to first img
        //+1 was added to loop to last image
        if (curr_bg_index >= images_vect.size()+1) curr_bg_index = 0;
    }


    //CONTROLLING PARTICLES IMAGES
    if (key == OF_KEY_UP) {
        curr_img_index++;
        //Loop to first img
        if (curr_img_index >= images_vect.size()) curr_img_index = 0;
    }
    
}

//CUSTOM FUNCTIONS

//LYRIC PARSING
void ofApp::loadlyric(string& filename) {

    ifstream file(ofToDataPath(filename));
    string line;

    while (getline(file, line)) {
        //Finding comma to seperate timestamp from lyric text itself
        size_t delimiterPos = line.find(',');

        if (delimiterPos != string::npos) {
            //Convert timestamp to float
            float time = stof(line.substr(0, delimiterPos));
            //Add text after comma to lyric string vector
            string lyricLine = line.substr(delimiterPos + 1);

            //Creating the lyrics struct and pushing it into the lyric vector
            lyrics_vect.push_back({ time,lyricLine,float(ofGetHeight()),ofColor(255) });

            cout << "Loaded lyric: " << lyricLine << " at time " << time << endl;
        }
    }
}

void ofApp::resetlyric() {
    for (auto& lyric : lyrics_vect) {
        lyric.active = false;
        lyric.alpha = 0;
        lyric.activeTime = 0;
        // Reset position
        lyric.ypos = float(ofGetHeight()); 
    }
}

//BIN AVERAGE
float ofApp::binAvg(int binStart, int binEnd) {
    float avgVar = 0;
    for (int i = binStart; i <= binEnd; ++i) {
        avgVar += m_audioAnalyser.getLinearAverage(i);
    }
    //Calculate Avg 
    //the +1 is because of index counting (Ex. 4 positions looped thorugh (0,1,2,3) but last bin is 4 not 3)
    return avgVar /= (binEnd - binStart + 1);
}

float ofApp::lerp(float start, float end, float percent) {

    //Ease Out or Removing High Frequency Noise
    return (start + percent * (end - start));
}

//BG COLOUR TRANSITION
void ofApp::bgChangeCol(float curr_time) {
    //Loop through colors vectors (with timestamp,color)
    for (int i = 0; i < colorChange_vect.size() - 1; ++i) {
        //if curr_time surpasses the timestamp in current ColorVector Index (i.e, morning) && curr_time hasn't reached the next timestamp yet (i.e sunset)
        if (curr_time >= colorChange_vect[i].timestamp && curr_time < colorChange_vect[i + 1].timestamp) {

            //Lerp amount is curr_timestamp (i.e morning timestamp) / next_timestamp (i.e sunset timestamp)
            float lerpAmount = (curr_time - colorChange_vect[i].timestamp) /
                (colorChange_vect[i + 1].timestamp - colorChange_vect[i].timestamp);

            //Using Lerp to Interpolate the RGB values
            float r = lerp(colorChange_vect[i].color.r, colorChange_vect[i + 1].color.r, lerpAmount);
            float g = lerp(colorChange_vect[i].color.g, colorChange_vect[i + 1].color.g, lerpAmount);
            float b = lerp(colorChange_vect[i].color.b, colorChange_vect[i + 1].color.b, lerpAmount);

            bgColor = ofColor(r, g, b);
            break;
        }
    }

}


void ofApp::updatelyric(float curr_time) {
    //To track what lyric we're on in txt file
    float curr_lyric_index = -1;

    //LOADING LYRICS STEPWISE
    for (int i = 0; i < lyrics_vect.size(); ++i) {
        auto& lyric = lyrics_vect[i];

        // Activate lyric on txt timestamp
        //if current time surpasses the timestamp and lyric hasn't been activated yet. 
        if (curr_time >= lyric.time && !lyric.active) {
            //if current lyric isn't the first one
            if (curr_lyric_index != -1) {
                // Deactivate previously active lyric
                lyrics_vect[curr_lyric_index].active = false;
                //Set its alpha to invisible
                lyrics_vect[curr_lyric_index].alpha = 0;
                //reset time
                lyrics_vect[curr_lyric_index].activeTime = 0;
            }
            lyric.active = true;
            //update index
            curr_lyric_index = i;
            //For looping purposes, need to reset alpha & time
            //Set its alpha to invisible
            lyrics_vect[curr_lyric_index].alpha = 0;
            //reset time
            lyrics_vect[curr_lyric_index].activeTime = 0;
        }
    }

}

void ofApp::fadelyrics(float fadeSpeed) {
    //Loop over lyrics vector
    for (auto& lyric : lyrics_vect) {
        //If it's alpha is visible
        if (lyric.alpha > 0) {
            //Subtract alpha
            //Max is used to ensure it doesn't go -alpha (alpha caps at 0)
            lyric.alpha = MAX(0, lyric.alpha - fadeSpeed * ofGetLastFrameTime());

            //Debugging
            cout << "Fading lyric: " << lyric.line << " Alpha: " << lyric.alpha << endl;

        }
    }
}

//PARTICLES
//Loading particles
void ofApp::loadParticle() {
    
    float volume = m_audioAnalyser.getMixLevel() * 100.0f;
    
    //Number of particles to generate (division used to decrease amount)
    int num_new_parts = volume/20; 

    //Maps image to each new particle
    for (int i = 0; i < num_new_parts; i++) {
        particles_vect.push_back(Particle(ofVec2f(ofRandomWidth(), ofRandomHeight()), &images_vect[curr_img_index]));
    }
}

//UTILIZING USER INPUT TO UPDATE PARTICLES
//updating particles
void ofApp::updateParticle() {
    // Update existing particles

    //Loop over particles vector (from end to beginning <--)
    for (int i = particles_vect.size() - 1; i >= 0; i--) {
        //commence particle class's update method
        particles_vect[i].update(windForce,windForce_down);
        //if timespan is hit, commence particle class's dead method
        if (particles_vect[i].dead()) {
            //Delete particle
            particles_vect.erase(particles_vect.begin() + i);
        }
    }
}

//PARTICLES FUNCTIONS

void Particle::update(float wind, float windForce_down) {
    //Sine for wavelike motion
    pos.x += sin(ofGetElapsedTimef() + pos.y * 0.01) * 5.0;
    pos += vel;
    //deccrement lifespan
    lifespan -= 2;
    //Apply wind force to x-direction of the particle
    vel.x += wind*0.01;
    //Apply speed down (user controlled)
    //Scale is used to slow it down
    vel.y += windForce_down*0.01;


}

void Particle::draw() {
    ofSetColor(255, 255, 255, lifespan);
    //Adjusting particles with scales
    img->draw(pos.x, pos.y, img->getWidth() * scale, img->getHeight() * scale);
}

bool Particle::dead() {
    
    return lifespan < 0;
}

//STAR FUNCTIONS
// fadein and fadeout when appearing and disappearing 
void Star::fadeStar(float curr_time, float fade_in_time, float fade_out_time) {
    if (curr_time > fade_in_time && curr_time < fade_out_time) {
        fade = ofLerp(fade, 1.0, 0.005); 
    }
    else if (curr_time > fade_out_time) {
        // Fade out
        fade = ofLerp(fade, 0.0, 0.005);
    }

    // Apply fade value to alpha
    alpha = fade;
};

// Change size and alpha to simulate twinkle
void Star::update(float freqAmp) {
    
    // Adjust size based on frequency amplitude
    size = ofLerp(baseSize, maxSize, freqAmp);

    // Adjust alpha based on frequency amplitude
    alpha = ofLerp(0.5, 1.0, freqAmp);
}

void Star::draw(ofColor color) {
    ofPushStyle();
    //Apply alpha changes
    ofSetColor(color, alpha * 255);
    //Draw circles
    ofDrawCircle(pos.x, pos.y, size);        
    ofPopStyle();

}