#include "Gameboy.h"

AudioInspection Audio::Inspect() const {
    const auto channels = [](const Audio &audio) {
        const auto channel = [](const auto &value) -> AudioChannelInspection {
            return {value.enabled, value.dacEnabled, value.GetDigitalOutput(), value.currentOutput};
        };
        return std::array{channel(audio.ch1), channel(audio.ch2), channel(audio.ch3), channel(audio.ch4)};
    };
    auto effective = *this;
    effective.CatchUp();
    AudioInspection result{
            .processedTicks = tickCounter, .deferredTicks = pendingTicks_, .effectiveTicks = effective.tickCounter,
            .frameSequencerStep = effective.frameSeqStep, .storedControl = ReadAudioControl(),
            .cpuVisibleControl = effective.ReadAudioControl(), .storedChannels = channels(*this),
            .effectiveChannels = channels(effective), .pcm12 = effective.ReadPCM12(), .pcm34 = effective.ReadPCM34(),
            .mixerLevelChanges = effective.mixLevelChanges_};
    for (uint16_t i = 0; i < result.cpuVisibleRegisters.size(); ++i)
        result.cpuVisibleRegisters[i] = effective.ReadByte(0xFF10 + i);
    return result;
}

uint8_t Audio::InspectRegister(const uint16_t address) const {
    auto effective = *this;
    if (address == 0xFF76)
        return effective.ReadPCM12();
    if (address == 0xFF77)
        return effective.ReadPCM34();
    if (address < 0xFF10 || address > 0xFF3F)
        throw std::invalid_argument("Not an APU register");
    return effective.ReadByte(address);
}

RegisterInspection Gameboy::InspectRegister(const uint16_t address) const {
    const auto simple = [address](const uint8_t stored, const uint8_t visible) -> RegisterInspection {
        return {.address = address, .stored = stored, .cpuVisible = visible};
    };
    const auto delayed = [&](const uint8_t stored, const uint8_t pending, const uint8_t stage) {
        auto result = simple(stored, gpu_.ReadRegisters(address));
        if (stage)
            result.pending = pending;
        result.pendingStage = stage;
        return result;
    };
    switch (address) {
        case 0xFF04: return simple(timer_.divCounter >> 8, timer_.ReadByte(address));
        case 0xFF05: return simple(timer_.tima, timer_.ReadByte(address));
        case 0xFF06: return simple(timer_.tma, timer_.ReadByte(address));
        case 0xFF07: return simple(timer_.tac, timer_.ReadByte(address));
        case 0xFF0F: return simple(interrupts_.interruptFlag, interrupts_.interruptFlag | interrupts_.interruptVisiblePending | 0xE0);
        case 0xFFFF: return simple(interrupts_.interruptEnable, interrupts_.interruptEnable);
        case 0xFF40: return delayed(gpu_.lcdc, gpu_.lcdcPending, gpu_.lcdcWriteStage);
        case 0xFF41: return simple(gpu_.stat.value(), gpu_.ReadRegisters(address));
        case 0xFF42: {
            auto result = simple(gpu_.scrollY, gpu_.ReadRegisters(address));
            result.peripheralValue = gpu_.scyWriteStage ? gpu_.scyFetcherOld : gpu_.scrollY;
            result.pendingStage = gpu_.scyWriteStage;
            if (result.pendingStage)
                result.pending = gpu_.scrollY;
            return result;
        }
        case 0xFF43: {
            auto result = simple(gpu_.scrollX, gpu_.ReadRegisters(address));
            result.peripheralValue = gpu_.scxWriteStage ? gpu_.scxFetcherOld : gpu_.scrollX;
            result.pendingStage = gpu_.scxWriteStage;
            if (result.pendingStage)
                result.pending = gpu_.scrollX;
            return result;
        }
        case 0xFF44: return simple(gpu_.currentLine, gpu_.ReadRegisters(address));
        case 0xFF45: return simple(gpu_.lyc, gpu_.ReadRegisters(address));
        case 0xFF46: return simple(dma_.writtenValue, dma_.writtenValue);
        case 0xFF47: return delayed(gpu_.backgroundPalette, gpu_.bgpPending, gpu_.bgpWriteStage);
        case 0xFF48: return delayed(gpu_.obp0Palette, gpu_.obp0Pending, gpu_.obp0WriteStage);
        case 0xFF49: return delayed(gpu_.obp1Palette, gpu_.obp1Pending, gpu_.obp1WriteStage);
        case 0xFF4A: return simple(gpu_.windowY, gpu_.ReadRegisters(address));
        case 0xFF4B: return delayed(gpu_.windowX, gpu_.wxPending, gpu_.wxWriteStage);
        case 0xFF4D: return simple(bus_.prepareSpeedShift, bus_.cgbMode ? 0x7E | (bus_.speed == Speed::Double ? 0x80 : 0) | bus_.prepareSpeedShift : 0xFF);
        case 0xFF4F: return simple(gpu_.vramBank, gpu_.ReadRegisters(address));
        case 0xFF51 ... 0xFF55: {
            const auto value = gpu_.hdma.ReadHDMA(address, bus_.cgbMode);
            return simple(value, value);
        }
        case 0xFF68 ... 0xFF6C: {
            const auto value = gpu_.ReadRegisters(address);
            const bool unmapped = (address == 0xFF69 || address == 0xFF6B) && !bus_.cgbMode && !bus_.bootromRunning;
            return simple(value, unmapped ? 0xFF : value);
        }
        case 0xFF10 ... 0xFF3F: {
            auto stored = audio_;
            const auto raw = [&] {
                if (address <= 0xFF14) return stored.ch1.ReadByte(address);
                if (address <= 0xFF19) return stored.ch2.ReadByte(address);
                if (address <= 0xFF1E) return stored.ch3.ReadByte(address);
                if (address <= 0xFF23) return stored.ch4.ReadByte(address);
                if (address == 0xFF24) return stored.nr50;
                if (address == 0xFF25) return stored.nr51;
                if (address == 0xFF26) return stored.ReadAudioControl();
                if (address >= 0xFF30) return stored.ch3.ReadWaveRam(address, stored.IsDMG());
                return uint8_t{0xFF};
            }();
            return simple(raw, audio_.InspectRegister(address));
        }
        case 0xFF76:
        case 0xFF77: {
            const auto value = IsCgb(GetModel()) ? audio_.InspectRegister(address) : uint8_t{0xFF};
            return simple(value, value);
        }
        default: throw std::invalid_argument("Register inspection does not support this address");
    }
}

