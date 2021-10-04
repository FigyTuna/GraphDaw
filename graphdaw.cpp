/* graphdaw.cpp */

#include "graphdaw.h"

#include "core/math/math_funcs.h"
#include "core/math/math_defs.h"
#include "core/print_string.h"

//------------------------------------------------------------------------------

float Osc::gen_sample(float hz) {
  float wave = 0.0;
  switch (wave_type) {
  case SINE_WAVE:
    wave = sin(phase * Math_TAU);
    break;
  case SQUARE_WAVE:
    wave = phase >= 0.5 ? -1.0 : 1.0;
    break;
  case TRIANGLE_WAVE:
    wave = abs(abs(phase * 4.0 - 1.0) * (-1.0) + 2.0) - 1.0;
    break;
  case SAW_WAVE:
    wave = (phase * 2) - 1;
    break;
  case NOISE:
    wave = r.randf_range(-1.0, 1.0);
    break;
  }
  float inc = hz / MIX_RATE;
  phase = fmod(phase + inc, 1.0);
  return wave;
}

void Osc::set_phase(float p) {
  phase = p;
}

Osc::Osc() {
  r.randomize();
}

//------------------------------------------------------------------------------

void Glide::start_glide(uint64_t pos, float target) {
  init_val = get_value(pos);
  target_val = target;
  attack_t = pos;
}

float Glide::get_value(uint64_t pos) {
  float val = target_val;
  float elapsed = float(pos - attack_t) * (1000.0 / MIX_RATE);
  if (length > 0.0 && elapsed < length)
    val = (elapsed / length) * (val - init_val) + init_val;
  return val;
}

Glide::Glide(float l) : length(l) {
}

//------------------------------------------------------------------------------

void LFO::_bind_methods() {
  ClassDB::bind_method(D_METHOD("reset", "init_phase"), &LFO::reset);
  ClassDB::bind_method(D_METHOD("set_wave_type", "wave_type"), &LFO::set_wave_type);
  ClassDB::bind_method(D_METHOD("set_hz", "pos", "hz"), &LFO::set_hz);
  ClassDB::bind_method(D_METHOD("set_amp", "pos", "amp"), &LFO::set_amp);
  ClassDB::bind_method(D_METHOD("get_value", "pos"), &LFO::get_value);
}

void LFO::reset(float init_phase) {
  osc.set_phase(init_phase);
}

void LFO::set_wave_type(int wave_type) {
  osc.wave_type = wave_type;
}

void LFO::set_hz(uint64_t pos, float hz) {
  hz_glide.start_glide(pos, hz);
}

void LFO::set_amp(uint64_t pos, float amp) {
  amp_glide.start_glide(pos, amp);
}

float LFO::get_value(uint64_t pos) {
  return ((osc.gen_sample(hz_glide.get_value(pos)) + 1) / 2) * amp_glide.get_value(pos);
}

//------------------------------------------------------------------------------

void Env::start_attack(uint64_t pos, float vel) {
  init_val = get_value(pos);
  velocity = vel;
  attack_t = pos;
  held = true;
}

void Env::start_release(uint64_t pos) {
  inter_val = get_value(pos);
  release_t = pos;
  held = false;
}

float Env::get_value(uint64_t pos) {
  float amp = 0.0;
  float elapsed = (pos - attack_t) * (1000.0 / MIX_RATE);
  float elapsed_stop = (pos - release_t) * (1000.0 / MIX_RATE);
  if (held && elapsed < attack) {
    amp = (elapsed / attack) * (velocity - init_val) + init_val;
  }  else if (held && elapsed < attack + decay) {
    float low = velocity * sustain;
    float gap = velocity - low;
    amp = ((1 - ((elapsed - attack) / decay)) * gap) + low;
  } else if (held) {
    amp = velocity * sustain;
  } else if (elapsed_stop < decay) {
    amp = (1 - (elapsed_stop / decay)) * inter_val;
  }
  return amp;
}

//------------------------------------------------------------------------------

float Voice::get_hz(uint64_t pos) {
  float hz = pitch.get_value(pos);
  float vib = vibrato.get_value(pos);
  return hz + (hz / 16.8) * (vib * vib * 2.0 - 1.0);
}

float Voice::get_env(uint64_t pos) {
  return env.get_value(pos);
}

float Voice::get_amp(uint64_t pos) {
  return env.get_value(pos) * volume.get_value(pos);
}

void Voice::note_on(uint64_t pos, int note, float velocity) {
  pitch.start_glide(pos, 440.0 * pow(2.0, (note - 69.0) / 12.0));
  env.start_attack(pos, velocity);
}

void Voice::note_off(uint64_t pos) {
  env.start_release(pos);
}

void Voice::set_volume(uint64_t pos, float value) {
  volume.start_glide(pos, value * value);
}

