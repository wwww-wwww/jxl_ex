#include "erl_nif.h"

#include <cstring>
#include <vector>

#include <jxl/codestream_header.h>
#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#include "jxl/thread_parallel_runner_cxx.h"

static ERL_NIF_TERM dec_create_nif(ErlNifEnv* env, int argc,
                                   const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM dec_load_data_nif(ErlNifEnv* env, int argc,
                                      const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM dec_basic_info_nif(ErlNifEnv* env, int argc,
                                       const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM dec_icc_profile_nif(ErlNifEnv* env, int argc,
                                        const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM dec_frame_nif(ErlNifEnv* env, int argc,
                                  const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM dec_keep_orientation_nif(ErlNifEnv* env, int argc,
                                             const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM dec_reset_nif(ErlNifEnv* env, int argc,
                                  const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM dec_rewind_nif(ErlNifEnv* env, int argc,
                                   const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM dec_skip_nif(ErlNifEnv* env, int argc,
                                 const ERL_NIF_TERM argv[]);

static ERL_NIF_TERM gray8_to_rgb8_nif(ErlNifEnv* env, int argc,
                                      const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM rgb8_to_gray8_nif(ErlNifEnv* env, int argc,
                                      const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM add_alpha8_nif(ErlNifEnv* env, int argc,
                                   const ERL_NIF_TERM argv[]);
static ERL_NIF_TERM premultiply_alpha8_nif(ErlNifEnv* env, int argc,
                                           const ERL_NIF_TERM argv[]);

static int nif_load(ErlNifEnv* env, void** data, const ERL_NIF_TERM info);
static int upgrade(ErlNifEnv* env, void** priv_data, void** old_priv_data,
                   ERL_NIF_TERM load_info) {
  return 0;
};

static ErlNifFunc jxl_nif_funcs[] = {
    {"dec_create", 1, dec_create_nif, 0},
    {"dec_load_blob", 2, dec_load_data_nif, 0},
    {"dec_basic_info", 1, dec_basic_info_nif, 0},
    {"dec_icc_profile", 1, dec_icc_profile_nif, 0},
    {"dec_frame", 1, dec_frame_nif, ERL_NIF_DIRTY_JOB_CPU_BOUND},
    {"dec_keep_orientation", 1, dec_keep_orientation_nif, 0},
    {"dec_reset", 1, dec_reset_nif, 0},
    {"dec_rewind", 1, dec_rewind_nif, 0},
    {"dec_skip", 2, dec_skip_nif, 0},
    {"gray8_to_rgb8", 2, gray8_to_rgb8_nif, 0},
    {"rgb8_to_gray8", 2, rgb8_to_gray8_nif, 0},
    {"add_alpha8", 2, add_alpha8_nif, 0},
    {"premultiply_alpha8", 2, premultiply_alpha8_nif, 0},
};

ERL_NIF_INIT(Elixir.JxlEx.Base, jxl_nif_funcs, nif_load, NULL, upgrade, NULL)

static ERL_NIF_TERM make_utf8str(ErlNifEnv* env, const char* data);

#define ERROR(e, m) \
  enif_make_tuple2(e, enif_make_atom(env, "error"), make_utf8str(env, m));

#define OK(e, m) enif_make_tuple2(e, enif_make_atom(env, "ok"), m);

#define MAP(env, map, key, value) \
  enif_make_map_put(env, map, enif_make_atom(env, key), value, &map);

struct jxl_dec_resource_t {
  JxlDecoderPtr decoder;
  JxlThreadParallelRunnerPtr runner;
  JxlBasicInfo jxl_info;
  JxlPixelFormat format;
  std::vector<uint8_t> icc_profile;
  bool have_info = false;
  bool have_icc = false;
};

static ERL_NIF_TERM dec_create_nif(ErlNifEnv* env, int argc,
                                   const ERL_NIF_TERM argv[]) {
  int num_threads;
  if (enif_get_int(env, argv[0], &num_threads) == 0) {
    return ERROR(env, "Invalid thread count");
  }
  if (num_threads < 0) {
    return ERROR(env, "Invalid thread count");
  }

  ErlNifResourceType* type = (ErlNifResourceType*)enif_priv_data(env);
  jxl_dec_resource_t* resource = (jxl_dec_resource_t*)enif_alloc_resource(
      type, sizeof(jxl_dec_resource_t));
  new (resource) jxl_dec_resource_t();

  if (resource == NULL) {
    return ERROR(env, "Failed to create resource");
  }

  if (num_threads == 0) {
    num_threads = (int)JxlThreadParallelRunnerDefaultNumWorkerThreads();
  }

  auto dec = JxlDecoderMake(nullptr);
  auto runner = JxlThreadParallelRunnerMake(nullptr, num_threads);

  if (JXL_DEC_SUCCESS !=
      JxlDecoderSubscribeEvents(dec.get(),
                                JXL_DEC_BASIC_INFO | JXL_DEC_COLOR_ENCODING |
                                    JXL_DEC_FRAME | JXL_DEC_FULL_IMAGE)) {
    return ERROR(env, "JxlDecoderSubscribeEvents failed");
  }

  if (JXL_DEC_SUCCESS != JxlDecoderSetParallelRunner(dec.get(),
                                                     JxlThreadParallelRunner,
                                                     runner.get())) {
    return ERROR(env, "JxlDecoderSetParallelRunner failed");
  }

  resource->decoder = std::move(dec);
  resource->runner = std::move(runner);

  ERL_NIF_TERM term = enif_make_resource(env, (void*)resource);
  enif_release_resource(resource);

  return OK(env, term);
}

static ERL_NIF_TERM dec_load_data_nif(ErlNifEnv* env, int argc,
                                      const ERL_NIF_TERM argv[]) {
  ErlNifResourceType* type = (ErlNifResourceType*)enif_priv_data(env);

  jxl_dec_resource_t* resource;
  if (enif_get_resource(env, argv[0], type, (void**)&resource) == 0) {
    return ERROR(env, "Invalid handle");
  }

  ErlNifBinary blob;
  if (enif_inspect_binary(env, argv[1], &blob) == 0) {
    return ERROR(env, "Bad argument");
  }

  JxlDecoderSetInput(resource->decoder.get(), blob.data, blob.size);

  return OK(env, argv[0]);
}

static ERL_NIF_TERM dec_basic_info_nif(ErlNifEnv* env, int argc,
                                       const ERL_NIF_TERM argv[]) {
  ErlNifResourceType* type = (ErlNifResourceType*)enif_priv_data(env);

  jxl_dec_resource_t* resource;
  if (enif_get_resource(env, argv[0], type, (void**)&resource) == 0) {
    return ERROR(env, "Invalid handle");
  }

  if (!resource->have_info) {
    while (true) {
      JxlDecoderStatus status = JxlDecoderProcessInput(resource->decoder.get());
      if (status == JXL_DEC_ERROR) {
        return ERROR(env, "Decode error");
      } else if (status == JXL_DEC_NEED_MORE_INPUT) {
        return ERROR(env, "Need more input");
      } else if (status == JXL_DEC_BASIC_INFO) {
        if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(resource->decoder.get(),
                                                      &resource->jxl_info)) {
          return ERROR(env, "JxlDecoderGetBasicInfo failed");
        }
        int num_channels = resource->jxl_info.num_color_channels +
                           (resource->jxl_info.alpha_bits > 0 ? 1 : 0);
        resource->format = {4, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};
        resource->have_info = true;
        goto have_info;
      } else if (status == JXL_DEC_COLOR_ENCODING) {
        size_t size = 0;
        if (JXL_DEC_SUCCESS != JxlDecoderGetICCProfileSize(
                                   resource->decoder.get(), &resource->format,
                                   JXL_COLOR_PROFILE_TARGET_DATA, &size)) {
          return ERROR(env, "JxlDecoderGetICCProfileSize failed");
        }

        resource->icc_profile.resize(size);
        if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
                                   resource->decoder.get(), &resource->format,
                                   JXL_COLOR_PROFILE_TARGET_DATA,
                                   resource->icc_profile.data(), size)) {
          return ERROR(env, "JxlDecoderGetColorAsICCProfile failed");
        }
        resource->have_icc = true;
      } else {
        return ERROR(env, "Unexpected decoder status");
      }
    }
  }

have_info:
  ERL_NIF_TERM map = enif_make_new_map(env);
  MAP(env, map, "have_container",
      enif_make_int(env, resource->jxl_info.have_container));
  MAP(env, map, "xsize", enif_make_uint(env, resource->jxl_info.xsize));
  MAP(env, map, "ysize", enif_make_uint(env, resource->jxl_info.ysize));
  MAP(env, map, "bits_per_sample",
      enif_make_uint(env, resource->jxl_info.bits_per_sample));
  MAP(env, map, "exponent_bits_per_sample",
      enif_make_uint(env, resource->jxl_info.exponent_bits_per_sample));
  MAP(env, map, "intensity_target",
      enif_make_double(env, resource->jxl_info.intensity_target));
  MAP(env, map, "min_nits", enif_make_double(env, resource->jxl_info.min_nits));
  MAP(env, map, "relative_to_max_display",
      enif_make_int(env, resource->jxl_info.relative_to_max_display));
  MAP(env, map, "linear_below",
      enif_make_double(env, resource->jxl_info.linear_below));
  MAP(env, map, "uses_original_profile",
      enif_make_int(env, resource->jxl_info.uses_original_profile));
  MAP(env, map, "have_preview",
      enif_make_int(env, resource->jxl_info.have_preview));
  MAP(env, map, "have_animation",
      enif_make_int(env, resource->jxl_info.have_animation));
  MAP(env, map, "orientation",
      enif_make_int(env, resource->jxl_info.orientation));
  MAP(env, map, "num_color_channels",
      enif_make_uint(env, resource->jxl_info.num_color_channels));
  MAP(env, map, "num_extra_channels",
      enif_make_uint(env, resource->jxl_info.num_extra_channels));
  MAP(env, map, "alpha_bits",
      enif_make_uint(env, resource->jxl_info.alpha_bits));
  MAP(env, map, "alpha_exponent_bits",
      enif_make_uint(env, resource->jxl_info.alpha_exponent_bits));
  MAP(env, map, "alpha_premultiplied",
      enif_make_int(env, resource->jxl_info.alpha_premultiplied));

  if (resource->jxl_info.have_preview) {
    ERL_NIF_TERM preview = enif_make_new_map(env);
    MAP(env, preview, "xsize",
        enif_make_uint(env, resource->jxl_info.preview.xsize));
    MAP(env, preview, "ysize",
        enif_make_uint(env, resource->jxl_info.preview.ysize));
    MAP(env, map, "preview", preview);
  }

  if (resource->jxl_info.have_animation) {
    ERL_NIF_TERM animation = enif_make_new_map(env);
    MAP(env, animation, "tps_numerator",
        enif_make_uint(env, resource->jxl_info.animation.tps_numerator));
    MAP(env, animation, "tps_denominator",
        enif_make_uint(env, resource->jxl_info.animation.tps_denominator));
    MAP(env, animation, "have_timecodes",
        enif_make_int(env, resource->jxl_info.animation.have_timecodes));
    MAP(env, map, "animation", animation);
  }

  return OK(env, map);
}

static ERL_NIF_TERM dec_icc_profile_nif(ErlNifEnv* env, int argc,
                                        const ERL_NIF_TERM argv[]) {
  ErlNifResourceType* type = (ErlNifResourceType*)enif_priv_data(env);

  jxl_dec_resource_t* resource;
  if (enif_get_resource(env, argv[0], type, (void**)&resource) == 0) {
    return ERROR(env, "Invalid handle");
  }

  if (!resource->have_icc) {
    while (true) {
      JxlDecoderStatus status = JxlDecoderProcessInput(resource->decoder.get());
      if (status == JXL_DEC_ERROR) {
        return ERROR(env, "Decode error");
      } else if (status == JXL_DEC_NEED_MORE_INPUT) {
        return ERROR(env, "Need more input");
      } else if (status == JXL_DEC_BASIC_INFO) {
        if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(resource->decoder.get(),
                                                      &resource->jxl_info)) {
          return ERROR(env, "JxlDecoderGetBasicInfo failed");
        }
        uint32_t num_channels = resource->jxl_info.num_color_channels +
                                (resource->jxl_info.alpha_bits > 0 ? 1 : 0);
        resource->format = {num_channels, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};
        resource->have_info = true;
      } else if (status == JXL_DEC_COLOR_ENCODING) {
        size_t size = 0;
        if (JXL_DEC_SUCCESS != JxlDecoderGetICCProfileSize(
                                   resource->decoder.get(), &resource->format,
                                   JXL_COLOR_PROFILE_TARGET_DATA, &size)) {
          return ERROR(env, "JxlDecoderGetICCProfileSize failed");
        }

        resource->icc_profile.resize(size);
        if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
                                   resource->decoder.get(), &resource->format,
                                   JXL_COLOR_PROFILE_TARGET_DATA,
                                   resource->icc_profile.data(), size)) {
          return ERROR(env, "JxlDecoderGetColorAsICCProfile failed");
        }
        resource->have_icc = true;
        goto have_icc;
      } else {
        return ERROR(env, "Unexpected decoder status");
      }
    }
  }

have_icc:
  ERL_NIF_TERM data;
  unsigned char* raw =
      enif_make_new_binary(env, resource->icc_profile.size(), &data);
  memcpy(raw, resource->icc_profile.data(), resource->icc_profile.size());

  return OK(env, data);
}

static ERL_NIF_TERM dec_frame_nif(ErlNifEnv* env, int argc,
                                  const ERL_NIF_TERM argv[]) {
  ErlNifResourceType* type = (ErlNifResourceType*)enif_priv_data(env);

  jxl_dec_resource_t* resource;
  if (enif_get_resource(env, argv[0], type, (void**)&resource) == 0) {
    return ERROR(env, "Invalid handle");
  }

  std::vector<uint8_t> pixels;
  JxlFrameHeader frame_header;

  while (true) {
    JxlDecoderStatus status = JxlDecoderProcessInput(resource->decoder.get());
    if (status == JXL_DEC_ERROR) {
      return ERROR(env, "Decode error");
    } else if (status == JXL_DEC_NEED_MORE_INPUT) {
      return ERROR(env, "Need more input");
    } else if (status == JXL_DEC_BASIC_INFO) {
      if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(resource->decoder.get(),
                                                    &resource->jxl_info)) {
        return ERROR(env, "JxlDecoderGetBasicInfo failed");
      }
      uint32_t num_channels = resource->jxl_info.num_color_channels +
                              (resource->jxl_info.alpha_bits > 0 ? 1 : 0);
      resource->format = {num_channels, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0};
      resource->have_info = true;
    } else if (status == JXL_DEC_COLOR_ENCODING) {
      size_t size = 0;
      if (JXL_DEC_SUCCESS != JxlDecoderGetICCProfileSize(
                                 resource->decoder.get(), &resource->format,
                                 JXL_COLOR_PROFILE_TARGET_DATA, &size)) {
        return ERROR(env, "JxlDecoderGetICCProfileSize failed");
      }

      resource->icc_profile.resize(size);
      if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
                                 resource->decoder.get(), &resource->format,
                                 JXL_COLOR_PROFILE_TARGET_DATA,
                                 resource->icc_profile.data(), size)) {
        return ERROR(env, "JxlDecoderGetColorAsICCProfile failed");
      }
      resource->have_icc = true;
    } else if (status == JXL_DEC_FRAME) {
      if (resource->jxl_info.have_animation) {
        if (JXL_DEC_SUCCESS !=
            JxlDecoderGetFrameHeader(resource->decoder.get(), &frame_header)) {
          return ERROR(env, "JxlDecoderGetFrameHeader failed");
        }
      }
    } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
      size_t buffer_size;
      if (JXL_DEC_SUCCESS !=
          JxlDecoderImageOutBufferSize(resource->decoder.get(),
                                       &resource->format, &buffer_size)) {
        return ERROR(env, "JxlDecoderImageOutBufferSize failed");
      }
      pixels.resize(buffer_size);
      if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(
                                 resource->decoder.get(), &resource->format,
                                 pixels.data(), buffer_size)) {
        return ERROR(env, "JxlDecoderSetImageOutBuffer failed");
      }
    } else if (status == JXL_DEC_FULL_IMAGE) {
      goto done;
    } else if (status == JXL_DEC_SUCCESS) {
      goto done;
    } else {
      return ERROR(env, "Unexpected decoder status");
    }
  }

