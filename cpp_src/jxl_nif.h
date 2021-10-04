#ifndef JXL_NIF_H
#define JXL_NIF_H

#include "erl_nif.h"
#include <cstring>

ERL_NIF_TERM make_utf8str(ErlNifEnv* env, const char* data);

#define ERROR(e, m) \
  enif_make_tuple2(e, enif_make_atom(env, "error"), make_utf8str(env, m));

#define OK(e, m) enif_make_tuple2(e, enif_make_atom(env, "ok"), m);

#define MAP(env, map, key, value) \
  enif_make_map_put(env, map, enif_make_atom(env, key), value, &map);

#endif
