/* graphdaw.h */

#ifndef GRAPHDAW_H
#define GRAPHDAW_H

#include "core/reference.h"
#include "core/resource.h"
#include "servers/audio/audio_stream.h"
#include "core/math/random_number_generator.h"

#define MIX_RATE 44100
#define AMP_T 2147483647.0

//------------------------------------------------------------------------------

class Osc : public Reference{
  GDCLASS(Osc, Reference);

private:
  float phase = 0.0;
  RandomNumberGenerator r;

public:
  enum {
    SINE_WAVE,
    SQUARE_WAVE,
    TRIANGLE_WAVE,
    SAW_WAVE,
    NOISE
  };

  int wave_type = 0;

  void set_phase(float p);

  float gen_sample(float hz);

  Osc();

};

//------------------------------------------------------------------------------

class Glide : public Reference{
  GDCLASS(Glide, Reference);

private:

  uint64_t attack_t = 0;

  float init_val = 0.0;
  float target_val = 0.0;

public:

  float length;

  void start_glide(uint64_t pos, float target);
  float get_value(uint64_t pos);

  Glide(float l);
};

//------------------------------------------------------------------------------

class LFO : public Reference{
  GDCLASS(LFO, Reference);

private:
  Osc osc;
  Glide hz_glide {10.0};
  Glide amp_glide {10.0};

protected:
  static void _bind_methods();

public:
  void reset(float init_phase = 0.0);
  void set_wave_type(int wave_type);
  void set_hz(uint64_t pos, float hz);
  void set_amp(uint64_t pos, float amp);
  float get_value(uint64_t pos);
};

//------------------------------------------------------------------------------

class Env : public Reference{
  GDCLASS(Env, Reference);

private:

  bool held = false;

  float velocity = 0.0;
  
  uint64_t attack_t = 0;
  uint64_t release_t = 0;

  float init_val = 0.0;
  float inter_val = 0.0;

public:

  float attack = 5.0;
  float decay = 5.0;
  float sustain = 1.0;

  void start_attack(uint64_t pos, float velocity);
  void start_release(uint64_t pos);
  float get_value(uint64_t pos);

};

//------------------------------------------------------------------------------

class Voice : public Reference {
  GDCLASS(Voice, Reference);

private:
  Glide pitch {0.0};
  Glide volume {10.0};
  LFO vibrato;
  Env env;

public:
  float get_hz(uint64_t pos);
  float get_env(uint64_t pos);
  float get_amp(uint64_t pos);

  bool get_occupied();
  
  void note_on(uint64_t pos, int note, float velocity);
  void note_off(uint64_t pos);

  void set_volume(uint64_t pos, float value);

  void set_attack(float value);
  void set_decay(float value);
  void set_sustain(float value);
  
  void set_glide(float value);
  void set_vibrato(uint64_t pos, float value);

};

//------------------------------------------------------------------------------

class FMASound : public Reference {
  GDCLASS(FMASound, Reference);

private:
  friend class InstrumentStream;

  enum {
    PARTIALS = 5
  };

  uint64_t pos = 0;
  Osc oscs[PARTIALS];
  Osc mod;
  Voice voice;
  
  LFO partials_lfos[PARTIALS - 1];
  Glide partials_glide {10.0};

  float fm_ratio = 0.0;
  float fm_amp = 0.0;

public:

  void reset();

  void gen_sound(int32_t *pcm_buf, int size);

  float get_env();

  void set_partials(float amount);
  float get_partial_vol(int p);
  void set_partials_wobble(float amount);
  
  void set_fm_ratio(float amount);
  void set_fm_amp(float amount);

  void set_base_wave_type(int t);
  void set_partials_wave_type(int t);
  
  void note_on(int note, float velocity);
  void note_off();

  void set_volume(float value);

  void set_attack(float value);
  void set_decay(float value);
  void set_sustain(float value);
  
  void set_glide(float value);
  void set_vibrato(float value);

  FMASound();
};

//------------------------------------------------------------------------------

class FMASynthStream : public AudioStream {
  GDCLASS(FMASynthStream, AudioStream);

private:
  int mix_rate;
  uint64_t pos = 0;
  enum {
    POLYPHONY = 4
  };
  friend class InstrumentStreamPlayback;
  friend class FMASound;
  FMASound voices[POLYPHONY];
  int poly[POLYPHONY];

  int choose_voice(int note);
  
protected:
  static void _bind_methods();

public:
  enum {
    FMA_VOL,
    FMA_VIBRATO,
    FMA_PARTIALS,
    FMA_PARTIALS_WOBBLE,
    FMA_BASE_WAVE_TYPE,
    FMA_PARTIALS_WAVE_TYPE,
    FMA_FM_RATIO,
    FMA_FM_AMP,
    FMA_GLIDE,
    FMA_ATTACK,
    FMA_DECAY,
    FMA_SUSTAIN
  };
  void reset();
  void set_position(uint64_t pos);
  virtual Ref<AudioStreamPlayback> instance_playback();
  virtual String get_stream_name() const;
  virtual void gen_sound(int32_t *pcm_buf, int size);
  virtual float get_length() const { return 0; }

  void note_on(int note, float velocity);
  void note_off(int note);
  void set_param(int param, float value);
  float get_env();
  
  FMASynthStream();

};

//------------------------------------------------------------------------------

class InstrumentStreamPlayback : public AudioStreamPlayback {
  GDCLASS(InstrumentStreamPlayback, AudioStreamPlayback)

private:
  enum {
    PCM_BUFFER_SIZE = 16384
  };
  enum {
    MIX_FRAC_BITS = 13,
    MIX_FRAC_LEN = (1 << MIX_FRAC_BITS),
    MIX_FRAC_MASK = MIX_FRAC_LEN - 1,
  };
  friend class FMASynthStream;
  void *pcm_buffer;
  Ref<FMASynthStream> base;
  bool active;

public:
  virtual void start(float p_from_pos = 0.0);
  virtual void stop();
  virtual bool is_playing() const;
  virtual int get_loop_count() const; // times it looped
  virtual float get_playback_position() const;
  virtual void seek(float p_time);
  virtual void mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames);
  virtual float get_length() const; // if supported, otherwise return 0
  InstrumentStreamPlayback();
  ~InstrumentStreamPlayback();
};

#endif // GRAPHDAW_H
