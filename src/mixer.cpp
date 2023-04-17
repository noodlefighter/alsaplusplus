#include <alsaplusplus/mixer.hpp>

using namespace AlsaPlusPlus;

constexpr long MINIMAL_VOLUME = 0;
constexpr long MAXIMUM_VOLUME = 65535;

Mixer::Mixer(std::string hw_device, std::string element_name, bool not_volume) :
  err(0),
  device_name(hw_device),
  simple_elem_name(element_name)
{
  if ((err = snd_mixer_open(&mixer_handle, 0)) < 0)
    handle_error_code(err, true, "Cannot open handle to mixer device.");

  if ((err = snd_mixer_attach(mixer_handle, device_name.c_str())) < 0)
    handle_error_code(err, true, "Cannot attach mixer to device.");

  if ((err = snd_mixer_selem_register(mixer_handle, NULL, NULL)) < 0)
    handle_error_code(err, true, "Cannot register simple mixer object.");

  if ((err = snd_mixer_load(mixer_handle)) < 0)
    handle_error_code(err, true, "Cannot load sound mixer.");

  snd_mixer_selem_id_alloca(&simple_mixer_handle);
  snd_mixer_selem_id_set_index(simple_mixer_handle, 0);
  snd_mixer_selem_id_set_name(simple_mixer_handle, simple_elem_name.c_str());
  element_handle = snd_mixer_find_selem(mixer_handle, simple_mixer_handle);

  if (element_handle == NULL)
  {
    std::ostringstream oss;
    oss << "Could not find simple mixer element named " << simple_elem_name << ".";
    handle_error_code(static_cast<int>(std::errc::argument_out_of_domain), true, oss.str());
  }

  if (!not_volume) {
    if ((err = snd_mixer_selem_set_playback_volume_range (element_handle, MINIMAL_VOLUME, MAXIMUM_VOLUME)) < 0)
      handle_error_code(err, true, "Cannot set element volume range.");
  }
}

Mixer::~Mixer()
{
  snd_mixer_close(mixer_handle);
}

bool Mixer::device_exists(std::string hw_device)
{
  int err;
  snd_mixer_t* temp_handle;

  if ((err = snd_mixer_open(&temp_handle, 0)) < 0)
  {
    handle_error_code(err, false, "Cannot open handle to a mixer device.");
    return false;
  }

  if ((err = snd_mixer_attach(temp_handle, hw_device.c_str())) < 0)
  {
    snd_mixer_close(temp_handle);
    return false;
  }

  snd_mixer_close(temp_handle);
  return true;
}

bool Mixer::element_exists(std::string hw_device, std::string element_name)
{
  int err;
  snd_mixer_t* temp_handle;
  snd_mixer_selem_id_t* simple_temp_handle;

  if ((err = snd_mixer_open(&temp_handle, 0)) < 0)
  {
    handle_error_code(err, false, "Cannot open handle to a mixer device.");
    return false;
  }

  if ((err = snd_mixer_attach(temp_handle, hw_device.c_str())) < 0)
  {
    handle_error_code(err, true, "Cannot attach mixer to device.");
    snd_mixer_close(temp_handle);
    return false;
  }

  if ((err = snd_mixer_selem_register(temp_handle, NULL, NULL)) < 0)
  {
    handle_error_code(err, true, "Cannot register simple mixer object.");
    snd_mixer_close(temp_handle);
    return false;
  }

  if ((err = snd_mixer_load(temp_handle)) < 0)
  {
    handle_error_code(err, true, "Cannot load sound mixer.");
    snd_mixer_close(temp_handle);
    return false;
  }

  snd_mixer_selem_id_alloca(&simple_temp_handle);
  snd_mixer_selem_id_set_index(simple_temp_handle, 0);
  snd_mixer_selem_id_set_name(simple_temp_handle, element_name.c_str());
  snd_mixer_elem_t* elem_handle = snd_mixer_find_selem(temp_handle, simple_temp_handle);

  if (elem_handle == NULL)
  {
    snd_mixer_close(temp_handle);
    return false;
  }

  snd_mixer_close(temp_handle);
  return true;
}


