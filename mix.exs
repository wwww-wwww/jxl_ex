defmodule JxlEx.MixProject do
  use Mix.Project

  @source_url "https://github.com/wwww-wwww/jxl_ex"

  @description """
  libjxl wrapper NIF for Elixir
  """

  def project do
    [
      app: :jxl_ex,
      version: "0.1.0",
      elixir: "~> 1.11",
      compilers: [:cmake] ++ Mix.compilers(),
      start_permanent: Mix.env() == :prod,
      description: @description,
      package: package(),
      source_url: @source_url,
      deps: deps()
    ]
  end

  # Run "mix help compile.app" to learn about applications.
  def application do
    [
      extra_applications: [:logger]
    ]
  end

  # Run "mix help deps" to learn about dependencies.
  defp deps do
    [
      {:elixir_cmake, github: "wwww-wwww/elixir-cmake"}
    ]
  end

  defp package do
    [
      files: ["lib", "cpp_src", "libjxl", "mix.exs", "CMakeLists.txt", "README.md"],
      links: %{
        "GitHub" => @source_url
      }
    ]
  end
end
