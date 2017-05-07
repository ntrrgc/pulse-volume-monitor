PulseAudio console volume monitor
=================================

This is a text-based PulseAudio volume monitor. 

At startup, it prints a new line with the current system volume as a percentage. Every time the system volume is changed, a new line is printed with the new value.

This is intended to be used in scripts like those for desktop widgets such as [lemonbar](https://github.com/LemonBoy/bar) and [conky](https://github.com/brndnmtthws/conky). This utility provides accurate readings without needing to resort to polling.

## Building

You can use the typical building procedure for CMake projects:

    mkdir build && cd build
    cmake <path to project>
    make

## Example usage

Execute the command in the terminal and change your system volume using hotkeys, your desktop controls or pavucontrol.

    $ ./pulse_volume_monitor --desired-sink analog
    57.341%
    57.9636%
    57.9636%
    59.1217%
    59.8343%
    60.6354%
    61.0794%

## Usage

    USAGE: 

       ./pulse_volume_monitor  [-s <name>] [-j] [-v] [--] [--version] [-h]


    Where: 

       -s <name>,  --desired-sink <name>
         Insert here part of the name of an audio card (as seen in pavucontrol)
         and it will be selected instead of just picking the first available.

       -j,  --json
         Use detailed JSON output.

       -v,  --verbose
         Shows more information than really necessary.

       --,  --ignore_rest
         Ignores the rest of the labeled arguments following this flag.

       --version
         Displays version information and exits.

       -h,  --help
         Displays usage information and exits.


       Reports the current volume of a PulseAudio sink, in an asynchronous way
       (without polling)

## License

Copyright (c) 2017 Alicia Boya García

MIT License.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
