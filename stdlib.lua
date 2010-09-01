osc = {}
function osc.rect(t, f) if (f*t) % 1 < .5 then return -1 end return 1 end
function osc.saw(t, f) local ft = (f*t - .5) % 1 return 2 * ft - 1 end
function osc.tri(t, f) local ft = (f*t + .25) % 1 return math.min(4*ft-1, 3-4*ft) end
function osc.sin(t, f) return math.sin(2 * math.pi * f * t) end
function osc.wn() return math.random() * 2 - 1 end

env = {}
function env.rise(len, offset)
	local offset = offset or 0
	return function(t)
		return math.max(0, math.min(1, (t-offset) / len))
	end
end
function env.fall(len, offset)
	local offset = offset or 0
	return function(t)
		return math.max(0, math.min(1, 1 - (t-offset) / len))
	end
end
function env.risefall(risetime, plateautime, falltime)
	return function(t)
		return math.max(0, math.min(t / risetime, 1, 1 - (t - (risetime+plateautime)) / falltime))
	end
end

-- additional helpers for SoundData objects
local SD_meta = getmetatable(SD{})

-- basic helpers
function SD_meta:maptime(func)
	return self:map(function(i,c,v) return func(i / self:samplerate(), c, v) end)
end

function SD_meta:clone()
	local ret = SD{len = self:length(), rate = self:samplerate(), ch = self:channels()}
	return ret:map(function(i,c) return self:get(i,c) end)
end

-- combination
function SD_meta:__add(other)
	assert(self:samplerate() == other:samplerate(), "Sample rate does not match")
	assert(self:channels() == other:channels(), "Cannel count does not match")

	local ret = SD{len = math.max(self:length(), other:length()), rate = self:samplerate(), ch = self:channels()}
	return ret:map(function(i,c)
		local a = i < self:samplecount() and self:get(i,c) or 0
		local b = i < other:samplecount() and other:get(i,c) or 0
		return a + b
	end)
end

function SD_meta:__sub(other)
	assert(self:samplerate() == other:samplerate(), "Sample rate does not match")
	assert(self:channels() == other:channels(), "Cannel count does not match")

	local ret = SD{len = math.max(self:length(), other:length()), rate = self:samplerate(), ch = self:channels()}
	return ret:map(function(i,c)
		local a = i < self:samplecount() and self:get(i,c) or 0
		local b = i < other:samplecount() and other:get(i,c) or 0
		return a - b
	end)
end

function SD_meta:__mul(other)
	assert(self:samplerate() == other:samplerate(), "Sample rate does not match")
	assert(self:channels() == other:channels(), "Cannel count does not match")

	local ret = SD{len = math.max(self:length(), other:length()), rate = self:samplerate(), ch = self:channels()}
	return ret:map(function(i,c)
		local a = i < self:samplecount() and self:get(i,c) or 1
		local b = i < other:samplecount() and other:get(i,c) or 1
		return a * b
	end)
end

function SD_meta:append(other)
	assert(self:samplerate() == other:samplerate(), "Sample rate does not match")
	assert(self:channels() == other:channels(), "Cannel count does not match")

	local ret = SD{len = self:length() + other:length(), rate = self:samplerate(), ch = self:channels()}
	return ret:map(function(i,c)
		if i < self:samplecount() then
			return self:get(i,c)
		end
		return other:get(i - self:samplecount(), c)
	end)
end

function SD_meta:sub(starttime, endtime)
	if starttime > endtime then starttime, endtime = endtime, starttime end

	local startidx = self:to_index(starttime)
	local endidx = self:to_index(endtime)
	assert(endtime - starttime > 0 and startidx >= 0 and endidx <= self:samplecount(),
		string.format("interval [%f:%f] out of bounds", starttime, endtime))
	local ret = SD{len = endtime - starttime, rate = self:samplerate(), ch = self:channels()}
	return ret:map(function(i,c) return self:get(i + startidx, c) end)
end

-- generators
function SD_meta:makeRect(...)
	local freq = {...}
	assert(#freq == 1 or #freq == self:channels(), "Wrong number of frequencies supplied")
	return self:maptime(function(t,c) return osc.rect(t,freq[c % #freq + 1]) end)
end
function SD_meta:makeSaw(...)
	local freq = {...}
	assert(#freq == 1 or #freq == self:channels(), "Wrong number of frequencies supplied")
	return self:maptime(function(t,c) return osc.saw(t,freq[c % #freq + 1]) end)
end
function SD_meta:makeTriangle(...)
	local freq = {...}
	assert(#freq == 1 or #freq == self:channels(), "Wrong number of frequencies supplied")
	return self:maptime(function(t,c) return osc.tri(t,freq[c % #freq + 1]) end)
end
function SD_meta:makeSin(...)
	local freq = {...}
	assert(#freq == 1 or #freq == self:channels(), "Wrong number of frequencies supplied")
	return self:maptime(function(t,c) return osc.sin(t,freq[c % #freq + 1]) end)
end
function SD_meta:makeNoise(f)
	return self:map(osc.wn)
end

-- map envelope
function SD_meta:envelope( func )
	return self:maptime(function(t,c,v) return v * func(t,c) end)
end

-- converters
function SD_meta:to_sample_rate(rate)
	local ret = SD{len = self:length(), rate = rate, ch = self:channels()}
	local scaleFactor = self:samplerate() / rate
	ret:map(function(i,c) return self:get(i * scaleFactor, c) end)
	return ret
end

function SD_meta:quantized(quants)
	local quants = quants / 2
	return self:clone():map(function(i,_,v)
		if v > 0 then
			return math.ceil(v * quants) / quants
		end
		return math.floor(v * quants) / quants
	end)
end

function SD_meta:compressed()
	self:clone():map(function(_,_,v) return math.tanh(v) end)
	return self
end

function SD_meta:normalized()
	local max = 0
	self:clone():map(function(_,_,v)
		local w = math.abs(v)
		if w > max then max = w end
		return v
	end)
	self:map(function(_,_,v) return v / max end)
	return self
end

-- player functions
function SD_meta:play()
	local p = Player(self)
	p:play()
	return p
end

function SD_meta:loop()
	local p = Player(self)
	p:looping(true)
	p:play()
	return p
end

-- useful for gnuplot output
function SD_meta:dump_to_file(filename)
	local f = io.open(filename, 'w')
	self:maptime(function(t,c,v)
		if c == 1 then
			f:write(string.format("%f    %f", t, v))
		else
			f:write(string.format("    %f", v))
		end
		if c == self:channels() then f:write("\n") end
		return v
	end)
end
