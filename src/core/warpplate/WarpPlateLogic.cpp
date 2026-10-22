#include "core/warpplate/WarpPlateLogic.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace bs::core::warpplate {

std::int32_t javaHash(std::string_view s) {
    std::uint32_t h = 0;
    for (unsigned char c : s) h = 31u * h + c;
    return static_cast<std::int32_t>(h);
}

std::string galacticIdentifier(std::string_view uid) {
    std::string out;
    for (char c : uid) {
        if ((c >= '0' && c <= '9') || c == '-') continue;
        out.push_back(c);
        if (out.size() == 8) break;
    }
    return out;
}

int colorIndex(std::string_view identifier) {
    std::int32_t h = javaHash(identifier);
    if (h == std::numeric_limits<std::int32_t>::min()) return 7; // Java: Math.abs(MIN) % 15 < 0 -> null -> GRAY
    return static_cast<int>((h < 0 ? -h : h) % 15);
}

int chatFormattingId(int idx) {
    switch (idx) {
    case 7:
        return 13; // gray -> light purple
    case 8:
        return 5; // dark gray -> dark purple
    case 0:
        return 6; // black -> gold
    default:
        return std::clamp(idx, 0, 15);
    }
}

char formattingCode(int id) { return "0123456789abcdef"[std::clamp(id, 0, 15)]; }

std::uint32_t formattingRgb(int id) {
    static constexpr std::array<std::uint32_t, 16> rgb{
        0x000000,
        0x0000AA,
        0x00AA00,
        0x00AAAA,
        0xAA0000,
        0xAA00AA,
        0xFFAA00,
        0xAAAAAA,
        0x555555,
        0x5555FF,
        0x55FF55,
        0x55FFFF,
        0xFF5555,
        0xFF55FF,
        0xFFFF55,
        0xFFFFFF,
    };
    return rgb[static_cast<std::size_t>(std::clamp(id, 0, 15))];
}

int useTime(int configured, int amethystShards, int slimeBalls) {
    float multiplier  = 1.f;
    multiplier       -= 0.016f * static_cast<float>(amethystShards);
    multiplier       += 0.016f * static_cast<float>(slimeBalls);
    int t             = static_cast<int>(static_cast<float>(configured) * multiplier);
    return std::clamp(t, 1, std::max(1, configured * 2));
}

ShardKind shardKindOf(std::string_view id) {
    if (id.empty()) return ShardKind::None;
    if (id == "bedstones:dormant_shard") return ShardKind::Dormant;
    if (id == "bedstones:attuned_shard") return ShardKind::Attuned;
    if (id == "bedstones:crumbling_attuned_shard") return ShardKind::Crumbling;
    if (id == "bedstones:deepslate_shard") return ShardKind::Deepslate;
    return ShardKind::Other;
}

bool isWarpShard(std::string_view id) {
    auto k = shardKindOf(id);
    return k != ShardKind::None && k != ShardKind::Other;
}

Status idleStatus(ShardKind shard, bool redstoneDisabled) {
    switch (shard) {
    case ShardKind::None:
        return Status::Empty;
    case ShardKind::Deepslate:
        return Status::Locked;
    case ShardKind::Dormant:
        return Status::Attuning;
    default:
        return redstoneDisabled ? Status::RedstoneDisabled : Status::Idle;
    }
}

bool hasPotentialTarget(ShardKind shard) { return shard != ShardKind::None && shard != ShardKind::Deepslate; }

} // namespace bs::core::warpplate
