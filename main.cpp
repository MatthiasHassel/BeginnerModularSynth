/*
____  _____ _        _
| __ )| ____| |      / \
|  _ \|  _| | |     / _ \
| |_) | |___| |___ / ___ \
|____/|_____|_____/_/   \_\
http://bela.io
*/

/*
//Bachelor-Project Matthias Hassel
//Hochschule Darmstadt
//31.01.2022
//Modular Synth Prototype 2
*/

/*
//Bela Settings:
//Sample Rate: 44.1kHz
//Blocksize: 16 Samples
//Multiplexer Capelet: Active, 64 Channels -> Jumper set to bypass Analog 4-7
//(Audio Expander: Active, Channel 6+7 set to In- & Outputs, Wires need to be added to the Capelet for 10x Gain) <-- Hardware not ready yet
*/

/*
//BCD Decoder Wiring:
//Phase -> 3,3V
//LatchDisable -> 3,3V
//Blanking -> GND
//Vss -> GND
//VDD -> 3,3V
*/

#include <Bela.h>
#include <unistd.h>
#include <libraries/ADSR/ADSR.h>
#include <libraries/math_neon/math_neon.h>
#include <libraries/Scope/Scope.h>
#include <array>

// instantiate the scope
Scope scope;

int gAudioFramesPerAnalogFrame = 0;
bool gIsStdoutTty;
unsigned int gPrintCount = 0;

/*IO*/
std::array<const unsigned int, 5> kButtonPin = {{6, 10, 7, 3, 5}};	// Pin number of the pushbutton
std::array<bool, 5> buttonState; 									// 0: Waveform A, 1: Waveform B, 2: SeqPreset, 3: Seq Gate, 4: Envelope Gate 
std::array<float, 23> input;
// State machine states
enum {
	kStateOpen = 0,
	kStateJustClosed,
	kStateClosed,
	kStateJustOpen
};
// State machine variables
std::array<int, 5> gDebounceState;
std::array<int, 5> gDebounceCounter;
int gDebounceInterval = 0;
bool switchPatch;
int kSwitchPin = 0;
/*********/
/*IO*/

/*CLOCK*/
unsigned int gClockInterval = 0;
unsigned int gClockCounter = 0;
const int kClockOutPin = 1;
float gTempo = 0;
float gHighPulseTime = 2205;
/*CLOCK*/

/*SEQUENCER*/
int gSequencer1Location = 0;
int gPresetSequence = 0;
int gNumPresets = 9;
std::array<int, 8> gSequencer1Buffer0{{40, 43, 45, 43, 50, 48, 50, 52}}; //Pink Floyd - On The Run, MIDI Notes
std::array<int, 8> gSequencer1Buffer1{{36, 38, 40, 42, 44, 46, 48, 50}}; //Olivier Messiaen Modes 1-7, MIDI Notes
std::array<int, 8> gSequencer1Buffer2{{36, 37, 39, 40, 42, 43, 45, 46}};
std::array<int, 8> gSequencer1Buffer3{{36, 37, 38, 40, 41, 42, 44, 45}};
std::array<int, 8> gSequencer1Buffer4{{36, 37, 38, 41, 42, 43, 46, 47}};
std::array<int, 8> gSequencer1Buffer5{{36, 37, 41, 42, 43, 47, 48, 49}};
std::array<int, 8> gSequencer1Buffer6{{36, 38, 40, 41, 42, 44, 46, 47}};
std::array<int, 8> gSequencer1Buffer7{{36, 37, 38, 39, 41, 42, 43, 44}};
std::array<int, 8> gSequencer1Buffer8{{36, 36, 36, 36, 36, 36, 36, 36}};
std::array<std::array<int, 8>, 9> gSequencer1Buffer{{gSequencer1Buffer0, gSequencer1Buffer1, gSequencer1Buffer2, gSequencer1Buffer3, gSequencer1Buffer4, gSequencer1Buffer5, gSequencer1Buffer6, gSequencer1Buffer7, gSequencer1Buffer8}};
/*SEQUENCER*/

