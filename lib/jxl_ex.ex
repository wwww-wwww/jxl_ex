defmodule JxlEx do
  @moduledoc """
  Documentation for `JxlEx`.
  """

  @env Mix.env()

  @environment List.to_string(:erlang.system_info(:system_architecture))

  target =
    cond do
      @environment == "win32" ->
        if @env == :dev do
          "Debug/jxl_from_tree.exe"
        else
          "Release/jxl_from_tree.exe"
        end

      true ->
        "jxl_from_tree"
    end

  @target target

  def tree(tree) do
    Path.join(:code.priv_dir(:jxl_ex), @target)
    |> Rambo.run(["-", "-"], in: tree, log: false)
    |> case do
      {:ok, %{out: out}} -> {:ok, out}
      {:error, %{err: err}} -> {:error, err}
      {:error, reason} -> {:error, reason}
    end
  end
end
