#include "jxl_enc_nif.h"

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <unordered_map>

#include "lib/jxl/base/file_io.h"
#include "lib/jxl/enc_cache.h"
#include "lib/jxl/enc_color_management.h"
#include "lib/jxl/enc_file.h"
#include "lib/jxl/enc_frame.h"
#include "lib/jxl/enc_heuristics.h"
#include "lib/jxl/modular/encoding/context_predict.h"
#include "lib/jxl/modular/encoding/enc_debug_tree.h"
#include "lib/jxl/modular/encoding/enc_ma.h"
#include "lib/jxl/modular/encoding/encoding.h"
#include "lib/jxl/splines.h"

namespace jxl {

namespace {
struct SplineData {
  int32_t quantization_adjustment = 1;
  std::vector<Spline> splines;
};

Splines SplinesFromSplineData(const SplineData& spline_data,
                              const ColorCorrelationMap& cmap) {
  std::vector<QuantizedSpline> quantized_splines;
  std::vector<Spline::Point> starting_points;
  quantized_splines.reserve(spline_data.splines.size());
  starting_points.reserve(spline_data.splines.size());
  for (const Spline& spline : spline_data.splines) {
    JXL_CHECK(!spline.control_points.empty());
    quantized_splines.emplace_back(spline, spline_data.quantization_adjustment,
                                   cmap.YtoXRatio(0), cmap.YtoBRatio(0));
    starting_points.push_back(spline.control_points.front());
  }
  return Splines(spline_data.quantization_adjustment,
                 std::move(quantized_splines), std::move(starting_points));
}

template <typename F>
bool ParseNode(F& tok, Tree& tree, SplineData& spline_data,
               CompressParams& cparams, size_t& W, size_t& H, CodecInOut& io,
               int& have_next, int& x0, int& y0) {
  static const std::unordered_map<std::string, int> property_map = {
      {"c", 0},
      {"g", 1},
      {"y", 2},
      {"x", 3},
      {"|N|", 4},
      {"|W|", 5},
      {"N", 6},
      {"W", 7},
      {"W-WW-NW+NWW", 8},
      {"W+N-NW", 9},
      {"W-NW", 10},
      {"NW-N", 11},
      {"N-NE", 12},
      {"N-NN", 13},
      {"W-WW", 14},
      {"WGH", 15},
      {"PrevAbs", 16},
      {"Prev", 17},
      {"PrevAbsErr", 18},
      {"PrevErr", 19},
      {"PPrevAbs", 20},
      {"PPrev", 21},
      {"PPrevAbsErr", 22},
      {"PPrevErr", 23},
  };
  static const std::unordered_map<std::string, Predictor> predictor_map = {
      {"Set", Predictor::Zero},
      {"W", Predictor::Left},
      {"N", Predictor::Top},
      {"AvgW+N", Predictor::Average0},
      {"Select", Predictor::Select},
      {"Gradient", Predictor::Gradient},
      {"Weighted", Predictor::Weighted},
      {"NE", Predictor::TopRight},
      {"NW", Predictor::TopLeft},
      {"WW", Predictor::LeftLeft},
      {"AvgW+NW", Predictor::Average1},
      {"AvgN+NW", Predictor::Average2},
      {"AvgN+NE", Predictor::Average3},
      {"AvgAll", Predictor::Average4},
  };
  auto t = tok();
  if (t == "if") {
    // Decision node.
    int p;
    t = tok();
    if (!property_map.count(t)) {
      std::cerr << "Unexpected property: " << t.c_str() << std::endl;
      return false;
    }
    p = property_map.at(t);
    if ((t = tok()) != ">") {
      std::cerr << "Expected >, found: " << t.c_str() << std::endl;
      return false;
    }
    t = tok();
    size_t num = 0;
    int split = std::stoi(t, &num);
    if (num != t.size()) {
      std::cerr << "Invalid splitval: " << t.c_str() << std::endl;
      return false;
    }
    size_t pos = tree.size();
    tree.emplace_back(PropertyDecisionNode::Split(p, split, pos + 1));
    JXL_RETURN_IF_ERROR(ParseNode(tok, tree, spline_data, cparams, W, H, io,
                                  have_next, x0, y0));
    tree[pos].rchild = tree.size();
  } else if (t == "-") {
    // Leaf
    t = tok();
    Predictor p;
    if (!predictor_map.count(t)) {
      std::cerr << "Unexpected predictor: " << t.c_str() << std::endl;
      return false;
    }
    p = predictor_map.at(t);
    t = tok();
    bool subtract = false;
    if (t == "-") {
      subtract = true;
      t = tok();
    } else if (t == "+") {
      t = tok();
    }
    size_t num = 0;
    int offset = std::stoi(t, &num);
    if (num != t.size()) {
      std::cerr << "Invalid offset: " << t.c_str() << std::endl;
      return false;
    }
    if (subtract) offset = -offset;
    tree.emplace_back(PropertyDecisionNode::Leaf(p, offset));
    return true;
  } else if (t == "Width") {
    t = tok();
    size_t num = 0;
    W = std::stoul(t, &num);
    if (num != t.size()) {
      std::cerr << "Invalid width: " << t.c_str() << std::endl;
      return false;
    }
  } else if (t == "Height") {
    t = tok();
    size_t num = 0;
    H = std::stoul(t, &num);
    if (num != t.size()) {
      std::cerr << "Invalid height: " << t.c_str() << std::endl;
      return false;
    }
  } else if (t == "/*") {
    t = tok();
    while (t != "*/" && t != "") t = tok();
  } else if (t == "Squeeze") {
    cparams.responsive = true;
  } else if (t == "GroupShift") {
    t = tok();
    size_t num = 0;
    cparams.modular_group_size_shift = std::stoul(t, &num);
    if (num != t.size() || cparams.modular_group_size_shift > 3) {
      std::cerr << "Invalid GroupShift: " << t.c_str() << std::endl;
      return false;
    }
  } else if (t == "XYB") {
    cparams.color_transform = ColorTransform::kXYB;
  } else if (t == "CbYCr") {
    cparams.color_transform = ColorTransform::kYCbCr;
  } else if (t == "RCT") {
    t = tok();
    size_t num = 0;
    cparams.colorspace = std::stoul(t, &num);
    if (num != t.size()) {
      std::cerr << "Invalid RCT: " << t.c_str() << std::endl;
      return false;
    }
  } else if (t == "Orientation") {
    t = tok();
    size_t num = 0;
    io.metadata.m.orientation = std::stoul(t, &num);
    if (num != t.size()) {
      std::cerr << "Invalid Orientation: " << t.c_str() << std::endl;
      return false;
    }
  } else if (t == "Alpha") {
    io.metadata.m.SetAlphaBits(io.metadata.m.bit_depth.bits_per_sample);
    ImageF alpha(W, H);
    io.frames[0].SetAlpha(std::move(alpha), false);
  } else if (t == "Bitdepth") {
    t = tok();
    size_t num = 0;
    io.metadata.m.bit_depth.bits_per_sample = std::stoul(t, &num);
    if (num != t.size() || io.metadata.m.bit_depth.bits_per_sample == 0 ||
        io.metadata.m.bit_depth.bits_per_sample > 31) {
      std::cerr << "Invalid Bitdepth: " << t.c_str() << std::endl;
      return false;
    }
  } else if (t == "FloatExpBits") {
    t = tok();
    size_t num = 0;
    io.metadata.m.bit_depth.floating_point_sample = true;
    io.metadata.m.bit_depth.exponent_bits_per_sample = std::stoul(t, &num);
    if (num != t.size()) {
      std::cerr << "Invalid FloatExpBits: " << t.c_str() << std::endl;
      return false;
    }
  } else if (t == "FramePos") {
    t = tok();
    size_t num = 0;
    x0 = std::stoi(t, &num);
    if (num != t.size()) {
      std::cerr << "Invalid FramePos x0: " << t.c_str() << std::endl;
      return false;
    }
    t = tok();
    y0 = std::stoi(t, &num);
    if (num != t.size()) {
      std::cerr << "Invalid FramePos y0: " << t.c_str() << std::endl;
      return false;
    }
  } else if (t == "NotLast") {
    have_next = 1;
  } else if (t == "Upsample") {
    t = tok();
    size_t num = 0;
    cparams.resampling = std::stoul(t, &num);
    if (num != t.size() ||
        (cparams.resampling != 1 && cparams.resampling != 2 &&
         cparams.resampling != 4 && cparams.resampling != 8)) {
      std::cerr << "Invalid Upsample: " << t.c_str() << std::endl;
      return false;
    }
  } else if (t == "Upsample_EC") {
    t = tok();
    size_t num = 0;
    cparams.ec_resampling = std::stoul(t, &num);
    if (num != t.size() ||
        (cparams.ec_resampling != 1 && cparams.ec_resampling != 2 &&
         cparams.ec_resampling != 4 && cparams.ec_resampling != 8)) {
      std::cerr << "Invalid Upsample_EC: " << t.c_str() << std::endl;
      return false;
    }
  } else if (t == "Animation") {
    io.metadata.m.have_animation = true;
    io.metadata.m.animation.tps_numerator = 1000;
    io.metadata.m.animation.tps_denominator = 1;
    io.frames[0].duration = 100;
  } else if (t == "AnimationFPS") {
    t = tok();
    size_t num = 0;
    io.metadata.m.animation.tps_numerator = std::stoul(t, &num);
    if (num != t.size()) {
      std::cerr << "Invalid numerator: " << t.c_str() << std::endl;
      return false;
    }
    t = tok();
    num = 0;
    io.metadata.m.animation.tps_denominator = std::stoul(t, &num);
    if (num != t.size()) {
      std::cerr << "Invalid denominator: " << t.c_str() << std::endl;
      return false;
    }
  } else if (t == "Duration") {
    t = tok();
    size_t num = 0;
    io.frames[0].duration = std::stoul(t, &num);
    if (num != t.size()) {
      std::cerr << "Invalid Duration: " << t.c_str() << std::endl;
      return false;
    }
  } else if (t == "BlendMode") {
    t = tok();
    if (t == "kAdd") {
      io.frames[0].blendmode = BlendMode::kAdd;
    } else if (t == "kReplace") {
      io.frames[0].blendmode = BlendMode::kReplace;
    } else if (t == "kBlend") {
      io.frames[0].blendmode = BlendMode::kBlend;
    } else if (t == "kAlphaWeightedAdd") {
      io.frames[0].blendmode = BlendMode::kAlphaWeightedAdd;
    } else if (t == "kMul") {
      io.frames[0].blendmode = BlendMode::kMul;
    } else {
      std::cerr << "Invalid BlendMode: " << t.c_str() << std::endl;
      return false;
    }
  } else if (t == "SplineQuantizationAdjustment") {
    t = tok();
    size_t num = 0;
    spline_data.quantization_adjustment = std::stoul(t, &num);
    if (num != t.size()) {
      std::cerr << "Invalid SplineQuantizationAdjustment: " << t.c_str()
                << std::endl;
      return false;
    }
  } else if (t == "Spline") {
    Spline spline;
    const auto ParseFloat = [&t, &tok](float& output) {
      t = tok();
      size_t num = 0;
      output = std::stof(t, &num);
      if (num != t.size()) {
        std::cerr << "Invalid spline data: " << t.c_str() << std::endl;
        return false;
      }
      return true;
    };
    for (auto& dct : spline.color_dct) {
      for (float& coefficient : dct) {
        JXL_RETURN_IF_ERROR(ParseFloat(coefficient));
      }
    }
    for (float& coefficient : spline.sigma_dct) {
      JXL_RETURN_IF_ERROR(ParseFloat(coefficient));
    }

    while (true) {
      t = tok();
      if (t == "EndSpline") break;
      size_t num = 0;
      Spline::Point point;
      point.x = std::stof(t, &num);
      bool ok_x = num == t.size();
      auto t_y = tok();
      point.y = std::stof(t_y, &num);
      if (!ok_x || num != t_y.size()) {
        std::cerr << "Invalid spline control point: " << t.c_str() << " "
                  << t_y.c_str() << std::endl;
        return false;
      }
      spline.control_points.push_back(point);
    }

    if (spline.control_points.empty()) {
      std::cerr << "Spline with no control point" << std::endl;
      return false;
    }

    spline_data.splines.push_back(std::move(spline));
  } else if (t == "Gaborish") {
    cparams.gaborish = jxl::Override::kOn;
  } else if (t == "DeltaPalette") {
    cparams.lossy_palette = true;
    cparams.palette_colors = 0;
  } else if (t == "EPF") {
    t = tok();
    size_t num = 0;
    cparams.epf = std::stoul(t, &num);
    if (num != t.size() || cparams.epf > 3) {
      std::cerr << "Invalid EPF: " << t.c_str() << std::endl;
      return false;
    }
  } else if (t == "Noise") {
    cparams.manual_noise.resize(8);
    for (size_t i = 0; i < 8; i++) {
      t = tok();
      size_t num = 0;
      cparams.manual_noise[i] = std::stof(t, &num);
      if (num != t.size()) {
        std::cerr << "Invalid noise entry: " << t.c_str() << std::endl;
        return false;
      }
    }
  } else if (t == "XYBFactors") {
    cparams.manual_xyb_factors.resize(3);
    for (size_t i = 0; i < 3; i++) {
      t = tok();
      size_t num = 0;
      cparams.manual_xyb_factors[i] = std::stof(t, &num);
      if (num != t.size()) {
        std::cerr << "Invalid XYB factor: " << t.c_str() << std::endl;
        return false;
      }
    }
  } else {
    std::cerr << "Unexpected node type: " << t.c_str() << std::endl;
    return false;
  }
  JXL_RETURN_IF_ERROR(
      ParseNode(tok, tree, spline_data, cparams, W, H, io, have_next, x0, y0));
  return true;
}

class Heuristics : public DefaultEncoderHeuristics {
 public:
  bool CustomFixedTreeLossless(const jxl::FrameDimensions& frame_dim,
                               Tree* tree) override {
    *tree = tree_;
    return true;
  }