/*OSC*/
float gFrequency = 60.0; 
float gInFreq = 0.0;
float gOscFreq = 0.0;
float gDetune = 0.0;
float gVolAB = 0.0;
float gGlide = 0.0;
float gRampDuration = 0.0;
float gFrequencyIncrement = 0.0;
float gLastInFreq1;
float gFrequency1; // Oscillator1 frequency (Hz)
float gPhase1; // Oscillator1 phase
float gFrequency2; //Oscillator2 frequency (Hz)
float gPhase2; //Oscillatro2 phase
float gInverseSampleRate;
float gPulseWidth1 = 0;
float gPulseWidth2 = 0;
enum osc_type1
{
	square1,	// 0
	sine1,		// 1
	triangle1,	// 2
	sawtooth1,	// 3
	numOscTypes1
};
enum osc_type2
{
	square2,	// 0
	sine2,		// 1
	triangle2,	// 2
	sawtooth2,	// 3
	numOscTypes2
};
unsigned int gOscType1 = 0;
unsigned int gOscType2 = 0;
/*OSC*/

/*ENVELOPE*/
ADSR envelope; // ADSR envelope
float gAttack = 0.1; // Envelope attack (seconds)
int gAttackTimeCounter = 0;
bool gAttackState = false;
float gDecay = 0.2; // Envelope decay (seconds)
float gRelease = 0.5; // Envelope release (seconds)
float gSustain = 1.0; // Envelope sustain level
float gEnvelopeAmp = 0.0;
/*ENVELOPE*/

/*FILTER*/
// Filter state and coefficients
float gLastX1 = 0, gLastX2 = 0;
float gLastY1 = 0, gLastY2 = 0;
float gA1 = 0, gA2 = 0;
float gB0 = 1, gB1 = 0, gB2 = 0;
float gCutoff = 10000;
float gQ = 0.707;
float gCutoffCVIN = 0.0;
float gCVAmount = 0.0;

// Calculate the filter coefficients based on the given parameters
void calculate_coefficients(float sampleRate, float frequency, float q)
{
    float k = tanf(M_PI * frequency / sampleRate);
    float norm = 1.0 / (1 + k / q + k * k);
    
    gB0 = k * k * norm;
    gB1 = 2.0 * gB0;
    gB2 = gB0;
    gA1 = 2 * (k * k - 1) * norm;
    gA2 = (1 - k / q + k * k) * norm;	
}
/*FILTER*/

/*MIXER*/
float gAmplitude = 0.3f; // Amplitude scaling for oscillator's envelope
float gVolume = 1.0f;
float gMixerCVIn = 0.0;
float gMixerAmp = 0.0;
/*MIXER*/

bool setup(BelaContext *context, void *userData)
{
	scope.setup(1, context->audioSampleRate);
	gClockInterval = 0.5 * context->audioSampleRate;
	
	if(context->multiplexerChannels == 0 || context->analogFrames == 0) {
                rt_printf("Please enable the Multiplexer Capelet to run this code.\n");
                return false;
        }

	gInverseSampleRate = 1.0 / context->audioSampleRate;
	gPhase1 = 0.0;
	
	// Set buttons pins as inputs
	for(int i = 0; i < kButtonPin.size(); i++)
	{	
	pinMode(context, 0, kButtonPin[i], INPUT);
	}
	pinMode(context, 0, kSwitchPin, INPUT);
	pinMode(context, 0, kClockOutPin, OUTPUT);
	
	gDebounceInterval = .05* context->digitalSampleRate;
	gDebounceState.fill(kStateOpen);
	gDebounceCounter.fill(0);
	buttonState.fill(1);
	
	if(context->analogFrames)
		gAudioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;
	
	gIsStdoutTty = isatty(1); // Check if stdout is a terminal

    // Set ADSR parameters
	envelope.setAttackRate(gAttack * context->audioSampleRate);
	envelope.setDecayRate(gDecay * context->audioSampleRate);
	envelope.setReleaseRate(gRelease * context->audioSampleRate);
	envelope.setSustainLevel(gSustain);
	
	return true;
}

