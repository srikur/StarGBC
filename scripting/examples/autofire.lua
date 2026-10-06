assert(stargbc.api_version == 1)
stargbc.on("before_frame", function(frame)
    stargbc.joypad.set({ A = (frame - 1) % 6 < 3 })
end)
