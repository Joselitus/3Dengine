#!/usr/bin/env python3
"""Generates wind.wav: only the wind of desert.wav, with no music.

Usage: python3 generate_wind.py        (writes next to this script)
Needs numpy only.

It is the very same wind that is inside desert.wav (same noise, gusts, reverb
and level: it comes from generate_desert_music.build, which mixes the piece and
the wind apart), so they are the same sound. TestStage plays it all the time and
fades desert.wav out as the sun sets, so at night only the wind is left. It
has the same length as desert.wav and loops with no seam too.
"""
import os

from generate_desert_music import OUT, build, write_wav


def main():
    _, _, wind_l, wind_r = build()
    write_wav(os.path.join(OUT, "wind.wav"), wind_l, wind_r)


if __name__ == "__main__":
    main()
