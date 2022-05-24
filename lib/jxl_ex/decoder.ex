defmodule JxlEx.Decoder do
  @moduledoc """
  Higher-level api for jxl_ex.
  """

  alias JxlEx.Base

  @container <<0, 0, 0, 0xC, 74, 88, 76, 32, 0xD, 0xA, 0x87, 0xA>>
  @codestream <<0xFF, 0x0A>>

  def new(num_threads \\ 0), do: Base.dec_create(num_threads)

  def new!(num_threads \\ 0) do
    case new(num_threads) do
      {:ok, dec} -> dec
      {:error, err} -> raise err
      err -> raise "Unkown error: #{inspect(err)}"
    end
  end

  def load(handle, data), do: Base.dec_load_blob(handle, data)

  def load!(handle, data) do
    case load(handle, data) do
      {:ok, dec} -> dec
      {:error, err} -> raise err
      err -> raise "Unkown error: #{inspect(err)}"
    end
  end

  def basic_info(handle) do
    case Base.dec_basic_info(handle) do
      {:ok, info} -> {:ok, JxlEx.JxlBasicInfo.from(info)}
      err -> err
    end
  end

  def basic_info!(handle) do
    case basic_info(handle) do
      {:ok, dec} -> dec
      {:error, err} -> raise err
      err -> raise "Unkown error: #{inspect(err)}"
    end
  end

  def next(handle) do
    case Base.dec_frame(handle) do
      {:ok, data} -> {:ok, JxlEx.Image.from(data)}
      err -> err
    end
  end

  def next!(handle) do
    case next(handle) do
      {:ok, dec} -> dec
      {:error, err} -> raise err
      err -> raise "Unkown error: #{inspect(err)}"
    end
  end

  def icc_profile(handle), do: Base.dec_icc_profile(handle)

  def icc_profile!(handle) do
    case icc_profile(handle) do
      {:ok, dec} -> dec
      {:error, err} -> raise err
      err -> raise "Unkown error: #{inspect(err)}"
    end
  end

  def keep_orientation(handle), do: Base.dec_keep_orientation(handle)

  def keep_orientation!(handle) do
    case keep_orientation(handle) do
      {:ok, dec} -> dec
      {:error, err} -> raise err
      err -> raise "Unkown error: #{inspect(err)}"
    end
  end

  def reset(handle), do: Base.dec_reset(handle)

  def reset!(handle) do
    case reset(handle) do
      {:ok, dec} -> dec
      {:error, err} -> raise err
      err -> raise "Unkown error: #{inspect(err)}"
    end
  end

  def rewind(handle), do: Base.dec_rewind(handle)

  def rewind!(handle) do
    case rewind(handle) do
      {:ok, dec} -> dec
      {:error, err} -> raise err
      err -> raise "Unkown error: #{inspect(err)}"
    end
  end

  def skip(handle, frames), do: Base.dec_skip(handle, frames)

  def skip!(handle, frames) do
    case skip(handle, frames) do
      {:ok, dec} -> dec
      {:error, err} -> raise err
      err -> raise "Unkown error: #{inspect(err)}"
    end
  end

  def is_jxl?(data) do
    case data do
      @container <> _ = ^data -> true
      @codestream <> _ = ^data -> true
      _ -> false
    end
  end

  def decode_all_frames(dec) do
    case next(dec) do
      {:ok, %{animation: %{is_last: 0}} = im} -> [im] ++ decode_all_frames(dec)
      {:ok, im} -> [im]
      _ -> []
    end
  end

  def decode(data, num_threads \\ 0) do
    case new(num_threads) do
      {:ok, dec} ->
        case load(dec, data) do
          {:ok, _} -> {:ok, {basic_info!(dec), decode_all_frames(dec)}}
          err -> err
        end

      err ->
        err
    end
  end

  def decode!(data, num_threads \\ 0) do
    case decode(data, num_threads) do
      {:ok, ret} -> ret
      {:error, err} -> raise err
      err -> raise "Unkown error: #{inspect(err)}"
    end
  end
end