MachineInspection Gameboy::Inspect() const {
    MachineInspection result{
            .tick = timelineTicks_, .model = GetModel(), .speed = bus_.speed, .cgbMode = bus_.cgbMode, .bootromRunning = bus_.bootromRunning,
            .cpuPhase = static_cast<uint8_t>(cpuTickPhase_ & 3), .cpu = cpu_.Inspect(),
            .interrupts = {.enable = interrupts_.interruptEnable, .storedFlags = interrupts_.interruptFlag,
                           .cpuVisibleFlags = static_cast<uint8_t>(interrupts_.interruptFlag | interrupts_.interruptVisiblePending | 0xE0),
                           .delayedRequests = interrupts_.interruptFlagDelayed, .visiblePending = interrupts_.interruptVisiblePending,
                           .requestDelays = interrupts_.interruptSetDelays, .visibilityDelays = interrupts_.interruptVisibleDelays,
                           .masterEnable = interrupts_.interruptMasterEnable, .enablePending = interrupts_.interruptDelay},
            .timer = {.divider = timer_.divCounter, .tima = timer_.tima, .tma = timer_.tma, .tac = timer_.tac,
                      .reloadDelay = timer_.overflowDelay, .apuEventDelay = timer_.apuEventDelay,
                      .overflowPending = timer_.overflowPending, .reloadActive = timer_.reloadActive, .apuEventSecondary = timer_.apuEventSecondary},
            .dma = {.source = dma_.startAddress, .destinationOffset = dma_.currentByte, .busLatch = bus_.dmaReadByte,
                    .cpuPhase = dma_.dmaTickCounter, .cycles = dma_.ticks, .pendingSource = dma_.pendingStart,
                    .restartCountdown = dma_.restartCountdown, .active = dma_.transferActive,
                    .complete = dma_.transferComplete, .restartPending = dma_.restartPending, .hdma = gpu_.hdma},
            .ppu = {.mode = gpu_.stat.mode, .ly = gpu_.currentLine, .dot = gpu_.scanlineCounter, .pixelsDrawn = gpu_.pixelsDrawn,
                    .backgroundQueueSize = static_cast<uint8_t>(gpu_.backgroundQueue.size()),
                    .spriteQueueSize = static_cast<uint8_t>(gpu_.spriteFetchQueue.size()),
                    .fetcherState = gpu_.fetcherState_, .fetchAddress = gpu_.lastAddress_, .tile = gpu_.fetcherTileNum_,
                    .tileRow = gpu_.fetcherTileRow_, .lowPlane = gpu_.fetcherTileDataLow_, .highPlane = gpu_.fetcherTileDataHigh_,
                    .scrollDiscard = gpu_.initialScrollXDiscard_, .windowLine = gpu_.windowLineCounter_,
                    .fetchingWindow = gpu_.isFetchingWindow_, .fetchingSprite = gpu_.spriteFetchActive_,
                    .windowTriggered = gpu_.windowTriggeredThisFrame, .statRequestLine = gpu_.statTriggered},
            .audio = audio_.Inspect()};
    constexpr std::array<uint16_t, 9> addresses{0xFF40, 0xFF41, 0xFF42, 0xFF43, 0xFF44, 0xFF47, 0xFF48, 0xFF49, 0xFF4B};
    for (size_t i = 0; i < addresses.size(); ++i)
        result.ppu.registers[i] = InspectRegister(addresses[i]);
    return result;
}
