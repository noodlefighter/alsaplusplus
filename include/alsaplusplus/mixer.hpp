#ifndef ALSAPLUSPLUS_MIXER_HPP
#define ALSAPLUSPLUS_MIXER_HPP

#include <alsaplusplus/common.hpp>
#include <alsa/control.h>
#include <alsa/pcm.h>
#include <alsa/mixer.h>

#include <cmath>

namespace AlsaPlusPlus
{
  class Mixer
  {
    public:
      Mixer(std::string hw_device, std::string element_name, bool not_volume = false);
      ~Mixer();

      static bool device_exists(std::string hw_device);
      static bool element_exists(std::string hw_device, std::string element_name);
      float inc_vol_pct(float pct, snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_MONO);
      float dec_vol_pct(float pct, snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_MONO);
      float set_vol_pct(float pct);
      float get_cur_vol_pct(snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_MONO);
      float mute();
      float unmute();
      void set_enum_item(std::string item);

    private:
      int err;
      std::string device_name;
      snd_mixer_t* mixer_handle;
      snd_mixer_selem_id_t* simple_mixer_handle;
      std::string simple_elem_name;
      snd_mixer_elem_t* element_handle;
      long mute_vol;

      void trim_pct(float& pct);
      void set_vol_raw(long vol);
      long get_cur_vol_raw(snd_mixer_selem_channel_id_t channel = SND_MIXER_SCHN_MONO);
      void get_vol_range(long* min_vol, long* max_vol);
  };
}

#endif
