#include "diagnostics/Checkpoint.h"

#include <fstream>
#include <sstream>
#include "Gameboy.h"
#include "diagnostics/Binary.h"
#include "diagnostics/Hash.h"
#include "diagnostics/Json.h"

namespace diagnostics {
    void WriteFingerprints(std::ostream &output, const InputFingerprint &inputs) {
        template for (constexpr auto member :
                      std::define_static_array(std::meta::nonstatic_data_members_of(^^InputFingerprint, std::meta::access_context::current())))
                WriteString(output, inputs.[:member:]);
    }

    InputFingerprint ReadFingerprints(std::istream &input) {
        InputFingerprint result;
        template for (constexpr auto member :
                      std::define_static_array(std::meta::nonstatic_data_members_of(^^InputFingerprint, std::meta::access_context::current())))
                result.[:member:] = ReadString(input);
        return result;
    }

    bool CompatibleInputs(const InputFingerprint &left, const InputFingerprint &right) {
        template for (constexpr auto member : std::define_static_array(std::meta::nonstatic_data_members_of(
                              ^^InputFingerprint, std::meta::access_context::current()))) if (left.[:member:] != right.[:member:]) return false;
        return true;
    }

    void WriteCheckpoint(const std::filesystem::path &path, const Gameboy &machine, CheckpointIdentity identity) {
        const auto payload = machine.SaveDebugState();
        identity.stateFormat = "StarGBC-debug-1-" + std::to_string(payload.size());
        identity.position = machine.ExecutionPosition();
        if (identity.build.empty() || identity.inputs.rom.empty() || identity.inputs.configuration.empty())
            throw std::invalid_argument("Checkpoint requires build, ROM and configuration fingerprints");
        std::ostringstream content(std::ios::binary);
        content << "SGCHECK1";
        WriteUnsigned(content, SchemaVersion, 4);
        WriteString(content, identity.stateFormat);
        WriteString(content, identity.build);
        WriteFingerprints(content, identity.inputs);
        WriteUnsigned(content, identity.position.tick);
        WriteUnsigned(content, identity.position.frame);
        WriteUnsigned(content, identity.position.dot, 2);
        WriteUnsigned(content, identity.position.ly, 1);
        WriteUnsigned(content, identity.position.cpuPhase, 1);
        WriteString(content, identity.replayHash);
        WriteUnsigned(content, identity.replayCursor);
        WriteUnsigned(content, payload.size());
        content.write(reinterpret_cast<const char *>(payload.data()), static_cast<std::streamsize>(payload.size()));
        const auto bytes = content.str();
        std::ofstream output(path, std::ios::binary);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        output << bytes << TextHash(bytes);
        std::ofstream metadata(path.string() + ".json");
        metadata.exceptions(std::ios::badbit | std::ios::failbit);
        WriteJson(metadata, identity);
        metadata << '\n';
    }

    CheckpointIdentity RestoreCheckpoint(const std::filesystem::path &path, Gameboy &machine, const CheckpointIdentity &compatible,
                                         const uint64_t maximumReplayCursor) {
        const auto size = std::filesystem::file_size(path);
        if (size < 76 || size > 32 * 1024 * 1024)
            throw std::invalid_argument("Invalid checkpoint size");
        std::ifstream file(path, std::ios::binary);
        std::string bytes(size, '\0');
        if (!file.read(bytes.data(), static_cast<std::streamsize>(size)))
            throw std::runtime_error("Cannot read checkpoint");
        const auto body = std::string_view(bytes).substr(0, bytes.size() - 64);
        if (TextHash(body) != std::string_view(bytes).substr(bytes.size() - 64))
            throw std::invalid_argument("Checkpoint integrity check failed");
        if (!body.starts_with("SGCHECK1"))
            throw std::invalid_argument("Unknown checkpoint format");
        std::istringstream input(std::string(body.substr(8)), std::ios::binary);
        CheckpointIdentity identity;
        identity.schemaVersion = ReadUnsigned(input, 4);
        identity.stateFormat = ReadString(input);
        identity.build = ReadString(input);
        identity.inputs = ReadFingerprints(input);
        identity.position.tick = ReadUnsigned(input);
        identity.position.frame = ReadUnsigned(input);
        identity.position.dot = ReadUnsigned(input, 2);
        identity.position.ly = ReadUnsigned(input, 1);
        identity.position.cpuPhase = ReadUnsigned(input, 1);
        identity.replayHash = ReadString(input);
        identity.replayCursor = ReadUnsigned(input);
        const auto payloadSize = ReadUnsigned(input);
        const auto expectedSize = machine.SaveDebugState().size();
        if (identity.schemaVersion != SchemaVersion || identity.stateFormat != "StarGBC-debug-1-" + std::to_string(expectedSize) ||
            payloadSize != expectedSize || identity.build != compatible.build || !CompatibleInputs(identity.inputs, compatible.inputs) ||
            identity.replayHash != compatible.replayHash || identity.replayCursor > maximumReplayCursor)
            throw std::invalid_argument("Checkpoint build, state format or initial inputs are incompatible");
        std::vector<std::byte> payload(payloadSize);
        if (!input.read(reinterpret_cast<char *>(payload.data()), static_cast<std::streamsize>(payloadSize)) ||
            input.peek() != std::char_traits<char>::eof())
            throw std::invalid_argument("Invalid checkpoint payload length");
        if (!machine.LoadDebugState(payload))
            throw std::invalid_argument("Checkpoint hardware state is invalid");
        return identity;
    }
} // namespace diagnostics
