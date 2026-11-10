#pragma once
// Sintetizador simples de pré-escuta (2 serras ligeiramente desafinadas + filtro suave).
#include <juce_audio_basics/juce_audio_basics.h>

struct HarmSound : juce::SynthesiserSound {
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class HarmVoice : public juce::SynthesiserVoice {
public:
    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<HarmSound*>(s) != nullptr; }

    void startNote(int midiNote, float velocity, juce::SynthesiserSound*, int) override {
        freq = juce::MidiMessage::getMidiNoteInHertz(midiNote);
        level = velocity * 0.14f;
        p1 = 0.0; p2 = 0.37; lp = 0.0f;
        coef = juce::jlimit(0.04f, 0.5f, (float) (freq / 4000.0 + 0.05));
        adsr.setSampleRate(getSampleRate());
        adsr.setParameters({0.01f, 0.25f, 0.7f, 0.3f});
        adsr.noteOn();
    }
    void stopNote(float, bool allowTailOff) override {
        if (allowTailOff) adsr.noteOff();
        else { adsr.reset(); clearCurrentNote(); }
    }
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& out, int start, int num) override {
        if (!adsr.isActive()) { clearCurrentNote(); return; }
        const double sr = getSampleRate();
        for (int i = 0; i < num; ++i) {
            float s = (float) ((2.0 * p1 - 1.0) + (2.0 * p2 - 1.0)) * 0.5f;
            lp += coef * (s - lp);
            float v = lp * adsr.getNextSample() * level;
            for (int ch = 0; ch < out.getNumChannels(); ++ch) out.addSample(ch, start + i, v);
            p1 += freq / sr; if (p1 >= 1.0) p1 -= 1.0;
            p2 += freq * 1.005 / sr; if (p2 >= 1.0) p2 -= 1.0;
            if (!adsr.isActive()) { clearCurrentNote(); break; }
        }
    }

private:
    juce::ADSR adsr;
    double freq = 440.0, p1 = 0.0, p2 = 0.0;
    float level = 0.1f, lp = 0.0f, coef = 0.1f;
};
