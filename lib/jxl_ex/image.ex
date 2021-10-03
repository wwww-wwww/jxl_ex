defmodule JxlEx.Image do
  alias JxlEx.Base

  defstruct [:image, :xsize, :ysize, :num_channels, :animation]

  def from(image) do
    struct(%__MODULE__{}, image)
  end

  def gray8_to_rgb8(image) do
    if image.num_channels >= 3 do
      {:ok, image}
    else
      case Base.gray8_to_rgb8(
             image.image,
             bool_to_int(image.num_channels == 2)
           ) do
        {:ok, new_image} ->
          {:ok,
           Map.from_struct(image)
           |> Map.merge(new_image)
           |> from()}

        err ->
          err
      end
    end
  end

  def gray8_to_rgb8!(image) do
    case gray8_to_rgb8(image) do
      {:ok, im} -> im
      {:error, err} -> raise err
      err -> raise "Unkown error: #{inspect(err)}"
    end
  end

  def rgb8_to_gray8(image) do
    if image.num_channels < 3 do
      {:ok, image}
    else
      case Base.rgb8_to_gray8(
             image.image,
             bool_to_int(image.num_channels == 4)
           ) do
        {:ok, new_image} ->
          {:ok,
           Map.from_struct(image)
           |> Map.merge(new_image)
           |> from()}

        err ->
          err
      end
    end
  end

  def rgb8_to_gray8!(image) do
    case rgb8_to_gray8(image) do
      {:ok, im} -> im
      {:error, err} -> raise err
      err -> raise "Unkown error: #{inspect(err)}"
    end
  end

  def add_alpha(image) do
    case Base.add_alpha8(image.image, image.num_channels) do
      {:ok, new_image} ->
        {:ok,
         Map.from_struct(image)
         |> Map.merge(new_image)
         |> from()}

      err ->
        err
    end
  end

  def add_alpha!(image) do
    case add_alpha(image) do
      {:ok, dec} -> dec
      {:error, err} -> raise err
      err -> raise "Unkown error: #{inspect(err)}"
    end
  end

  def bool_to_int(bool) do
    if bool, do: 1, else: 0
  end
end