done:
  if (pixels.empty()) {
    return ERROR(env, "No image");
  }

  ERL_NIF_TERM map = enif_make_new_map(env);

  ERL_NIF_TERM data;
  unsigned char* raw = enif_make_new_binary(env, pixels.size(), &data);
  memcpy(raw, pixels.data(), pixels.size());

  MAP(env, map, "image", data);
  MAP(env, map, "xsize", enif_make_uint(env, resource->jxl_info.xsize));
  MAP(env, map, "ysize", enif_make_uint(env, resource->jxl_info.ysize));
  MAP(env, map, "bits_per_sample",
      enif_make_uint(env, resource->jxl_info.bits_per_sample));
  MAP(env, map, "num_channels",
      enif_make_uint(env, resource->format.num_channels));

  if (resource->jxl_info.have_animation) {
    MAP(env, map, "duration", enif_make_uint(env, frame_header.duration));
    MAP(env, map, "timecode", enif_make_uint(env, frame_header.timecode));
    MAP(env, map, "is_last", enif_make_int(env, frame_header.is_last));
  }

  return OK(env, map);
}

static ERL_NIF_TERM dec_keep_orientation_nif(ErlNifEnv* env, int argc,
                                             const ERL_NIF_TERM argv[]) {
  ErlNifResourceType* type = (ErlNifResourceType*)enif_priv_data(env);

  jxl_dec_resource_t* resource;
  if (enif_get_resource(env, argv[0], type, (void**)&resource) == 0) {
    return ERROR(env, "Invalid handle");
  }

  if (JXL_DEC_SUCCESS !=
      JxlDecoderSetKeepOrientation(resource->decoder.get(), JXL_TRUE)) {
    return ERROR(env, "JxlDecoderSetKeepOrientation failed");
  }

  return OK(env, argv[0]);
}

