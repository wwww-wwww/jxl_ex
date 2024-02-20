#include "jxl_dec_nif.h"

void release(jxl_dec_resource_t* resource) {
  size_t remaining = JxlDecoderReleaseInput(resource->decoder.get());
  size_t front = resource->remaining_data.size() - remaining;
  resource->remaining_data.erase(resource->remaining_data.begin(),
                                 resource->remaining_data.begin() + front);
}

ERL_NIF_TERM dec_create_nif(ErlNifEnv* env, int argc,
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

ERL_NIF_TERM dec_load_data_nif(ErlNifEnv* env, int argc,
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

  resource->remaining_data.insert(resource->remaining_data.end(), blob.data,
                                  blob.data + blob.size);

  if (!resource->remaining_data.empty()) {
    JxlDecoderSetInput(resource->decoder.get(), resource->remaining_data.data(),
                       resource->remaining_data.size());
  }

  return OK(env, argv[0]);
}

ERL_NIF_TERM dec_basic_info_nif(ErlNifEnv* env, int argc,
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
        release(resource);
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
        break;
      } else if (status == JXL_DEC_COLOR_ENCODING) {
        size_t size = 0;
        if (JXL_DEC_SUCCESS !=
            JxlDecoderGetICCProfileSize(resource->decoder.get(),
                                        JXL_COLOR_PROFILE_TARGET_DATA, &size)) {
          return ERROR(env, "JxlDecoderGetICCProfileSize failed");
        }

        resource->icc_profile.resize(size);
        if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
                                   resource->decoder.get(),
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
    MAP(env, animation, "num_loops",
        enif_make_uint(env, resource->jxl_info.animation.num_loops));
    MAP(env, animation, "have_timecodes",
        enif_make_int(env, resource->jxl_info.animation.have_timecodes));
    MAP(env, map, "animation", animation);
  }

  return OK(env, map);
}

ERL_NIF_TERM dec_icc_profile_nif(ErlNifEnv* env, int argc,
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
        release(resource);
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
        if (JXL_DEC_SUCCESS !=
            JxlDecoderGetICCProfileSize(resource->decoder.get(),
                                        JXL_COLOR_PROFILE_TARGET_DATA, &size)) {
          return ERROR(env, "JxlDecoderGetICCProfileSize failed");
        }

        resource->icc_profile.resize(size);
        if (JXL_DEC_SUCCESS != JxlDecoderGetColorAsICCProfile(
                                   resource->decoder.get(),
                                   JXL_COLOR_PROFILE_TARGET_DATA,
                                   resource->icc_profile.data(), size)) {
          return ERROR(env, "JxlDecoderGetColorAsICCProfile failed");
        }
        resource->have_icc = true;
        break;
      } else {
        return ERROR(env, "Unexpected decoder status");
      }
    }
  }

  ERL_NIF_TERM data;
  unsigned char* raw =
      enif_make_new_binary(env, resource->icc_profile.size(), &data);
  memcpy(raw, resource->icc_profile.data(), resource->icc_profile.size());

  return OK(env, data);
}

ERL_NIF_TERM dec_frame_nif(ErlNifEnv* env, int argc,
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
      release(resource);
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
      JxlColorEncoding color_encoding;
      color_encoding.color_space = JXL_COLOR_SPACE_RGB;
      color_encoding.white_point = JXL_WHITE_POINT_D65;
      color_encoding.primaries = JXL_PRIMARIES_SRGB;
      color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_SRGB;
      color_encoding.rendering_intent = JXL_RENDERING_INTENT_PERCEPTUAL;
      JxlDecoderSetPreferredColorProfile(resource->decoder.get(),
                                         &color_encoding);

      size_t size = 0;
      if (JXL_DEC_SUCCESS !=
          JxlDecoderGetICCProfileSize(resource->decoder.get(),
                                      JXL_COLOR_PROFILE_TARGET_DATA, &size)) {
        return ERROR(env, "JxlDecoderGetICCProfileSize failed");
      }

      resource->icc_profile.resize(size);
      if (JXL_DEC_SUCCESS !=
          JxlDecoderGetColorAsICCProfile(resource->decoder.get(),
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
      break;
    } else if (status == JXL_DEC_SUCCESS) {
      break;
    } else {
      return ERROR(env, "Unexpected decoder status");
    }
  }

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
    ERL_NIF_TERM animation = enif_make_new_map(env);
    MAP(env, animation, "duration", enif_make_uint(env, frame_header.duration));
    MAP(env, animation, "timecode", enif_make_uint(env, frame_header.timecode));
    MAP(env, animation, "is_last", enif_make_int(env, frame_header.is_last));
    MAP(env, map, "animation", animation);
  }

  return OK(env, map);
}

