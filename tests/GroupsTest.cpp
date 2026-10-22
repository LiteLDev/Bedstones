#include <gtest/gtest.h>

#include "core/groups/GroupRegistry.h"

using namespace bs::core;
namespace g = bs::core::groups;

namespace {
std::vector<std::string> ids(std::vector<WaystoneGroup> const& list) {
    std::vector<std::string> out;
    for (auto const& x : list) out.push_back(x.id);
    return out;
}
} // namespace

TEST(Groups, InbuiltOrderAndColors) {
    auto list = g::resolve({});
    ASSERT_EQ(list.size(), 7u);
    EXPECT_EQ(ids(list), (std::vector<std::string>{
                             "waystones:favorites", "waystones:players", "waystones:global", "waystones:teams",
                             "waystones:dimension/0", "waystones:dimension/2", "waystones:dimension/1"}));
    EXPECT_EQ(list[0].color, "gold");
    EXPECT_EQ(list[0].sortIndex, -30);
    EXPECT_EQ(list[1].color, "green");
    EXPECT_EQ(list[2].color, "yellow");
    EXPECT_EQ(list[3].color, "aqua");
    EXPECT_EQ(list[3].sortIndex, -5);
    EXPECT_EQ(list[4].color, "white");
    EXPECT_EQ(list[4].name, "Overworld");
    for (auto const& x : list) EXPECT_TRUE(x.inbuilt);
}

TEST(Groups, TranslatorNamesInbuilt) {
    auto list = g::resolve({}, [](std::string const& key, std::string const&) { return "T:" + key; });
    EXPECT_EQ(list[0].name, "T:waystones.groups.favorites");
}

TEST(Groups, CreateEditDelete) {
    std::vector<WaystoneGroup>                      reg;
    std::map<std::string, std::vector<std::string>> assign;
    auto id = g::create(reg, "  Farms  ", "villages", "green", "1234");
    EXPECT_EQ(id, "waystones:user/1234");
    auto f = g::find(reg, id);
    ASSERT_TRUE(f);
    EXPECT_EQ(f->name, "Farms");
    EXPECT_FALSE(f->inbuilt);
    EXPECT_EQ(ids(g::resolve(reg)).back(), id); // sorted last
    EXPECT_TRUE(g::update(reg, id, "Mines", "resource_sites", "dark_gray"));
    f = g::find(reg, id);
    EXPECT_EQ(f->name, "Mines");
    EXPECT_EQ(f->icon, "resource_sites");
    EXPECT_FALSE(g::update(reg, id, "x", "bogus", "white"));
    EXPECT_FALSE(g::update(reg, id, "x", "teams", "black")); // black is not offered
    EXPECT_FALSE(g::update(reg, "waystones:user/none", "x", "teams", "white"));
    assign["w1"] = {id, "waystones:favorites"};
    assign["w2"] = {id};
    EXPECT_FALSE(g::remove(reg, assign, "waystones:favorites"));
    EXPECT_TRUE(g::remove(reg, assign, id));
    EXPECT_FALSE(g::find(reg, id));
    EXPECT_EQ(assign.size(), 1u);
    EXPECT_EQ(assign["w1"], std::vector<std::string>{"waystones:favorites"});
    EXPECT_FALSE(g::remove(reg, assign, id));
}

TEST(Groups, InvalidCreateFallsBack) {
    std::vector<WaystoneGroup> reg;
    auto                       id = g::create(reg, std::string(300, 'a'), "nope", "black", "u");
    auto                       f  = g::find(reg, id);
    EXPECT_EQ(f->icon, "community_hubs");
    EXPECT_EQ(f->color, "white");
    EXPECT_EQ(f->name.size(), g::kMaxNameLength);
}

TEST(Groups, EditInbuiltKeepsDefaultName) {
    std::vector<WaystoneGroup> reg;
    EXPECT_TRUE(g::update(reg, g::kFavorites, "Favorites", "global", "red", "Favorites"));
    auto f = g::find(reg, g::kFavorites);
    EXPECT_EQ(f->name, "Favorites");
    EXPECT_EQ(f->color, "red");
    EXPECT_EQ(reg.front().name, ""); // stays translatable
    EXPECT_TRUE(f->inbuilt);
    EXPECT_TRUE(g::update(reg, g::kFavorites, "Best", "global", "red", "Favorites"));
    EXPECT_EQ(g::find(reg, g::kFavorites)->name, "Best");
}

