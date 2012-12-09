# Lua Humble Collider

... provides non-real time sound processing facilities for Lua.

## Non-real time?

Real time sound synthesis is hard to do right, and there are
[other tools](http://supercollider.sourceforge.net/) by smarter people that
do a very good job at this.

## Then why bother?

Because it's fun (and also Lua)!

## Can you provide an example?

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