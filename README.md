# Lua Humble Collider

... provides non-real time sound processing facilities for Lua.

## Non-real time?

Real time sound synthesis is hard to do right, and there are
[other tools](http://supercollider.sourceforge.net/) by smarter people that
do a very good job at this.

## Then why bother?

Because it's fun (and also Lua)!

## Can you give an example?

    local lhc = require 'lhc'
    
    local sine = lhc.buffer(44100, function(i)
        return math.sin(i/44100 * 2 * math.pi * 440)
    end)
    -- you can also initialize a buffer with a number, a table and data
    -- e.g.
    --     lhc.buffer(44100)    --> no initialization
    --     lhc.buffer(44100, 0) --> silence
    --     lhc.buffer{1,2,3,4,5}
    --     lhc.buffer("Hello, World!")
    
    local function envelope(x)
        x = x / 44100
        return (1 / math.exp((5*x)^2) + (1-x)) / 2
    end
    
    local tone = sine * envelope
    -- you can do any arithmetic operation on buffers:
    --    buffer `op` buffer
    --    buffer `op` number
    --    buffer `op` string (arbitrary data interpreted as buffer)
    --    buffer `op` table
    --    buffer `op` function
    
    lhc.play(tone)
    
    -- save it
    lhc.soundfile.write(tone, 'seatbelts.wav', 44100, 1)

A reference of all functions might follow...

## License

Copyright (c) 2012 Matthias Richter

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to
deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Except as contained in this notice, the name(s) of the above copyright
holders shall not be used in advertising or otherwise to promote the sale,
use or other dealings in this Software without prior written authorization.

If you find yourself in a situation where you can safe the author's life
without risking your own safety, you are obliged to do so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

## Third party software

LHC makes use of the following libraries:

 * Lua (obviously): http://lua.org
 * Portaudio, for playing stuff: http://portaudio.com/
 * libsndfile, for de- and encoding: http://www.mega-nerd.com/libsndfile/

## Build instructions

 * Make sure you have Lua 5.1 (or equivalent), Portaudio, libsndfile and a C-compiler on your computer.
 * Edit the `Makefile` to fit your needs (most importantly the `CC` and `CFLAGS` variables).
 * Run `make`.