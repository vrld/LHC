-- run with busted

local lhc = require 'lhc'

local function sleep(t)
	local t0 = os.clock()
	local t1 = t0 + t
	while os.clock() < t1 do --[[nothing]] end
end

describe("Buffers", function()

	describe("Creation", function()
		it("initializes with a number argument", function()
			local b = lhc.buffer(3)
			assert.are.equals(#b, 3)
		end)

		it("can be empty", function()
			local b
			assert.has_no.errors(function()
				b = lhc.buffer(0)
			end)
		end)

		it("can be empty", function()
			local b = lhc.buffer(0)
			assert.are.equals(#b, 0)
		end)

		it("can fill the buffer with a default value", function()
			local n = math.random(5,10)
			local b = lhc.buffer(n, 0)
			assert.are.equals(#b, n)
			for i = 1,n do
				assert.are.equals(b[i], 0)
			end
		end)

		it("can be initialized using a function", function()
			local b = lhc.buffer(5, function(i) return i * i end)
			for i = 1,5 do
				assert.are.equals(b[i], i*i)
			end
		end)

		it("can be initialized using a table", function()
			local t = {1,2,3,4,5}
			local b = lhc.buffer(t)
			assert.are.equals(#b, #t)
			for i = 1,#t do
				assert.are.equals(b[i], t[i])
			end
		end)

		it("can be initialized using a string", function()
			local b = lhc.buffer("\0\0\0\0\0\0\0\0")
			assert.are.equals(#b, 2) -- float == 32 bit
			for i = 1,2 do
				assert.are.equals(b[i], 0)
			end
		end)
	end)

	describe("Getters and setters", function()
		local b
		before_each(function()
			b = lhc.buffer{1,2,3,4,5,6,7,8,9,10}
		end)

		it("uses __index to get values", function()
			for i = 1,10 do
				assert.are.equals(b[i], i)
			end
		end)

		it("uses __index for linear interpolation", function()
			for i = 1,9 do
				assert.are.equals(b[i+0.5], i+0.5)
			end
		end)

		it("uses :get() to get values", function()
			for i = 1,10 do
				assert.are.equals(b:get(i), i)
			end
		end)

		it("uses :get() to get ranges", function()
			assert.are.equals(select('#', b:get(3,6)), 4)
			local a,b,c,d = b:get(3,6)
			assert.are.equals(a, 3)
			assert.are.equals(b, 4)
			assert.are.equals(c, 5)
			assert.are.equals(d, 6)
		end)

		it("uses :get() to get ranges", function()
			assert.are.equals(select('#', b:get(1,-1)), #b)
			local a,b,c,d,e,f,g,h,i,j = b:get(1,-1)
			assert.are.equals(a, 1)
			assert.are.equals(b, 2)
			assert.are.equals(c, 3)
			assert.are.equals(d, 4)
			assert.are.equals(e, 5)
			assert.are.equals(f, 6)
			assert.are.equals(g, 7)
			assert.are.equals(h, 8)
			assert.are.equals(i, 9)
			assert.are.equals(j, 10)
		end)

		it("uses :get() to get ranges", function()
			assert.are.equals(select('#', b:get(2,-2)), #b - 2)
			local b,c,d,e,f,g,h,i = b:get(2,-2)
			assert.are.equals(b, 2)
			assert.are.equals(c, 3)
			assert.are.equals(d, 4)
			assert.are.equals(e, 5)
			assert.are.equals(f, 6)
			assert.are.equals(g, 7)
			assert.are.equals(h, 8)
			assert.are.equals(i, 9)
		end)

		it("uses :set() to set values", function()
			b:set(1,42)
			b:set(2,23)
			assert.are.equals(b[1], 42)
			assert.are.equals(b[2], 23)
		end)
	end)

	describe("Arithmetics", function()
		before_each(function()
			a = lhc.buffer{3,2,1}
			b = lhc.buffer{1,2,3,4}
		end)

		it("can add buffers", function()
			local c = a + b -- expands a with 0s
			assert.are.same({4,4,4,4}, {c:get(1,-1)})
		end)

		it("can subtract buffers", function()
			local c = a - b -- expands a with 0s
			assert.are.same({2,0,-2,-4}, {c:get(1,-1)})
		end)

		it("can multiply buffers", function()
			local c = a * b -- expands a with 1s
			assert.are.same({3,4,3,4}, {c:get(1,-1)})
		end)

		it("can divide buffers", function()
			local c = a / b -- expands a with 1s
			-- beware the float<->double precision mismatch!
			assert.are.same({3/1, 2/2, 1/4}, {c[1], c[2], c[4]})
		end)

		it("can modulo buffers", function()
			local c = a % b -- expands a with (max_float)
			assert.are.same({3%1, 2%2, 1%3, 1e37 % 4}, {c:get(1,-1)})
		end)

		it("can modulo buffers", function()
			local c = b % a -- expands b with (max_float)
			assert.are.same({1%3, 2%2, 3%1, 4 % 1e37}, {c:get(1,-1)})
		end)

		it("can raise by buffers", function()
			local c = a ^ b -- expands a with 1s
			assert.are.same({3^1, 2^2, 1^3, 1^4}, {c:get(1,-1)})
		end)

		it("can raise by buffers", function()
			local c = b ^ a -- expands a with 1s
			assert.are.same({1^3, 2^2, 3^1, 4^1}, {c:get(1,-1)})
		end)

		it("can do all operations with numbers", function()
			local c = a + 1
			local d = a - 1
			local e = a * 2
			local f = a / 2
			local g = a % 2
			local h = a ^ 2
			assert.are.same({3+1,2+1,1+1}, {c:get(1,-1)})
			assert.are.same({3-1,2-1,1-1}, {d:get(1,-1)})
			assert.are.same({3*2,2*2,1*2}, {e:get(1,-1)})
			assert.are.same({3/2,2/2,1/2}, {f:get(1,-1)})
			assert.are.same({3%2,2%2,1%2}, {g:get(1,-1)})
			assert.are.same({3^2,2^2,1^2}, {h:get(1,-1)})
		end)

		it("can do all operations with functions", function()
			local c = a + function(i) return i end
			local d = a - function(i) return i end
			local e = a * function(i) return i end
			local f = a / function(i) return i end
			local g = a % function(i) return i end
			local h = a ^ function(i) return i end
			assert.are.same({3+1,2+2,1+3}, {c:get(1,-1)})
			assert.are.same({3-1,2-2,1-3}, {d:get(1,-1)})
			assert.are.same({3*1,2*2,1*3}, {e:get(1,-1)})
			assert.are.same({3/1,2/2    }, {f:get(1,-2)}) -- single precision again
			assert.are.same({3%1,2%2,1%3}, {g:get(1,-1)})
			assert.are.same({3^1,2^2,1^3}, {h:get(1,-1)})
		end)

		it("can do all operations with tables", function()
			local c = a + {1,2,3}
			local d = a - {1,2,3}
			local e = a * {1,2,3}
			local f = a / {1,2,3}
			local g = a % {1,2,3}
			local h = a ^ {1,2,3}
			assert.are.same({3+1,2+2,1+3}, {c:get(1,-1)})
			assert.are.same({3-1,2-2,1-3}, {d:get(1,-1)})
			assert.are.same({3*1,2*2,1*3}, {e:get(1,-1)})
			assert.are.same({3/1,2/2    }, {f:get(1,-2)})
			assert.are.same({3%1,2%2,1%3}, {g:get(1,-1)})
			assert.are.same({3^1,2^2,1^3}, {h:get(1,-1)})
		end)

		it("can concatenate buffers", function()
			local c = a .. b
			assert.are.same({3,2,1,1,2,3,4}, {c:get(1,-1)})
		end)

		it("can flip buffers", function()
			local c = -a
			local d = -b
			assert.are.same({1,2,3}, {c:get(1,-1)})
			assert.are.same({4,3,2,1}, {d:get(1,-1)})
		end)
	end)

	describe("Utility functions", function()
		local a, b
		before_each(function()
			a = lhc.buffer{1,1,1,1,1}
			b = lhc.buffer{2,2,2,2,2}
		end)

		it("can map functions", function()
			a:map(function(i,v) return i*v end)
			assert.are.same({1,2,3,4,5}, {a:get(1,-1)})
		end)

		it("can map functions", function()
			a:map(2, function(i,v) return 0 end)
			assert.are.same({1,0,0,0,0}, {a:get(1,-1)})
		end)

		it("can map functions", function()
			a:map(2, 3, function(i,v) return 0 end)
			assert.are.same({1,0,0,1,1}, {a:get(1,-1)})
		end)

		it("can slice buffers", function()
			a = lhc.buffer{1,2,3,4,5}
			local c = a:sub(3)
			assert.are.same({3,4,5}, {c:get(1,-1)})
		end)

		it("can slice buffers", function()
			a = lhc.buffer{1,2,3,4,5}
			local c = a:sub(2,4)
			assert.are.same({2,3,4}, {c:get(1,-1)})
		end)

		it("can slice buffers", function()
			a = lhc.buffer{1,2,3,4,5}
			local c = a:sub(1,-1)
			assert.are.same({1,2,3,4,5}, {c:get(1,-1)})
		end)

		it("can insert buffers in the front", function()
			local c = a:insert(0, b)
			assert.are.same({2,2,2,2,2,1,1,1,1,1}, {c:get(1,-1)})
		end)

		it("can insert buffers in the back", function()
			local c = a:insert(#a, b)
			assert.are.same({1,1,1,1,1,2,2,2,2,2}, {c:get(1,-1)})
		end)

		it("can insert buffers in the back", function()
			local c = a:insert(-1, b)
			assert.are.same({1,1,1,1,1,2,2,2,2,2}, {c:get(1,-1)})
		end)

		it("can insert buffers in the middle", function()
			local c = a:insert(2, b)
			assert.are.same({1,1,2,2,2,2,2,1,1,1}, {c:get(1,-1)})
		end)

		it("can insert tables", function()
			local t = {0,0,0}
			local c = a:insert( 0, t)
			local d = a:insert(-1, t)
			local e = a:insert( 3, t)
			assert.are.same({0,0,0,1,1,1,1,1}, {c:get(1,-1)})
			assert.are.same({1,1,1,1,1,0,0,0}, {d:get(1,-1)})
			assert.are.same({1,1,1,0,0,0,1,1}, {e:get(1,-1)})
		end)

		it("can insert numbers", function()
			local c = a:insert( 0,  0)
			local d = a:insert(-1, -1)
			local e = a:insert( 3,  3)
			assert.are.same({0,1,1,1,1,1}, {c:get(1,-1)})
			assert.are.same({1,1,1,1,1,-1}, {d:get(1,-1)})
			assert.are.same({1,1,1,3,1,1}, {e:get(1,-1)})
		end)

		it("can insert numbers", function()
			local c = a:insert( 0,  0,  10)
			local d = a:insert(-1, -1, -10)
			local e = a:insert( 3,  3,  30)
			assert.are.same({0,10,1,1,1,1,1}, {c:get(1,-1)})
			assert.are.same({1,1,1,1,1,-1,-10}, {d:get(1,-1)})
			assert.are.same({1,1,1,3,30,1,1}, {e:get(1,-1)})
		end)

		it("can insert functions", function()
			local c = a:insert( 0, 2, function(i) return i end)
			local d = a:insert(-1, 3, function(i) return i end)
			local e = a:insert( 3, 4, function(i) return i end)
			assert.are.same({1,2, 1,1,1,1,1}, {c:get(1,-1)})
			assert.are.same({1,1,1,1,1, 6, 7, 8}, {d:get(1,-1)})
			assert.are.same({1,1,1, 4,5,6,7, 1,1}, {e:get(1,-1)})
		end)

		it("can convolve buffers", function()
			local c = a:convolve(b)
			assert.are.same({
				1*2,
				1*2 + 1*2,
				1*2 + 1*2 + 1*2,
				1*2 + 1*2 + 1*2 + 1*2,
				1*2 + 1*2 + 1*2 + 1*2 + 1*2,
				1*2 + 1*2 + 1*2 + 1*2,
				1*2 + 1*2 + 1*2,
				1*2 + 1*2,
				1*2,
			}, {c:get(1,-1)})
		end)

		it("can convolve buffers", function()
			b = lhc.buffer{1,1,1}
			local c = a:convolve(b)
			assert.are.same({
				1*1,
				1*1 + 1*1,
				1*1 + 1*1 + 1*1,
				1*1 + 1*1 + 1*1,
				1*1 + 1*1 + 1*1,
				1*1 + 1*1,
				1*1,
			}, {c:get(1,-1)})
		end)

		it("can convolve buffers and tables", function()
			local c = a:convolve{1,2,3}
			assert.are.same({
				1*1,
				2*1 + 1*1,
				3*1 + 2*1 + 1*1,
				3*1 + 2*1 + 1*1,
				3*1 + 2*1 + 1*1,
				3*1 + 2*1,
				3*1,
			}, {c:get(1,-1)})
		end)

		it("can convolve buffers and functions", function()
			local c = a:convolve(function(i) return i end)
			assert.are.same({
				1*1,
				2*1 + 1*1,
				3*1 + 2*1 + 1*1,
				4*1 + 3*1 + 2*1 + 1*1,
				5*1 + 4*1 + 3*1 + 2*1 + 1*1,
				5*1 + 4*1 + 3*1 + 2*1,
				5*1 + 4*1 + 3*1,
				5*1 + 4*1,
				5*1,
			}, {c:get(1,-1)})
		end)

		it("can convolve buffers and functions", function()
			local c = a:convolve(2, function(i) return i end)
			assert.are.same({
				1*1,
				2*1 + 1*1,
				2*1 + 1*1,
				2*1 + 1*1,
				2*1 + 1*1,
				2*1,
			}, {c:get(1,-1)})
		end)

		it("can zip buffers", function()
			local c = a:zip(b)
			assert.are.same({1,2,1,2,1,2,1,2,1,2}, {c:get(1,-1)})
		end)

		it("can zip buffers", function()
			local c = lhc.buffer{3,3,3,3,3}
			local d = a:zip(b,c)
			assert.are.same({1,2,3,1,2,3,1,2,3,1,2,3,1,2,3}, {d:get(1,-1)})
		end)

		it("can zip buffers and numbers", function()
			local c = a:zip(4,5)
			assert.are.same({1,4,5,1,4,5,1,4,5,1,4,5,1,4,5}, {c:get(1,-1)})
		end)

		it("can zip buffers and tables", function()
			local c = a:zip{2,3,4,5,6}
			assert.are.same({1,2,1,3,1,4,1,5,1,6}, {c:get(1,-1)})
		end)

		it("can zip buffers and functions", function()
			local c = a:zip(function(i) return i+1 end)
			assert.are.same({1,2,1,3,1,4,1,5,1,6}, {c:get(1,-1)})
		end)

		it("can zip buffers and a mixture of buffers, numbers, tables, and functions", function()
			local c = a:zip(b, 0, {-1,-2,-3,-4,-5}, function(i) return i+1 end)
			assert.are.same({
				1,2,0,-1,2,
				1,2,0,-2,3,
				1,2,0,-3,4,
				1,2,0,-4,5,
				1,2,0,-5,6,
			}, {c:get(1,-1)})
		end)

		it("can unzip buffers again", function()
			local c = a:zip(b)
			local d,e = c:unzip(2)
			assert.are.same({
				{a:get(1,-1)},
				{b:get(1,-1)},
			}, {
				{d:get(1,-1)},
				{e:get(1,-1)},
			})
		end)

		it("can unzip buffers again", function()
			local c = a:zip(b, 0, {-1,-2,-3,-4,-5}, function(i) return i+1 end)
			local d,e,f,g,h = c:unzip(5)
			assert.are.same({
				{ 1, 1, 1, 1, 1},
				{ 2, 2, 2, 2, 2},
				{ 0, 0, 0, 0, 0},
				{-1,-2,-3,-4,-5},
				{ 2, 3, 4, 5, 6}
			}, {
				{d:get(1,-1)},
				{e:get(1,-1)},
				{f:get(1,-1)},
				{g:get(1,-1)},
				{h:get(1,-1)},
			})
		end)

		it("can clone buffers", function()
			local c = a:clone()
			local d = b:clone()
			assert.are.same({
				{a:get(1,-1)},
				{b:get(1,-1)},
			}, {
				{c:get(1,-1)},
				{d:get(1,-1)},
			})
		end)
	end)
end)

describe("Player tests", function()
	local seatbelts = lhc.buffer(44100, function(i)
		return math.sin(i/44100 * 2 * math.pi * 440)
	end) * (function(x)
		x = x / 40000
		return math.max(0, (1 / math.exp((5*x)^2) + (1-x)) / 2)
	end) * 0.5

	it("can play buffers", function()
		assert.has_no.errors(function()
			local p = lhc.player(seatbelts, 44100, 1)
			p:play()
			sleep(1)
		end)
	end)

	it("can pause", function()
		assert.has_no.errors(function()
			local p = lhc.player(seatbelts, 44100, 1)
			p:play()
			sleep(0.3)
			p:pause()
			sleep(0.5)
			p:play()
			sleep(0.7)
		end)
	end)

	it("can rewind", function()
		assert.has_no.errors(function()
			local p = lhc.player(seatbelts, 44100, 1)
			p:play()
			sleep(0.3)
			p:rewind()
			sleep(1)
		end)
	end)

	it("can seek to samples", function()
		assert.has_no.errors(function()
			local p = lhc.player(seatbelts, 44100, 1)
			p:seekTo(44100 * 0.5)
			p:play()
			sleep(1)
		end)
	end)
end)

describe("Soundfile tests", function()
	local seatbelts = lhc.buffer(44100, function(i)
		return math.sin(i/44100 * 2 * math.pi * 440)
	end) * (function(x)
		x = x / 40000
		return math.max(0, (1 / math.exp((5*x)^2) + (1-x)) / 2)
	end) * 0.5

	local encoded
	it("can encode audio to a string", function()
		assert.has_no.errors(function()
			encoded = lhc.soundfile.encode(seatbelts, "wav", 44100, 1)
		end)
	end)

	it("can decode audio from a string", function()
		assert.has_no.errors(function()
			local b, rate, channels = lhc.soundfile.decode(encoded)
			lhc.play(b, rate, channels)
			sleep(1)
		end)
	end)

	it("write audio to a file", function()
		assert.has_no.errors(function()
			encoded = lhc.soundfile.write(seatbelts, "seatbelts.wav", 44100, 1)
		end)
	end)

	it("read audio from a file", function()
		assert.has_no.errors(function()
			local b, rate, channels = lhc.soundfile.read("seatbelts.wav")
			lhc.play(b, rate, channels)
			sleep(1)
		end)
	end)
end)
