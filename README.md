# Riff

A small terminal music player written in C++ using [FTXUI](https://github.com/ArthurSonzogni/FTXUI) and [mpv](https://mpv.io/).

This is mostly a little side project where I'm messing around with C++, trying new ideas, and seeing what I can build in a terminal.

## Why FTXUI?

I really like **FTXUI**.

It's a really fun library to work with, and it makes building terminal interfaces feel much less painful than I expected. This project started partly because I wanted an excuse to play around with it more.

## Features

* Play `.mp3` files from a directory
* Play / pause
* Next / previous track
* Seek forward and backward
* Playback progress
* Synced lyrics via [LRCLIB](https://lrclib.net/)
* Keyboard-driven terminal UI

## Controls

| Key       | Action              |
| --------- | ------------------- |
| `Space`   | Play / Pause        |
| `Enter`   | Play selected track |
| `n`       | Next track          |
| `Ctrl+N`  | Previous track      |
| `h` / `←` | Seek backward       |
| `l` / `→` | Seek forward        |
| `q`       | Quit                |

## Requirements

* C++23
* [mpv](https://mpv.io/)
* [libcurl](https://github.com/curl/curl)

[FTXUI](https://github.com/ArthurSonzogni/FTXUI) is included as a Git submodule, while [nlohmann/json](https://github.com/nlohmann/json) is included directly in the project.

## Building

Clone the repository with its submodules:

```bash
git clone --recurse-submodules https://github.com/nitinbhat972/riff.git
cd riff
```

Then build the project using your preferred C++ build setup.

## Usage

Put some `.mp3` files in a `music` directory:

```bash
./riff
```

Or pass a different directory:

```bash
./riff ~/Music
```

## Status

This is just me messing around and trying new stuff.

It's not meant to be a polished or production-ready music player. I'm mainly using it to experiment with C++, mpv, terminal UIs, and FTXUI.

There are definitely things that could be improved, but that's kind of the point of the project.

## License

Riff is licensed under the [MIT License](LICENSE).