TEST(Groups, HideInbuiltOnly) {
    std::vector<WaystoneGroup> reg;
    EXPECT_TRUE(g::setHidden(reg, g::kGlobal, true));
    EXPECT_TRUE(g::find(reg, g::kGlobal)->hidden);
    EXPECT_TRUE(g::setHidden(reg, g::kGlobal, false));
    EXPECT_FALSE(g::find(reg, g::kGlobal)->hidden);
    auto id = g::create(reg, "A", "teams", "white", "a");
    EXPECT_FALSE(g::setHidden(reg, id, true));
    EXPECT_FALSE(g::setHidden(reg, "waystones:unknown", true));
}

TEST(Groups, SwapAndMove) {
    std::vector<WaystoneGroup> reg;
    auto                       a = g::create(reg, "A", "teams", "white", "a");
    EXPECT_TRUE(g::swap(reg, g::kFavorites, g::kPlayers));
    auto order = ids(g::resolve(reg));
    EXPECT_EQ(order[0], "waystones:players");
    EXPECT_EQ(order[1], "waystones:favorites");
    EXPECT_FALSE(g::swap(reg, g::kFavorites, g::kFavorites));
    EXPECT_FALSE(g::swap(reg, g::kFavorites, "waystones:user/zzz"));
    EXPECT_TRUE(g::moveTo(reg, a, 0));
    EXPECT_EQ(ids(g::resolve(reg)).front(), a);
    EXPECT_TRUE(g::moveTo(reg, a, 1000));
    EXPECT_EQ(ids(g::resolve(reg)).back(), a);
    // A new group still goes last after a renumbering.
    auto b = g::create(reg, "B", "teams", "white", "b");
    EXPECT_EQ(ids(g::resolve(reg)).back(), b);
}

TEST(Groups, Cycles) {
    EXPECT_EQ(g::cycleIcon("community_hubs", 1), "dimension");
    EXPECT_EQ(g::cycleIcon("community_hubs", -1), "the_end");
    EXPECT_EQ(g::cycleIcon("the_end", 1), "community_hubs");
    EXPECT_EQ(g::cycleIcon("bogus", 1), "community_hubs");
    EXPECT_EQ(g::cycleColor("white", 1), "gold");
    EXPECT_EQ(g::cycleColor("white", -1), "dark_red");
    EXPECT_EQ(g::displayName("light_purple"), "Light Purple");
}

TEST(Groups, GroupsOfExplicitAndImplicit) {
    std::vector<WaystoneGroup> reg;
    auto                       a = g::create(reg, "A", "teams", "white", "a");
    auto implicit = g::implicitGroups("waystone", true, true, 1);
    EXPECT_EQ(implicit, (std::vector<std::string>{"waystones:global", "waystones:teams", "waystones:dimension/1"}));
    EXPECT_EQ(g::implicitGroups("player", false, false, 0), std::vector<std::string>{"waystones:players"});
    auto of = g::groupsOf(reg, {std::string(g::kFavorites), a, "waystones:user/deleted", a}, implicit);
    EXPECT_EQ(of, (std::vector<std::string>{"waystones:favorites", a, "waystones:global", "waystones:teams", "waystones:dimension/1"}));
}

TEST(Groups, SanitizeName) {
    EXPECT_EQ(g::sanitizeName("  hi  "), "hi");
    EXPECT_EQ(g::sanitizeName("a\nb"), "ab");
    EXPECT_EQ(g::sanitizeName("   "), "");
    EXPECT_EQ(g::sanitizeName("\xC2\xA7" "cRed\x7F"), "cRed");
    // Dropped characters do not count towards the cap.
    EXPECT_EQ(g::sanitizeName(std::string(200, '\n') + "x" + std::string(200, 'y')).size(), g::kMaxNameLength);
    EXPECT_EQ(g::sanitizeName("\xE4\xB8\xAD\xE6\x96\x87"), "\xE4\xB8\xAD\xE6\x96\x87");
}

TEST(Groups, UserGroupCap) {
    std::vector<WaystoneGroup> reg;
    for (std::size_t i = 0; i < g::kMaxUserGroups; ++i) EXPECT_FALSE(g::create(reg, "g", "teams", "white", std::to_string(i)).empty());
    EXPECT_TRUE(g::create(reg, "g", "teams", "white", "over").empty());
}

TEST(Groups, TranslationKeepsOrder) {
    // Ties sort by the untranslated name, so every locale sees the same order.
    auto en = g::resolve({});
    auto tr = g::resolve({}, [](std::string const& key, std::string const&) {
        return std::string(key == "waystones.groups.dimension.overworld" ? "zzz" : "aaa");
    });
    ASSERT_EQ(en.size(), tr.size());
    for (std::size_t i = 0; i < en.size(); ++i) EXPECT_EQ(en[i].id, tr[i].id);
}
