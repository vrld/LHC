osc = {}
function osc.rect(t, f) if (f*t) % 1 < .5 then return -1 end return 1 end
function osc.saw(t, f) local ft = (f*t - .5) % 1 return 2 * ft - 1 end
function osc.tri(t, f) local ft = (f*t + .25) % 1 return math.min(4*ft-1, 3-4*ft) end
function osc.sin(t, f) return math.sin(2 * math.pi * f * t) end
function osc.wn() return math.random() * 2 - 1 end

env = {}
function env.rise(t, len, offset)
	local offset = offset or 0
	return math.max(0, math.min(1, (t-offset) / len))
end
function env.fall(t, len, offset)
	local offset = offset or 0
	return math.max(0, math.min(1, 1 - (t-offset) / len))
end
function env.risefall(t, risetime, plateautime, falltime)
	return math.max(0, math.min(t / risetime, 1, 1 - (t - (risetime+plateautime)) / falltime))
end

function compress(sd)
	sd:map(function(_,_,v) return math.tanh(v) end)
end

function play(sd)
	local p = Player(sd)
	p:play()
	return p
end

function loop(sd)
	local p = Player(sd)
	p:set_loop(true)
	p:play()
	return p
end

-- useful for gnuplot output
function dump_to_file(sd, filename)
	local f = io.open(filename, 'w')
	sd:maptime(function(t,c,v)
		if c == 1 then 
			f:write(string.format("%f    %f    ", t, v))
		elseif c == sd:channels() then 
			f:write(string.format("%f\n", v))
		else
			f:write(string.format("%f    ", v))
		end
		return v
	end)
end