void render(BelaContext *context, void *userData)
{
	/*SERIAL MONITOR*/
	/*
    gPrintCount += context->analogFrames;
	if(gPrintCount >= (unsigned int)(context->analogSampleRate * 0.1)) 
	{
	    gPrintCount = 0;
	    if(gIsStdoutTty)
	    rt_printf("\e[1;1H\e[2J");      // Command to clear the screen (on a terminal)
	    for (unsigned int i = 0; i < 24; i++)
	    {
			rt_printf("Input %d: ", i);
	     	rt_printf("%f ", input[i]);
	        if(i%8 == 0)
	        rt_printf("\n");
	    }
	}
	*/
	/*SERIAL MONITOR*/                   
	            			
	        
	for(unsigned int n = 0; n < context->audioFrames; n++) 
	{
		/* Go through each multiplexer setting of each analog input and display the value */
	    for(unsigned int i = 0; i < 3; i++) 
	    {
	    	for(unsigned int muxChannel = 0; muxChannel < context->multiplexerChannels; muxChannel++) 
	    	{
	             input[muxChannel + (8*i)] = multiplexerAnalogRead(context, i, muxChannel);
	        }
	    }
	    
        //make sure that PotValues reach from 0.0 to 1.0
		for(unsigned int i = 0; i < 22; i++)
		{
			input[i] = input[i]*(4.096/3.3);
			if (input[i] < 0.0003)
			{
				input[i] = 0.0;
			}
			if (input[i] > 1.0)
			{
				input[i] = 1.0;
			}
		}

        //scale each parameter   
		gOscFreq = input[0]*1000.0;
		gPulseWidth1 = input[1] * 4.0 - 2.0;
		gDetune = input[2]*0.04;
		gPulseWidth2 = input[3] * 4.0 - 2.0;
		gVolAB = input[4];
		gGlide = input[5]*0.2;
		gCutoff = map(input[6], 0, 1, 80, 15000);
		gQ = map(input[7], 0, 1, 0.5, 10);
		gCVAmount = input[8]; 
		gAttack = input[9]*0.7 + 0.001;
		gRelease = input[10]*2.0;
		gTempo = map(input[11], 0, 1, 40, 208)*4.0;
		gVolume = input[12];
		gMixerCVIn = multiplexerAnalogRead(context, 3, 0);
		
		switchPatch = digitalRead(context, n, kSwitchPin);
		
		/*CLOCK*/
			gClockInterval = 60.0 * context->audioSampleRate / gTempo;
			if(++gClockCounter >= gClockInterval)
			{
				gClockCounter = 0;
				if(switchPatch)
				{
					gSequencer1Location++;
					if(gSequencer1Location >= gSequencer1Buffer0.size())
					{
						gSequencer1Location = 0;
					}
					// ...turn ON the envelope's gate
					envelope.gate(true);
					gAttackState = true;
					gAttackTimeCounter = 0;	
				}
			}
			if(!switchPatch)
			{
				if(gClockCounter < gHighPulseTime)
				{
					digitalWriteOnce(context, n, kClockOutPin, HIGH);
				}
				else
				{
					digitalWrite(context, n, kClockOutPin, LOW);
				}
			}
		/*CLOCK*/
		
		/*BUTTONS + GATES + SWITCH*/	
		for(unsigned int n = 0; n < context->digitalFrames; n++) 
		{ 
			// Read the button state
   			for(int i = 0; i < kButtonPin.size(); i++)
	   		{
	   			buttonState[i] = digitalRead(context, n, kButtonPin[i]);
	   			
	   			if(gDebounceState[i] == kStateOpen) {
	   			// Button is not pressed, could be pressed anytime
	   			// Input: look for switch closure
	   			if (buttonState[i] == 1)
	   			{
	   				gDebounceState[i] = kStateJustClosed;
	   				gDebounceCounter[i] = 0;
	               
	                if (i == 0) //OSC1 Mode
	                {
	                    // Change oscillator type
						gOscType1 += 1;
						// Wrap around number of different oscillator types if counter is out of bounds
						if(gOscType1 >= numOscTypes1)
							gOscType1 = 0;
						rt_printf("Oscillator1 type: %d\n", gOscType1);
	                }
	                if (i == 1) //OSC2 Mode
	                {
	                    // Change oscillator type
						gOscType2 += 1;
						// Wrap around number of different oscillator types if counter is out of bounds
						if(gOscType2 >= numOscTypes2)
							gOscType2 = 0;
						rt_printf("Oscillator2 type: %d\n", gOscType2);
				    }
				    if (i == 2)
				    {
					    gPresetSequence++;
                        //Wrap around number of different Sequencer Presets if counter is out of bounds
					    if(gPresetSequence >= gNumPresets)
						{
							gPresetSequence = 0;
						}
				    }
	                if (i == 3) //SeqPreset
	                {
	                	if(!switchPatch)
	                	{
	                		// ...turn ON the envelope's gate
							envelope.gate(true);
							gAttackState = true;
							gAttackTimeCounter = 0;	
	                	}
	                }
	                if (i == 4)
	                {
	                	if(!switchPatch)
	                	{
		                	gSequencer1Location++;
							if(gSequencer1Location >= gSequencer1Buffer0.size())
							{
								gSequencer1Location = 0;
							}	
	                	}
	                }
	   			}
	   		}
	   		else if(gDebounceState[i] == kStateJustClosed) {
	   			// Button was just pressed, wait for debounce
	   			// Input: run counter, wait for timeout
	   			if(++gDebounceCounter[i] >= gDebounceInterval)
	   			{
	   				gDebounceState[i] = kStateClosed;
	   			}
	   		}
	   		else if(gDebounceState[i] == kStateClosed) {
	   			// Button is pressed, could be released anytime
	   			// Input: look for switch opening
	   			if(buttonState[i] != 1)
	   			{
	   				gDebounceState[i] = kStateJustOpen;
	   				gDebounceCounter[i] = 0;
	   			}
	   		}
	   		else if(gDebounceState[i] == kStateJustOpen) {
	   			// Button was just released, wait for debounce
	   			// Input: run counter, wait for timeout
	   			if(++gDebounceCounter[i] >= gDebounceInterval)
	   			{
	   				gDebounceState[i] = kStateOpen;
	   			}
	   		}
	   		}
		}
		/*BUTTONS + GATES + SWITCH*/

		/*SEQUENCER*/
		float Seq0Out = input[gSequencer1Location + 13];
		float Seq1Out = 440*powf(2.0, (gSequencer1Buffer[gPresetSequence][gSequencer1Location] - 69.0) / 12.0); //convert MIDI to Frequency
		Seq1Out = log2(Seq1Out/55)/4.096;
		float Seq0OutScaled = Seq0Out * (4.096/5.0);
		float Seq1OutScaled = Seq1Out * (4.096/5.0);
		if(!switchPatch)
		{
			for(unsigned int n = 0; n < context->analogFrames; n++) 
			{
			analogWriteOnce(context, n, 2, Seq1OutScaled);
			analogWriteOnce(context, n, 1, Seq0OutScaled);
			}
		}
		/*SEQUENCER*/
		
		/*OSC*/
		if(switchPatch)
		{
			gInFreq = 55.0 * powf(2.0, Seq1Out*4.096);
		}
		if(!switchPatch)
		{
			gInFreq = 55.0 * powf(2.0, input[22]*4.096);
		}
		gRampDuration = gGlide * context->audioSampleRate;
		if (gGlide == 0)
		{
			gFrequencyIncrement = 0;
			gLastInFreq1 = gInFreq;
			gFrequency = gInFreq + gOscFreq;
		}
		else
		{
			gFrequencyIncrement =  (gInFreq - gLastInFreq1) / gRampDuration;
			gLastInFreq1 = gLastInFreq1 + gFrequencyIncrement;
		}
		gFrequency = gLastInFreq1 + gOscFreq + gFrequencyIncrement;
		
		// Calculate output based on the selected oscillator mode
		float Osc1Out;
		switch(gOscType1) {
			default:
			// SINEWAVE
			case sine1:
				Osc1Out = sinf_neon(gPhase1);
				break;
			// TRIANGLE WAVE
			case triangle1:
				if (gPhase1 > 0) {
				      Osc1Out = -1 + (2 * gPhase1 / (float)M_PI);
				} else {
				      Osc1Out = -1 - (2 * gPhase1/ (float)M_PI);
				}
				break;
			// SQUARE WAVE
			case square1:
				if (gPhase1 > gPulseWidth1) {
				      Osc1Out = 1;
				} else {
				      Osc1Out = -1;
				}
				Osc1Out = Osc1Out * 0.5;
				break;
			// SAWTOOTH
			case sawtooth1:
				Osc1Out = 1 - (1 / (float)M_PI * gPhase1);
				Osc1Out = Osc1Out * 0.5;
				break;
		}
		
		float Osc2Out;
		switch(gOscType2) {
			default:
			// SINEWAVE
			case sine2:
				Osc2Out = sinf_neon(gPhase2);
				break;
			// TRIANGLE WAVE
			case triangle2:
				if (gPhase2 > 0) {
				      Osc2Out = -1 + (2 * gPhase2 / (float)M_PI);
				} else {
				      Osc2Out = -1 - (2 * gPhase2/ (float)M_PI);
				}
				break;
			// SQUARE WAVE
			case square2:
				if (gPhase2 > gPulseWidth2) {
				      Osc2Out = 1;
				} else {
				      Osc2Out = -1;
				}
				Osc2Out = Osc2Out * 0.5;
				break;
			// SAWTOOTH
			case sawtooth2:
				Osc2Out = 1 - (1 / (float)M_PI * gPhase2);
				Osc2Out = Osc2Out * 0.5;
				break;
		}
		gFrequency1 = gFrequency * (1.0 + gDetune);
		gFrequency2 = gFrequency * (1.0 - gDetune);
		// Compute phase
		gPhase1 += 2.0f * (float)M_PI * gFrequency1 * gInverseSampleRate;
		if(gPhase1 > M_PI)
			gPhase1 -= 2.0f * (float)M_PI;
			
		gPhase2 += 2.0f * (float)M_PI * gFrequency2 * gInverseSampleRate;
		if(gPhase2 > M_PI)
			gPhase2 -= 2.0f * (float)M_PI;
		
		float OscOut = Osc1Out*(1 - gVolAB) + Osc2Out*gVolAB;
		/* Audio Patching, doesn't work yet
		if(!switchPatch)
		{
			analogWriteOnce(context, n/2, 6, OscOut*0.7);
		}
		*/
		/*OSC*/
		
		/*FILTER*/
		float CutoffCVIn;
		float FilterIn;
		/* Audio Patching, doesn't work yet
		if(switchPatch)
		{
			FilterIn = 0.5 * OscOut;
			CutoffCVIn = (Seq0Out - 0.5) * 20000;
		}
		if(!switchPatch)
		{
			FilterIn = analogRead(context, n/2, 6);
			CutoffCVIn = (input[23] - 0.5) * 20000;	
		}
		*/
		FilterIn = 0.5 * OscOut;
		if(switchPatch)
		{
			CutoffCVIn = (Seq0Out - 0.5) * 20000;
		}
		if(!switchPatch)
		{
			CutoffCVIn = (input[23] - 0.5) * 20000;	
		}
		gCutoff = gCutoff + (CutoffCVIn * gCVAmount);
		if (gCutoff > 15000)
		{
			gCutoff = 15000;
		}
		if (gCutoff < 80)
		{
			gCutoff = 80;
		}
		
		// Calculate initial coefficients
		calculate_coefficients(context->audioSampleRate, gCutoff, gQ); 
        float FilterOut = gB0*FilterIn + gB1*gLastX1 + gB2*gLastX2 - gA1*gLastY1 - gA2*gLastY2;

		gLastX2 = gLastX1;
		gLastX1 = FilterIn;
		gLastY2 = gLastY1;
		gLastY1 = FilterOut;
		/* Audio Patching, doesn't work yet
		if(!switchPatch)
		{
			analogWriteOnce(context, n/2, 7, FilterOut);
		}
		*/
		/*FILTER*/
		
		/*ENVELOPE*/	
		envelope.setAttackRate(gAttack * context->audioSampleRate);
		envelope.setReleaseRate(gRelease * context->audioSampleRate);
		if(gAttackState)
		{
			gAttackTimeCounter++;
			if(gAttackTimeCounter >= (gAttack * context->audioSampleRate))
			{
				envelope.gate(false);
				gAttackState = false;
			}
		}
		
		gEnvelopeAmp = gAmplitude * envelope.process();
		if(!switchPatch)
		{
			for(unsigned int n = 0; n < context->analogFrames; n++) 
			{
			analogWriteOnce(context, n, 0, gEnvelopeAmp * (4.096/5.0));
			}
		}
		/*ENVELOPE*/
		
		/*MIXER*/
		float MixerIn;
		/* Audiopatching, doesn't work yet
		if(switchPatch)
		{
			MixerIn = FilterOut;
			gMixerAmp = gEnvelopeAmp;
		}
		if(!switchPatch)
		{
			MixerIn = analogRead(context, n/2, 7);
			gMixerAmp = gMixerCVIn;
		}
		*/
		MixerIn = FilterOut;
		if(switchPatch)
		{
			gMixerAmp = gEnvelopeAmp;
		}
		if(!switchPatch)
		{
			gMixerAmp = gMixerCVIn;
		}
		float out = MixerIn * gVolume * gMixerAmp;
		// Write output to different channels
		for(unsigned int channel = 0; channel < 2; channel++) {
			audioWrite(context, n, channel, out*3);
		}
		/*MIXER*/
		scope.log(FilterOut);
	}
}

void cleanup(BelaContext *context, void *userData)
{
}