static ERL_NIF_TERM dec_reset_nif(ErlNifEnv* env, int argc,
                                  const ERL_NIF_TERM argv[]) {
  ErlNifResourceType* type = (ErlNifResourceType*)enif_priv_data(env);

  jxl_dec_resource_t* resource;
  if (enif_get_resource(env, argv[0], type, (void**)&resource) == 0) {
    return ERROR(env, "Invalid handle");
  }

  JxlDecoderReset(resource->decoder.get());
  resource->have_info = false;
  resource->have_icc = false;

  return OK(env, argv[0]);
}

static ERL_NIF_TERM dec_rewind_nif(ErlNifEnv* env, int argc,
                                   const ERL_NIF_TERM argv[]) {
  ErlNifResourceType* type = (ErlNifResourceType*)enif_priv_data(env);

  jxl_dec_resource_t* resource;
  if (enif_get_resource(env, argv[0], type, (void**)&resource) == 0) {
    return ERROR(env, "Invalid handle");
  }

  JxlDecoderRewind(resource->decoder.get());

  return OK(env, argv[0]);
}

static ERL_NIF_TERM dec_skip_nif(ErlNifEnv* env, int argc,
                                 const ERL_NIF_TERM argv[]) {
  ErlNifResourceType* type = (ErlNifResourceType*)enif_priv_data(env);

  jxl_dec_resource_t* resource;
  if (enif_get_resource(env, argv[0], type, (void**)&resource) == 0) {
    return ERROR(env, "Invalid handle");
  }

  int num_frames;
  if (enif_get_int(env, argv[1], &num_frames) == 0) {
    return ERROR(env, "Invalid frame count");
  }

  JxlDecoderSkipFrames(resource->decoder.get(), num_frames);

  return OK(env, argv[0]);
}

