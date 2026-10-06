assert(stargbc.api_version == 1)
local address = 0xC000 -- Change this to the game's variable of interest.
local previous
stargbc.on("after_frame", function(frame)
    local value = stargbc.memory.peek8(address)
    if value ~= previous then
        stargbc.log(string.format("frame %d: %04X = %02X", frame, address, value))
        previous = value
    end
end)
stargbc.on("state_loaded", function() previous = nil end)
