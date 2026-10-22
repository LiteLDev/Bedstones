#include "game/Effects.h"

#include <cmath>
#include <random>

#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/math/Vec3.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/MinecraftPackets.h"
#include "mc/network/packet/PlaySoundPacket.h"
#include "mc/network/packet/TextPacket.h"
#include "mc/network/packet/TextPacketPayload.h"
#include "mc/network/packet/TextPacketType.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"

namespace bs::game {

void actionBar(Player& player, std::string const& text) {
    auto                            packet = TextPacket::createRawMessage(text);
    TextPacketPayload::MessageOnly  message;
    message.mType = TextPacketType::Tip;
    message.mMessage->assign(text);
    packet.mBody = message;
    packet.sendTo(player);
}

void chat(Player& player, std::string const& text) { player.sendMessage(text); }

void sound(Player& to, std::string const& name, Vec3 const& at, float volume, float pitch) {
    auto  pkt = MinecraftPackets::createPacket(MinecraftPacketIds::PlaySound);
    auto& s   = static_cast<PlaySoundPacket&>(*pkt);
    s.mName   = name;
    s.mPos    = BlockPos{
        static_cast<int>(std::floor(at.x * 8.0f)),
        static_cast<int>(std::floor(at.y * 8.0f)),
        static_cast<int>(std::floor(at.z * 8.0f))
    };
    s.mVolume                   = volume;
    s.mPitch                    = pitch;
    s.mLoopCount                = 0;
    s.mBypassListenerRangeCheck = false;
    s.mServerSoundHandle->reset();
    s.mPlaybackPositionSeconds->reset();
    s.sendTo(to);
}

void soundAround(int dim, std::string const& name, Vec3 const& at, float volume, float pitch, float range) {
    auto level = ll::service::getLevel();
    if (!level) return;
    level->forEachPlayer([&](Player& p) {
        if (!p.isClientSide() && static_cast<int>(p.getDimensionId()) == dim && p.getPosition().distanceTo(at) <= range) {
            sound(p, name, at, volume, pitch);
        }
        return true;
    });
}

void particle(int dim, std::string const& name, Vec3 const& at) {
    auto level = ll::service::getLevel();
    if (!level) return;
    auto dimension = level->getDimension(dim).lock();
    if (!dimension) return;
    level->spawnParticleEffect(name, at, dimension.get());
}

void particles(int dim, std::string const& name, Vec3 const& at, int count, float spread) {
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> d(-spread, spread);
    for (int i = 0; i < count; ++i) particle(dim, name, Vec3{at.x + d(rng), at.y + d(rng), at.z + d(rng)});
}

} // namespace bs::game