static ERL_NIF_TERM gray8_to_rgb8_nif(ErlNifEnv* env, int argc,
                                      const ERL_NIF_TERM argv[]) {
  ErlNifBinary original;
  if (enif_inspect_binary(env, argv[0], &original) == 0) {
    return ERROR(env, "Bad argument");
  }

  int have_alpha;
  if (enif_get_int(env, argv[1], &have_alpha) == 0) {
    return ERROR(env, "Invalid have_alpha");
  }

  int stride = 1 + have_alpha;
  int new_stride = 3 + have_alpha;

  std::vector<uint8_t> pixels(original.size * new_stride);

  size_t num_pixels = original.size / stride;

  for (int i = 0; i < num_pixels; i++) {
    uint8_t gray = original.data[i * stride];
    pixels[i * new_stride + 0] = gray;
    pixels[i * new_stride + 1] = gray;
    pixels[i * new_stride + 2] = gray;
  }

  if (have_alpha) {
    for (size_t i = 0; i < num_pixels; i++) {
      pixels[i * new_stride + 3] = original.data[i * stride + 1];
    }
  }

  ERL_NIF_TERM map = enif_make_new_map(env);

  ERL_NIF_TERM data;
  unsigned char* raw = enif_make_new_binary(env, pixels.size(), &data);
  memcpy(raw, pixels.data(), pixels.size());

  MAP(env, map, "image", data);
  MAP(env, map, "num_channels", enif_make_uint(env, new_stride));

  return OK(env, map);
}