  explicit Heuristics(Tree tree) : tree_(std::move(tree)) {}

 private:
  Tree tree_;
};
}  // namespace

int JxlFromTree(std::istream& in, std::vector<char>& out,
                const char* tree_out) {
  Tree tree;
  SplineData spline_data;
  CompressParams cparams = {};
  size_t width = 1024, height = 1024;
  int x0 = 0, y0 = 0;
  cparams.SetLossless();
  cparams.resampling = 1;
  cparams.ec_resampling = 1;
  cparams.modular_group_size_shift = 3;
  CodecInOut io;
  int have_next = 0;

  auto tok = [&in]() {
    std::string out;
    in >> out;
    return out;
  };
  if (!ParseNode(tok, tree, spline_data, cparams, width, height, io, have_next,
                 x0, y0)) {
    return 1;
  }

  if (tree_out) {
    PrintTree(tree, tree_out);
  }
  Image3F image(width, height);
  io.SetFromImage(std::move(image), ColorEncoding::SRGB());
  io.SetSize((width + x0) * cparams.resampling,
             (height + y0) * cparams.resampling);
  io.metadata.m.color_encoding.DecideIfWantICC();
  cparams.options.zero_tokens = true;
  cparams.palette_colors = 0;
  cparams.channel_colors_pre_transform_percent = 0;
  cparams.channel_colors_percent = 0;
  cparams.patches = jxl::Override::kOff;
  cparams.already_downsampled = true;
  PaddedBytes compressed;

  io.CheckMetadata();
  BitWriter writer;

  std::unique_ptr<CodecMetadata> metadata = jxl::make_unique<CodecMetadata>();
  *metadata = io.metadata;
  JXL_RETURN_IF_ERROR(metadata->size.Set(io.xsize(), io.ysize()));

  metadata->m.xyb_encoded = cparams.color_transform == ColorTransform::kXYB;

  JXL_RETURN_IF_ERROR(WriteHeaders(metadata.get(), &writer, nullptr));
  writer.ZeroPadToByte();

  while (true) {
    PassesEncoderState enc_state;
    enc_state.heuristics = jxl::make_unique<Heuristics>(tree);
    enc_state.shared.image_features.splines =
        SplinesFromSplineData(spline_data, enc_state.shared.cmap);

    FrameInfo info;
    info.is_last = !have_next;
    if (!info.is_last) info.save_as_reference = 1;

    io.frames[0].origin.x0 = x0;
    io.frames[0].origin.y0 = y0;

    JXL_RETURN_IF_ERROR(EncodeFrame(cparams, info, metadata.get(), io.frames[0],
                                    &enc_state, GetJxlCms(), nullptr, &writer,
                                    nullptr));
    if (!have_next) break;
    tree.clear();
    spline_data.splines.clear();
    have_next = 0;
    if (!ParseNode(tok, tree, spline_data, cparams, width, height, io,
                   have_next, x0, y0)) {
      return 1;
    }
    Image3F image(width, height);
    io.SetFromImage(std::move(image), ColorEncoding::SRGB());
    io.frames[0].blend = true;
  }

  compressed = std::move(writer).TakeBytes();

  out.insert(out.end(), compressed.data(),
             compressed.data() + compressed.size());

  return 0;
}
}  // namespace jxl

ERL_NIF_TERM jxl_from_tree_nif(ErlNifEnv* env, int argc,
                               const ERL_NIF_TERM argv[]) {
  ErlNifBinary blob;
  if (enif_inspect_binary(env, argv[0], &blob) == 0) {
    return ERROR(env, "Bad argument");
  }

  std::stringstream in;
  in.write(reinterpret_cast<const char*>(blob.data), blob.size);

  std::vector<char> out;

  std::stringstream buffer;
  std::streambuf* old = std::cerr.rdbuf(buffer.rdbuf());

  try {
    if (jxl::JxlFromTree(in, out, NULL)) {
      std::string text = buffer.str();
      std::cerr.rdbuf(old);
      return ERROR(env, text.c_str());
    }
  } catch (const std::exception& e) {
    std::cerr << e.what();

    std::string text = buffer.str();
    std::cerr.rdbuf(old);
    return ERROR(env, text.c_str());
  }

  std::cerr.rdbuf(old);

  ERL_NIF_TERM data;
  unsigned char* raw = enif_make_new_binary(env, out.size(), &data);
  memcpy(raw, out.data(), out.size());

  return OK(env, data);
}