void Voice::set_attack(float value) {
  env.attack = value * value * 995.0 + 5.0;
}

void Voice::set_decay(float value) {
  env.decay = value * value * 995.0 + 5.0;
}

void Voice::set_sustain(float value) {
  env.sustain = value * value;
}

void Voice::set_glide(float value) {
  pitch.length = value * value * 2000.0;
}

void Voice::set_vibrato(uint64_t pos, float value) {
  float v = value * value;
  vibrato.set_amp(pos, v);
  vibrato.set_hz(pos, v * 3 + 4);
}

//------------------------------------------------------------------------------

void FMASound::reset() {
  pos = 0;
  for (int i = 0; i < PARTIALS; ++i)
    oscs[i].set_phase(0.0);
  mod.set_phase(0.0);
  voice.note_off(0.0);
  for (int i = 0; i < PARTIALS - 1; ++i)
    partials_lfos[i].reset();
}

void FMASound::gen_sound(int32_t *pcm_buf, int size) {
  for (int s = 0; s < size; s++) {
    float hz = voice.get_hz(pos);
    float m = mod.gen_sample(hz * fm_ratio);
    float sample = oscs[0].gen_sample(hz + m * fm_amp);
    for (int i = 1; i < PARTIALS; ++i)
      sample += oscs[i].gen_sample(hz * (i + 1))
	* (1 - partials_lfos[i - 1].get_value(pos))
	* get_partial_vol(i);
    pcm_buf[s] += AMP_T * voice.get_amp(pos) * sample;
    pos += 1;
  }
}

float FMASound::get_env() {
  return voice.get_env(pos);
}

void FMASound::set_base_wave_type(int t) {
  if (t >= 0 && t < 5) {
    oscs[0].wave_type = t;
    mod.wave_type = t;
  }
}

void FMASound::set_partials_wave_type(int t) {
  if (t >= 0 && t < 5)
    for (int i = 1; i < PARTIALS; ++i)
      oscs[i].wave_type = t;
}

void FMASound::set_partials(float value) {
  partials_glide.start_glide(pos, value);
}

float FMASound::get_partial_vol(int p) {
  if (p == 0)
    return 1.0;
  float region = 1.0 / float(PARTIALS - 1);
  float begin = region * float(p - 1);
  float end = region * float(p);
  float value = partials_glide.get_value(pos);
  float ret = 0.0;
  if (value >= begin) {
    if (value < end) {
      ret = (value - begin) * (PARTIALS - 1);
    }
    else
      ret = 1.0;
  }
  return ret * 0.7;
}

void FMASound::set_partials_wobble(float value) {
  for (int i = 0; i < PARTIALS - 1; ++i)
    partials_lfos[i].set_amp(pos, value);
}

void FMASound::set_fm_ratio(float value) {
  fm_ratio = value * 2.0;
}

void FMASound::set_fm_amp(float value) {
  fm_amp = value * value * 600.0;
}

void FMASound::note_on(int note, float velocity) {
  voice.note_on(pos, note, velocity);
}

void FMASound::note_off() {
  voice.note_off(pos);
}

void FMASound::set_volume(float value) {
  voice.set_volume(pos, value);
}

void FMASound::set_attack(float value) {
  voice.set_attack(value);
}

void FMASound::set_decay(float value) {
  voice.set_decay(value);
}

void FMASound::set_sustain(float value) {
  voice.set_sustain(value);
}

void FMASound::set_glide(float value) {
  voice.set_glide(value);
}

void FMASound::set_vibrato(float value) {
  voice.set_vibrato(pos, value);
}

FMASound::FMASound() {
  for (int i = 0; i < PARTIALS - 1; ++i)
    partials_lfos[i].set_hz(0, ((float)PARTIALS - (float)i) / 5.0);
}

//------------------------------------------------------------------------------

void FMASynthStream::_bind_methods() {
  ClassDB::bind_method(D_METHOD("reset"), &FMASynthStream::reset);
  ClassDB::bind_method(D_METHOD("get_stream_name"), &FMASynthStream::get_stream_name);
  ClassDB::bind_method(D_METHOD("note_on", "note", "velocity"), &FMASynthStream::note_on);
  ClassDB::bind_method(D_METHOD("note_off", "note"), &FMASynthStream::note_off);
  ClassDB::bind_method(D_METHOD("set_param", "param", "value"), &FMASynthStream::set_param);
  ClassDB::bind_method(D_METHOD("get_env"), &FMASynthStream::get_env);
}

Ref<AudioStreamPlayback> FMASynthStream::instance_playback() {
  Ref<InstrumentStreamPlayback> playback;
  playback.instance();
  playback->base = Ref<FMASynthStream>(this);
  return playback;
}

void FMASynthStream::reset() {
  set_position(0);
  for (int i = 0; i < POLYPHONY; ++i) {
    voices[i].reset();
    poly[i] = -1;
  }
}

