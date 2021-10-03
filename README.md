# JxlEx
libjxl wrapper NIF for Elixir

## Basic Usage
```
{:ok, f} = File.read("image.jxl")
im =
  JxlEx.Decoder.new!()
  |> JxlEx.Decoder.load!(f)
  |> JxlEx.Decoder.next!()
```

```
{:ok, dec} =
  JxlEx.Decoder.new!()
  |> JxlEx.Decoder.load(f)

{:ok, basic_info} = JxlEx.Decoder.basic_info(dec)
{:ok, icc_profile} = JxlEx.Decoder.icc_profile(dec)
{:ok, im} = JxlEx.Decoder.next(dec)
```

```
> JxlEx.Decoder.new!()
  |> JxlEx.Decoder.load!(f)
  |> JxlEx.Decoder.next!()
  |> JxlEx.Image.rgb8_to_gray8!()
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

