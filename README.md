# JxlEx
libjxl wrapper NIF for Elixir

## Basic Usage
```elixir
{:ok, f} = File.read("image.jxl")
im =
  JxlEx.Decoder.new!()
  |> JxlEx.Decoder.load!(f)
  |> JxlEx.Decoder.next!()
```

```elixir
{:ok, dec} =
  JxlEx.Decoder.new!()
  |> JxlEx.Decoder.load(f)

{:ok, basic_info} = JxlEx.Decoder.basic_info(dec)
{:ok, icc_profile} = JxlEx.Decoder.icc_profile(dec)
{:ok, im} = JxlEx.Decoder.next(dec)
```

```elixir
> JxlEx.Decoder.new!()
  |> JxlEx.Decoder.load!(f)
  |> JxlEx.Decoder.next!()
  |> JxlEx.Image.rgb_to_gray!()
  |> JxlEx.Image.add_alpha!()
%JxlEx.Image{
  animation: nil,
  image: <<233, 255, 233, 255, 234, 255, 234, 255, 234, 255, 233, 255, 232, 255,
    231, 255, 236, 255, 236, 255, 237, 255, 237, 255, 237, 255, 236, 255, 236,
    255, 235, 255, 237, 255, 237, 255, 237, 255, 237, 255, 237, 255, 237, 255,
    237, 255, 237, 255, ...>>,
  num_channels: 2,
  xsize: 1131,
  ysize: 1600
}
```

Transcoding to png using [yuce/png](https://github.com/yuce/png)
```elixir
{:ok, f} = File.read("image.jxl")

im =
  JxlEx.Decoder.new!()
  |> JxlEx.Decoder.load!(f)
  |> JxlEx.Decoder.next!()

mode =
  case im.num_channels do
    1 -> :grayscale
    2 -> :grayscale_alpha
    3 -> :rgb
    4 -> :rgba
  end

config = {:png_config, {im.xsize, im.ysize}, {mode, 8}, 0, 0, 0}

xstride = im.xsize * im.num_channels * 8

rows = for <<chunk::size(xstride) <- im.image>>, do: <<chunk::size(xstride)>>

rows =
:png.chunk(:IDAT, {:rows, rows})
|> Enum.reduce(<<>>, fn x, acc -> acc <> x end)

png_data =
[:png.header(), :png.chunk(:IHDR, config), rows, :png.chunk(:IEND)]
|> Enum.reduce(<<>>, fn x, acc -> acc <> x end)

{:ok, f} = File.open("out.png", [:write])
IO.binwrite(f, png_data)
File.close(f)
```

Transcoding to jpeg using [BinaryNoggin/elixir-turbojpeg](https://github.com/BinaryNoggin/elixir-turbojpeg)
```elixir
im =
  JxlEx.Decoder.new!()
  |> JxlEx.Decoder.load!(File.read!("image.jxl"))
  |> JxlEx.Decoder.next!()
  |> JxlEx.Image.gray_to_rgb!()
  |> JxlEx.Image.premultiply_alpha!()
  |> JxlEx.Image.rgb_to_ycbcr!()

# this may have issues for heights % 4 != 0
{:ok, jpeg} = Turbojpeg.yuv_to_jpeg(im.image, im.xsize, im.ysize, 90, :I444)
{:ok, file} = File.open("out.jpg", [:write])
IO.binwrite(file, jpeg)
File.close(file)
```

## Installation

The package can be installed by adding `jxl_ex` to your list of dependencies in `mix.exs`:

```elixir
def deps do
  [
    {:jxl_ex, git: "https://github.com/wwww-wwww/jxl_ex"}
  ]
end
```

Documentation can be generated with [ExDoc](https://github.com/elixir-lang/ex_doc)
and published on [HexDocs](https://hexdocs.pm). Once published, the docs can
be found at [https://hexdocs.pm/jxl_ex](https://hexdocs.pm/jxl_ex).