static ERL_NIF_TERM rgb8_to_gray8_nif(ErlNifEnv* env, int argc,
                                      const ERL_NIF_TERM argv[]) {
  ErlNifBinary original;
  if (enif_inspect_binary(env, argv[0], &original) == 0) {
    return ERROR(env, "Bad argument");
  }

  int have_alpha;
  if (enif_get_int(env, argv[1], &have_alpha) == 0) {
    return ERROR(env, "Invalid have_alpha");
  }

  int stride = 3 + have_alpha;
  int new_stride = 1 + have_alpha;

  std::vector<uint8_t> pixels(original.size * new_stride);
  size_t num_pixels = original.size / stride;

  for (int i = 0; i < num_pixels; i++) {
    uint8_t r = original.data[i * stride + 0];
    uint8_t g = original.data[i * stride + 1];
    uint8_t b = original.data[i * stride + 2];
    int gray = (int)((r * 0.2126 + g * 0.7152 + b * 0.0722) + 0.5);
    if (gray > 255) {
      gray = 255;
    }
    pixels[i * new_stride] = gray;
  }

  if (have_alpha) {
    for (size_t i = 0; i < num_pixels; i++) {
      pixels[i * new_stride + 1] = original.data[i * stride + 3];
    }
  }

  ERL_NIF_TERM map = enif_make_new_map(env);

  ERL_NIF_TERM data;
  unsigned char* raw = enif_make_new_binary(env, pixels.size(), &data);
  memcpy(raw, pixels.data(), pixels.size());

  MAP(env, map, "image", data);
  MAP(env, map, "num_channels", enif_make_uint(env, new_stride));

  return OK(env, map);
}

