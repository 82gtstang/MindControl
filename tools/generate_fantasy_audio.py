"""Generate original royalty-free game audio as PCM WAV files."""
import math
import random
import struct
import wave
from pathlib import Path
import sys

RATE = 22050
TAU = math.tau
OUT = Path(sys.argv[1] if len(sys.argv) > 1 else "generated_audio")
OUT.mkdir(parents=True, exist_ok=True)

def write(name, samples):
    with wave.open(str(OUT / (name + ".wav")), "wb") as f:
        f.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
        f.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, x)) * 30000)) for x in samples))

def note(buf, start, duration, midi, volume, flute=False):
    hz = 440 * 2 ** ((midi - 69) / 12)
    for i in range(int(duration * RATE)):
        t = i / RATE
        j = int(start * RATE) + i
        if j >= len(buf):
            break
        env = min(1, t / (.12 if flute else .012)) * min(1, (duration-t)/.18)
        if flute:
            p = TAU * hz * t + .15 * math.sin(TAU * 4.5 * t)
            sound = math.sin(p) + .16 * math.sin(2*p)
        else:
            env *= math.exp(-2.8*t)
            p = TAU * hz * t
            sound = math.sin(p) + .32*math.sin(2*p) + .12*math.sin(3*p)
        buf[j] += volume * env * sound

def music():
    beat = .85
    chords = [(50,57,62,65), (48,55,60,64), (46,53,58,62), (48,55,60,64),
              (50,57,62,65), (53,60,65,69), (48,55,60,64), (50,57,62,65)]
    melody = [(69,65,67,62),(64,67,69,67),(65,62,60,62),(64,67,64,60),
              (62,65,69,72),(69,67,65,60),(64,67,69,64),(65,64,62,62)]
    buf = [0.] * int(64 * beat * RATE)
    for repeat in range(2):
        for bar, chord in enumerate(chords):
            start = (repeat*32 + bar*4)*beat
            for k in range(8):
                note(buf,start+k*beat/2,1.6,chord[k%4],.075)
            for k,pitch in enumerate(melody[bar]):
                note(buf,start+k*beat,.78,pitch,.065,True)
    for i in range(len(buf)):
        buf[i] *= min(1,i/(RATE*.35),(len(buf)-i)/(RATE*.5))
    write("FantasyExploration",buf)

def effect(name, duration, freq, noise, decay):
    rng = random.Random(name)
    buf=[]
    low=0.
    for i in range(int(duration*RATE)):
        t=i/RATE
        low=.8*low+.2*rng.uniform(-1,1)
        env=min(1,t/.008)*math.exp(-decay*t)*min(1,(duration-t)/.03)
        tone=math.sin(TAU*freq*(t-.35*t*t/duration))
        buf.append(.45*env*(noise*low+(1-noise)*tone))
    write(name,buf)

if __name__ == "__main__":
    music()
    for args in [("Footstep",.25,85,.8,12),("Punch",.22,100,.55,16),
                 ("SwordSwing",.4,380,.92,5),("SwordImpact",.45,720,.55,8),
                 ("Splash",.7,140,.95,4),("SwimStroke",.55,90,.95,5)]:
        effect(*args)