ERL_NIF_TERM dec_keep_orientation_nif(ErlNifEnv* env, int argc,
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

ERL_NIF_TERM dec_reset_nif(ErlNifEnv* env, int argc,
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

ERL_NIF_TERM dec_rewind_nif(ErlNifEnv* env, int argc,
                            const ERL_NIF_TERM argv[]) {
  ErlNifResourceType* type = (ErlNifResourceType*)enif_priv_data(env);

  jxl_dec_resource_t* resource;
  if (enif_get_resource(env, argv[0], type, (void**)&resource) == 0) {
    return ERROR(env, "Invalid handle");
  }

  JxlDecoderRewind(resource->decoder.get());

  return OK(env, argv[0]);
}

ERL_NIF_TERM dec_skip_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
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

ERL_NIF_TERM gray8_to_rgb8_nif(ErlNifEnv* env, int argc,
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

ERL_NIF_TERM rgb8_to_gray8_nif(ErlNifEnv* env, int argc,
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

ERL_NIF_TERM add_alpha8_nif(ErlNifEnv* env, int argc,
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
    ERL_NIF_TERM map = enif_make_new_map(env);

    MAP(env, map, "image", argv[0]);
    MAP(env, map, "num_channels", argv[1]);

    return OK(env, map);
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

ERL_NIF_TERM premultiply_alpha8_nif(ErlNifEnv* env, int argc,
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
    ERL_NIF_TERM map = enif_make_new_map(env);

    MAP(env, map, "image", argv[0]);
    MAP(env, map, "num_channels", argv[1]);

    return OK(env, map);
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

ERL_NIF_TERM rgb8_to_ycbcr_nif(ErlNifEnv* env, int argc,
                               const ERL_NIF_TERM argv[]) {
  ErlNifBinary original;
  if (enif_inspect_binary(env, argv[0], &original) == 0) {
    return ERROR(env, "Bad argument");
  }

  size_t num_pixels = original.size / 3;

  std::vector<uint8_t> y(num_pixels);
  std::vector<uint8_t> cb(num_pixels);
  std::vector<uint8_t> cr(num_pixels);

  for (int i = 0; i < num_pixels; i++) {
    float r = (float)original.data[i * 3 + 0];
    float g = (float)original.data[i * 3 + 1];
    float b = (float)original.data[i * 3 + 2];
    y[i] = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
    cb[i] = (uint8_t)(128 - 0.168736 * r - 0.331264 * g + 0.5 * b);
    cr[i] = (uint8_t)(128 + 0.5 * r - 0.418688 * g - 0.081312 * b);
  }

  ERL_NIF_TERM map = enif_make_new_map(env);

  ERL_NIF_TERM data;
  unsigned char* raw = enif_make_new_binary(env, original.size, &data);
  memcpy(raw, y.data(), y.size());
  memcpy(raw + y.size(), cb.data(), cb.size());
  memcpy(raw + y.size() + cb.size(), cr.data(), cr.size());

  MAP(env, map, "image", data);
  MAP(env, map, "num_channels", enif_make_uint(env, 3));

  return OK(env, map);
}
