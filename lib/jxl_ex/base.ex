defmodule JxlEx.Base do
  @moduledoc """
  Lower-level api for jxl_ex. Higher-level wrapper of libjxl.
  """

  @compile {:autoload, false}
  @on_load {:init, 0}

  @priv_paths [
    :code.priv_dir(:jxl_ex),
    '_build/prod/lib/jxl_ex/priv/',
    '_build/dev/lib/jxl_ex/priv/'
  ]

  @make_default 'libjxl_ex_nif'
  @msvc_debug 'Debug/jxl_ex_nif'
  @msvc_release 'Release/jxl_ex_nif'

  def init do
    case load_nif() do
      :ok ->
        :ok

      err ->
        raise """
        #{inspect(err)}
        An error occurred when loading jxl_ex.
        Make sure you have a C compiler and Erlang 20 installed.
        """
    end
  end

  defp load_nif() do
    [@make_default, @msvc_release, @msvc_debug]
    |> Enum.map(fn x -> Enum.map(@priv_paths, fn y -> :filename.join(y, x) end) end)
    |> Enum.reduce([], fn x, acc -> acc ++ x end)
    |> Enum.reduce_while([], fn x, acc ->
      case :erlang.load_nif(x, 0) do
        :ok -> {:halt, :ok}
        err -> {:cont, acc ++ [err]}
      end
    end)
  end

  def dec_create(_num_threads), do: fail()
  def dec_load_blob(_handle, _data), do: fail()
  def dec_basic_info(_handle), do: fail()
  def dec_icc_profile(_handle), do: fail()
  def dec_frame(_handle), do: fail()
  def dec_keep_orientation(_handle), do: fail()
  def dec_reset(_handle), do: fail()
  def dec_rewind(_handle), do: fail()
  def dec_skip(_handle, _frames), do: fail()

  def gray8_to_rgb8(_data, _have_alpha), do: fail()
  def rgb8_to_gray8(_data, _have_alpha), do: fail()
  def add_alpha8(_data, _stride), do: fail()
  def premultiply_alpha8(_data, _stride), do: fail()
  def rgb8_to_ycbcr(_data), do: fail()

  defp fail, do: raise("native function error")
end
