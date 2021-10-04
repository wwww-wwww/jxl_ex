defmodule JxlEx.JxlBasicInfo do
  defstruct [
    :have_container,
    :xsize,
    :ysize,
    :bits_per_sample,
    :exponent_bits_per_sample,
    :intensity_target,
    :min_nits,
    :relative_to_max_display,
    :linear_below,
    :uses_original_profile,
    :have_preview,
    :have_animation,
    :orientation,
    :num_color_channels,
    :num_extra_channels,
    :alpha_bits,
    :alpha_exponent_bits,
    :alpha_premultiplied,
    :preview,
    :animation
  ]

  def from(basic_info) do
    struct(
      %__MODULE__{},
      %{
        basic_info
        | have_container: bool(basic_info.have_container),
          relative_to_max_display: bool(basic_info.relative_to_max_display),
          uses_original_profile: bool(basic_info.uses_original_profile),
          have_preview: bool(basic_info.have_preview),
          have_animation: bool(basic_info.have_animation),
          alpha_premultiplied: bool(basic_info.alpha_premultiplied)
      }
    )
  end

  defp bool(int), do: if(int == 1, do: true, else: false)
end
