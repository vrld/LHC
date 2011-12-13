local PATH = (...):match("^(.*%.).+?$") or ""
local player    = require(PATH .. 'player')
local soundfile = require(PATH .. 'soundfile')

--[[ Misc utils ]]--
local util = {}
function util.totime(pos, samplerate)
	return pos / samplerate
end

function util.topos(time, samplerate)
	return time * samplerate
end

function util.phase(time, duration)
	return time/duration % 1
end

-- writes a single frame {ch1, ch2, ... chn} to /frame/-index i in out
-- e.g. {.23, .42}, 10 writes out[18] = .23, out[19] = .42
function util.writeframe(out, i, frame)
	local channels = #frame
	local offset = (i-1)*channels
	for c = 1,channels do
		rawset(out, offset+c, rawget(frame, c))
	end
	return out
end

-- writes a number of frames {ch1, ch2, ..., chn} to out buffer
-- e.g. util.writeframes(out, {{.23,.42}, {.24,.44}}) --> {.23, .42, .24, .44}
function util.writeframes(out, frames)
	local offset, curframe = 0, rawget(frames, 1)
	local count, channels = #frames, #curframe
	for i = 1,count do
		offset, curframe = (i-1)*channels, rawget(frames, i)
		for c = 1,channels do
			rawset(out, offset+c, rawget(curframe, c))
		end
	end
	return out
end

function util.clone_table(tbl, visited)
	visited = visited or {}
	local out = {}
	for k,v in pairs(tbl) do
		if type(k) == "table" then
			k = util.clone_table(k, visited)
		end
		if type(v) == "table" then
			v = util.clone_table(v, visited)
		end
		out[k] = v
	end

	setmetatable(out, getmetatable(tbl))
	visited[tbl] = out
	return out
end


--[[ Signal primitives ]]--
local signal = {}
function signal.rect(phase)
	return phase % 1 < .5 and -1 or 1
end

function signal.saw(phase)
	return 2 * ((phase+.5) % 1) - 1
end

function signal.tri(phase)
	phase = (phase + .25) % 1
	return math.min(4 * phase - 1, 3 - 4 * phase)
end

function signal.sin(phase)
	return math.sin(2 * math.pi * (phase % 1))
end

function signal.noise()
	return math.random() * 2 - 1
end

--[[ Interpolators ]]--
signal.interpolator = {}
local function prepare_mesh(points)
	local mesh = util.clone_table(points)
	table.sort(mesh, function(a,b) return a[1] < b[1] or (a[1] == b[1] and a[2] < b[2]) end)
	assert(mesh[1][1] >= 0 and mesh[#mesh][1] <= 1, "Control points out out bounds")
	mesh[0]       = {0, mesh[1][2]}
	mesh[#mesh+1] = {1, mesh[#mesh][2]}
	return mesh
end

function signal.interpolator.step(points)
	local mesh = prepare_mesh(points)
	return function(phase)
		phase = phase % 1
		local l,r = rawget(mesh, 0), rawget(mesh,1)
		for i = 1,#mesh-1 do
			if rawget(l, 1) <= phase and phase < rawget(r,1) then
				break
			end
			l,r = r, rawget(mesh,i+1)
		end
		return rawget(l,2)
	end
end

function signal.interpolator.linear(points)
	local mesh = prepare_mesh(points)

	return function(phase)
		phase = phase % 1
		local l,r = rawget(mesh, 0), rawget(mesh,1)
		for i = 1,#mesh-1 do
			if rawget(l, 1) <= phase and phase < rawget(r,1) then
				break
			end
			l,r = r, rawget(mesh,i+1)
		end
		local a,b = rawget(l,1), rawget(r,1)
		local s = (b - phase) / (b - a)
		return s * rawget(l,2) + (1-s) * rawget(r,2)
	end
end

function signal.interpolator.cosine(points)
	local mesh = prepare_mesh(points)
	local cos,pi = math.cos, math.pi

	return function(phase)
		phase = phase % 1
		local l,r = rawget(mesh, 0), rawget(mesh,1)
		for i = 1,#mesh-1 do
			if rawget(l, 1) <= phase and phase < rawget(r,1) then
				break
			end
			l,r = r, rawget(mesh,i+1)
		end
		local a,b = rawget(l,1), rawget(r,1)
		local s = .5 - .5 * cos(pi * (b - phase) / (b - a))
		return s * rawget(l,2) + (1-s) * rawget(r,2)
	end
end


--[[ Soundfile shortcuts ]]--
function soundfile.info(path)
	local f = soundfile.new(path)
	local info = f:info()
	f:close()
	return info.frames, info.samplerate, info.channels, info.format
end

function soundfile.read(path, pos, frames, out)
	pos    = pos or 0
	frames = frames or 0
	out    = out or {}

	local f = soundfile.new(path)
	local info = f:info()

	assert(pos >= 0 and pos < info.frames, "Invalid seeking position: " ..pos)
	if frames == 0 then frames = info.frames - pos end

	local out, read = f:read(frames, out)
	f:close()
	return out, read
end

function soundfile.write(path, samples, samplerate, channels, bits)
	samplerate = samplerate or 44100
	channels   = channels or 1
	bits       = bits or 16
	assert(type(samples) == "table", "`samples' must be a table argument")
	assert(#samples % channels == 0, "`samples' must be a multiple of " .. channels)

	local f = soundfile.new(path, "write", samplerate, channels, bits)
	local _, written = f:write(samples)
	f:close()
	return written
end

--[[ The module ]]--
return {
	util      = util,
	signal    = signal,
	player    = player,
	soundfile = soundfile
}