void FMASynthStream::set_position(uint64_t p) {
  pos = p;
}

int FMASynthStream::choose_voice(int note) {
  for (int i = 0; i < POLYPHONY; ++i) 
    if (poly[i] == note) {
      return i;
    }
  for (int i = 0; i < POLYPHONY; ++i)
    if (poly[i] == -1)
      return i;
  return 0;
}

void FMASynthStream::gen_sound(int32_t *pcm_buf, int size) {
  for (int i = 0; i < POLYPHONY; ++i)
    voices[i].gen_sound(pcm_buf, size);
  pos += size;
}

String FMASynthStream::get_stream_name() const {
  return "Additive Synth";
}

void FMASynthStream::note_on(int note, float velocity) {
  if (velocity == 0.0)
    note_off(note);
  else {
    int voice = choose_voice(note);
    voices[voice].note_on(note, velocity);
    poly[voice] = note;
  }
}

void FMASynthStream::note_off(int note) {
  int voice = choose_voice(note);
  voices[voice].note_off();
  poly[voice] = -1;
}

void FMASynthStream::set_param(int param, float value) {
  for (int i = 0; i < POLYPHONY; ++i) {
    switch (param) {
    case FMA_VOL:
      voices[i].set_volume(value);
      break;
    case FMA_VIBRATO:
      voices[i].set_vibrato(value);
      break;
    case FMA_PARTIALS:
      voices[i].set_partials(value);
      break;
    case FMA_PARTIALS_WOBBLE:
      voices[i].set_partials_wobble(value);
      break;
    case FMA_BASE_WAVE_TYPE:
      voices[i].set_base_wave_type((int)value);
      break;
    case FMA_PARTIALS_WAVE_TYPE:
      voices[i].set_partials_wave_type((int)value);
      break;
    case FMA_FM_RATIO:
      voices[i].set_fm_ratio(value);
      break;
    case FMA_FM_AMP:
      voices[i].set_fm_amp(value);
      break;
    case FMA_GLIDE:
      voices[i].set_glide(value);
      break;
    case FMA_ATTACK:
      voices[i].set_attack(value);
      break;
    case FMA_DECAY:
      voices[i].set_decay(value);
      break;
    case FMA_SUSTAIN:
      voices[i].set_sustain(value);
      break;
    default:
      print_line("GRAPHDAW ERROR: Unknown param");
      break;
    }
  }
}

float FMASynthStream::get_env() {
  float ret = 0.0;
  for (int i = 0; i < POLYPHONY; ++i) {
    float e = voices[i].get_env();
    if (e > ret)
      ret = e;
  }
  return ret;
}

FMASynthStream::FMASynthStream() : mix_rate(44100) {
  for (int i = 0; i < POLYPHONY; ++i)
    poly[i] = -1;
}

//------------------------------------------------------------------------------

InstrumentStreamPlayback::InstrumentStreamPlayback()
                : active(false) {
  AudioServer::get_singleton()->lock();
  pcm_buffer = AudioServer::get_singleton()->audio_data_alloc(PCM_BUFFER_SIZE);
  zeromem(pcm_buffer, PCM_BUFFER_SIZE);
  AudioServer::get_singleton()->unlock();
}

InstrumentStreamPlayback::~InstrumentStreamPlayback() {
  if(pcm_buffer) {
    AudioServer::get_singleton()->audio_data_free(pcm_buffer);
    pcm_buffer = NULL;
  }
}

void InstrumentStreamPlayback::stop() {
  active = false;
  base->reset();
}

void InstrumentStreamPlayback::start(float p_from_pos) {
  seek(p_from_pos);
  active = true;
}

void InstrumentStreamPlayback::seek(float p_time) {
  //float max = get_length();
  if (p_time < 0) {
    p_time = 0;
  }
  base->set_position(uint64_t(p_time * base->mix_rate) << MIX_FRAC_BITS);
}

void InstrumentStreamPlayback::mix(AudioFrame *p_buffer, float p_rate, int p_frames) {
  if (p_frames > 1024)
    return;
  ERR_FAIL_COND(!active);
  if (!active) {
    return;
  }
  zeromem(pcm_buffer, PCM_BUFFER_SIZE);
  int32_t *buf = (int32_t *)pcm_buffer;
  base->gen_sound(buf, p_frames);

  for(int i = 0; i < p_frames; i++) {
    float sample = float(buf[i]) / AMP_T;
    p_buffer[i] = AudioFrame(sample, sample);
  }
}

int InstrumentStreamPlayback::get_loop_count() const {
  return 0;
}

float InstrumentStreamPlayback::get_playback_position() const {
  return 0.0;
}

float InstrumentStreamPlayback::get_length() const {
  return 0.0;
}

bool InstrumentStreamPlayback::is_playing() const {
  return active;
}