static ERL_NIF_TERM add_alpha8_nif(ErlNifEnv* env, int argc,
                                   const ERL_NIF_TERM argv[]) {
  ErlNifBinary original;
  if (enif_inspect_binary(env, argv[0], &original) == 0) {
    return ERROR(env, "Bad argument");
  }

  int stride;
  if (enif_get_int(env, argv[1], &stride) == 0) {
    return ERROR(env, "Invalid stride");
  }

  if (stride == 2 || stride == 4) {
    return OK(env, argv[0]);
  }

  size_t num_pixels = original.size / stride;
  int new_stride = stride + 1;

  std::vector<uint8_t> pixels(num_pixels * new_stride);

  for (size_t i = 0; i < num_pixels; i++) {
    memcpy(pixels.data() + i * new_stride, original.data + i * stride, stride);
    pixels[i * new_stride + stride] = 0xFF;
  }

  ERL_NIF_TERM map = enif_make_new_map(env);

  ERL_NIF_TERM data;
  unsigned char* raw = enif_make_new_binary(env, pixels.size(), &data);
  memcpy(raw, pixels.data(), pixels.size());

  MAP(env, map, "image", data);
  MAP(env, map, "num_channels", enif_make_uint(env, new_stride));

  return OK(env, map);
}

static ERL_NIF_TERM premultiply_alpha8_nif(ErlNifEnv* env, int argc,
                                           const ERL_NIF_TERM argv[]) {
  ErlNifBinary original;
  if (enif_inspect_binary(env, argv[0], &original) == 0) {
    return ERROR(env, "Bad argument");
  }

  int stride;
  if (enif_get_int(env, argv[1], &stride) == 0) {
    return ERROR(env, "Invalid stride");
  }

  if (stride == 1 || stride == 3) {
    return OK(env, argv[0]);
  }

  int new_stride = stride - 1;

  std::vector<uint8_t> pixels(original.size * new_stride);

  size_t num_pixels = original.size / stride;

  for (int i = 0; i < num_pixels; i++) {
    uint8_t alpha = original.data[i * stride + stride - 1];
    if (alpha == 0) {
      memset(pixels.data() + i * new_stride, 0, new_stride);
    } else if (alpha == 255) {
      memcpy(pixels.data() + i * new_stride, original.data + i * stride,
             new_stride);
    } else {
      for (int j = 0; j < stride; j++) {
        pixels[i * new_stride + j] =
            (uint8_t)(alpha / 255.0 * original.data[i * stride + j]);
      }
    }
  }

  ERL_NIF_TERM map = enif_make_new_map(env);

  ERL_NIF_TERM data;
  unsigned char* raw = enif_make_new_binary(env, pixels.size(), &data);
  memcpy(raw, pixels.data(), pixels.size());

  MAP(env, map, "image", data);
  MAP(env, map, "num_channels", enif_make_uint(env, new_stride));

  return OK(env, map);
}

static ERL_NIF_TERM make_utf8str(ErlNifEnv* env, const char* data) {
  ErlNifBinary utf8;

  size_t datalen = data == NULL ? 0 : strlen(data); /* TODO:strlen is wrong */
  if (0 == enif_alloc_binary(datalen, &utf8)) {
    return (enif_make_badarg(env));
  } /* XXX: use enif_raise_exception or a better way to
       report a memory error */

  memcpy(utf8.data, data, datalen);
  return (enif_make_binary(env, &utf8));
}

static void nif_destroy(ErlNifEnv* env, void* obj) {
  jxl_dec_resource_t* resource = (jxl_dec_resource_t*)obj;
  resource->~jxl_dec_resource_t();
}

static int nif_load(ErlNifEnv* env, void** data, const ERL_NIF_TERM info) {
  void* type = enif_open_resource_type(env, "Elixir", "JxlEx", nif_destroy,
                                       ERL_NIF_RT_CREATE, NULL);
  if (type == NULL) {
    return -1;
  }

  *data = type;
  return 0;
}