float Mixer::dec_vol_pct(float pct, snd_mixer_selem_channel_id_t channel)
{
  trim_pct(pct);
  float cur_vol = get_cur_vol_pct(channel);
  return set_vol_pct(cur_vol - pct);
}

float Mixer::inc_vol_pct(float pct, snd_mixer_selem_channel_id_t channel)
{
  trim_pct(pct);
  float cur_vol = get_cur_vol_pct(channel);
  return set_vol_pct(cur_vol + pct);
}

float Mixer::set_vol_pct(float pct)
{
  long min, max;

  trim_pct(pct);
  get_vol_range(&min, &max);
  set_vol_raw((long)((float)min + (pct * (max - min))));
  return get_cur_vol_pct();
}

float Mixer::get_cur_vol_pct(snd_mixer_selem_channel_id_t channel)
{
  long min, max, cur;
  get_vol_range(&min, &max);
  cur = get_cur_vol_raw(channel);
  return round((float)cur / (max - min) * 100.0) / 100.0;
}

float Mixer::mute()
{
  mute_vol = get_cur_vol_raw();
  return set_vol_pct(0);
}

float Mixer::unmute()
{
  set_vol_raw(mute_vol);
  return get_cur_vol_pct();
}

void Mixer::trim_pct(float& pct)
{
  pct = (pct < 0) ? 0 : pct;
  pct = (pct > 1) ? 1 : pct;
}

void Mixer::set_vol_raw(long vol)
{
  err = snd_mixer_selem_set_playback_volume_all(element_handle, vol);

  if (err < 0)
    handle_error_code(err, false, "Cannot set volume to requested value.");
}

long Mixer::get_cur_vol_raw(snd_mixer_selem_channel_id_t channel)
{
  long cur_vol;

  err = snd_mixer_selem_get_playback_volume(element_handle, channel, &cur_vol);

  if (err < 0)
    handle_error_code(err, false, "Could not get volume for provided channel.");

  return cur_vol;
}

void Mixer::get_vol_range(long* min_vol, long* max_vol)
{
  err = snd_mixer_selem_get_playback_volume_range(element_handle, min_vol, max_vol);

  if (err < 0)
    handle_error_code(err, false, "Cannot get min/max volume range.");
}

static int get_enum_item_index(snd_mixer_elem_t *elem, const char **ptrp)
{
  const char *ptr = *ptrp;
  int items, i, len;

  /* See snd_ctl_elem_init_enum_names() in sound/core/control.c. */
  char name[64];

  items = snd_mixer_selem_get_enum_items(elem);
  if (items <= 0)
    return -1;

  for (i = 0; i < items; i++) {
    if (snd_mixer_selem_get_enum_item_name(elem, i, sizeof(name)-1, name) < 0)
      continue;

    len = strlen(name);
    if (! strncmp(name, ptr, len)) {
      if (! ptr[len] || ptr[len] == ',' || ptr[len] == '\n') {
        ptr += len;
        *ptrp = ptr;
        return i;
      }
    }
  }
  return -1;
}
void Mixer::set_enum_item(std::string item_name)
{
  unsigned int item = 0;
  const char *ptr = item_name.c_str();
  while (*ptr) {
    int ival = get_enum_item_index(element_handle, &ptr);
    if (ival < 0)
      handle_error_code(err, true, "Cannot find enum item..");

    if (snd_mixer_selem_set_enum_item(element_handle, (snd_mixer_selem_channel_id_t)item++, ival) < 0)
      handle_error_code(err, true, "Cannot set enum item..");

    /* skip separators */
    while (*ptr == ',' || isspace(*ptr))
      ptr++;
  }
}
