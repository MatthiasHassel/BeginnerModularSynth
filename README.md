# BeginnerModularSynth
DIY Modular Synth based on the Bela Board

I built this DIY Modular Synth for my bachelor thesis in February 2022. It is based on the Bela Board from the Augmented Instruments Laboratory in London and Eurorack compatible, where only voltages between 0V and +5V are evaluated correctly at the moment. However voltages beyond that limit won't harm the hardware and will be interpreted as 0V or +5V

With a "Default Patch" option it is possible to activate a fixed signal flow, where no patching is needed.

A lot of the code is inspired by materials based on a Master’s module “Music and Audio Programming”, taught by Dr Andrew McPherson in the School of Electronic Engineering and Computer Science. Thank you for making this lecture accessible for the public! 

C++ Realtime Programming with Bela:
https://www.youtube.com/watch?v=aVLRUyPBBJk&list=PLCrgFeG6pwQmdbB6l3ehC8oBBZbatVoz3


The Audio itself is not patchable for now, meaning the Audio signal flow is always Oscillator Out -> Filter In -> Filter Out -> Mixer In -> Mixer Out
For a next prototype however it is planned to change this to complete the modularity.

It's still a "work in progress" thing so I am happy about feedback and improvements!
