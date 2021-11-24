#include "jxl_nif.h"
#include "jxl_dec_nif.h"
#include "jxl_enc_nif.h"

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
    {"rgb8_to_ycbcr", 1, rgb8_to_ycbcr_nif, 0},
    {"jxl_from_tree", 1, jxl_from_tree_nif, ERL_NIF_DIRTY_JOB_CPU_BOUND},
};

ERL_NIF_INIT(Elixir.JxlEx.Base, jxl_nif_funcs, nif_load, NULL, upgrade, NULL)

ERL_NIF_TERM make_utf8str(ErlNifEnv* env, const char* data) {
  ErlNifBinary utf8;

  size_t datalen = data == NULL ? 0 : strlen(data);
  if (0 == enif_alloc_binary(datalen, &utf8)) {
    return enif_make_badarg(env);
  }

  memcpy(utf8.data, data, datalen);
  return enif_make_binary(env, &utf8);
}
