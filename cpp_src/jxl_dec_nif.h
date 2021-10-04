#include "jxl_nif.h"

#include <vector>

#include <jxl/codestream_header.h>
#include <jxl/decode.h>
#include <jxl/decode_cxx.h>
#include "jxl/thread_parallel_runner_cxx.h"

struct jxl_dec_resource_t {
  JxlDecoderPtr decoder;
  JxlThreadParallelRunnerPtr runner;
  JxlBasicInfo jxl_info;
  JxlPixelFormat format;
  std::vector<uint8_t> icc_profile;
  bool have_info = false;
  bool have_icc = false;
};

ERL_NIF_TERM dec_create_nif(ErlNifEnv* env, int argc,
                            const ERL_NIF_TERM argv[]);
ERL_NIF_TERM dec_load_data_nif(ErlNifEnv* env, int argc,
                               const ERL_NIF_TERM argv[]);
ERL_NIF_TERM dec_basic_info_nif(ErlNifEnv* env, int argc,
                                const ERL_NIF_TERM argv[]);
ERL_NIF_TERM dec_icc_profile_nif(ErlNifEnv* env, int argc,
                                 const ERL_NIF_TERM argv[]);
ERL_NIF_TERM dec_frame_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]);
ERL_NIF_TERM dec_keep_orientation_nif(ErlNifEnv* env, int argc,
                                      const ERL_NIF_TERM argv[]);
ERL_NIF_TERM dec_reset_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]);
ERL_NIF_TERM dec_rewind_nif(ErlNifEnv* env, int argc,
                            const ERL_NIF_TERM argv[]);
ERL_NIF_TERM dec_skip_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]);

ERL_NIF_TERM gray8_to_rgb8_nif(ErlNifEnv* env, int argc,
                               const ERL_NIF_TERM argv[]);
ERL_NIF_TERM rgb8_to_gray8_nif(ErlNifEnv* env, int argc,
                               const ERL_NIF_TERM argv[]);
ERL_NIF_TERM add_alpha8_nif(ErlNifEnv* env, int argc,
                            const ERL_NIF_TERM argv[]);
ERL_NIF_TERM premultiply_alpha8_nif(ErlNifEnv* env, int argc,
                                    const ERL_NIF_TERM argv[]);
ERL_NIF_TERM rgb8_to_ycbcr_nif(ErlNifEnv* env, int argc,
                               const ERL_NIF_TERM argv[]);
