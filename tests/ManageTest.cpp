#include <gtest/gtest.h>

#include "core/manage/ManageRules.h"

using namespace bs::core::manage;
using V = std::vector<std::string>;

TEST(Manage, OrderedPutsIndexedFirst) {
    EXPECT_EQ(ordered({"a", "b", "c", "d"}, {"c", "x", "a"}), (V{"c", "a", "b", "d"}));
    EXPECT_EQ(ordered({}, {"a"}), V{});
}

TEST(Manage, SwapIndexesMissingEntries) {
    V index{"a", "b"};
    V shown = ordered({"a", "b", "g"}, index); // g: a global waystone, not activated
    EXPECT_TRUE(swap(index, shown, "b", "g"));
    EXPECT_EQ(index, (V{"a", "g", "b"}));
    EXPECT_FALSE(swap(index, shown, "a", "a"));
    EXPECT_FALSE(swap(index, shown, "a", "zz"));
    EXPECT_EQ(index, (V{"a", "g", "b"}));
}

TEST(Manage, SwapKeepsUnlistedEntries) {
    V index{"hidden1", "a", "other", "b"};
    EXPECT_TRUE(swap(index, {"a", "b"}, "a", "b"));
    EXPECT_EQ(index, (V{"hidden1", "b", "other", "a"}));
}

TEST(Manage, MoveToTopAndBottom) {
    V index{"a", "b", "c"};
    V shown{"a", "b", "c"};
    EXPECT_TRUE(moveToTop(index, shown, "c"));
    EXPECT_EQ(index, (V{"c", "a", "b"}));
    EXPECT_FALSE(moveToTop(index, shown, "c"));
    EXPECT_TRUE(moveToBottom(index, shown, "c"));
    EXPECT_EQ(index, (V{"a", "b", "c"}));
    EXPECT_FALSE(moveToBottom(index, shown, "c"));
    EXPECT_FALSE(moveToTop(index, shown, "nope"));
}

TEST(Manage, DeleteRules) {
    ActorFacts survival{false, false}, creative{true, true}, manager{false, true};
    EntryFacts plain{true, false, false, false, true};
    EXPECT_EQ(deleteAction(plain, survival), DeleteAction::Forget);
    EXPECT_EQ(deleteAction(plain, creative), DeleteAction::Forget);

    EntryFacts global{true, false, true, false, false};
    EXPECT_FALSE(canDelete(global, survival));
    EXPECT_TRUE(canHide(global, survival));
    EXPECT_EQ(deleteAction(global, creative), DeleteAction::Demote);
    EXPECT_EQ(deleteAction(global, manager), DeleteAction::Demote);
    EXPECT_TRUE(deleteAffectsEveryone(global));

    EntryFacts team{true, false, false, true, false};
    EXPECT_FALSE(canDelete(team, survival));
    EXPECT_TRUE(canHide(team, survival));
    team.activated = true;
    EXPECT_EQ(deleteAction(team, survival), DeleteAction::Forget);
    EXPECT_FALSE(canHide(team, survival));

    EntryFacts share{false, true, false, false, false};
    EXPECT_FALSE(canDelete(share, survival));
    EXPECT_TRUE(canHide(share, survival));
    EXPECT_EQ(deleteAction(share, creative), DeleteAction::Purge);
    EXPECT_TRUE(deleteAffectsEveryone(share));

    EntryFacts plate{false, false, false, false, false};
    EXPECT_FALSE(canDelete(plate, creative));
    EXPECT_FALSE(canHide(plate, creative));
    EXPECT_FALSE(deleteAffectsEveryone(plain));
}

TEST(Manage, Alias) {
    EXPECT_EQ(normalizeAlias("  "), std::nullopt);
    EXPECT_EQ(normalizeAlias(""), std::nullopt);
    EXPECT_EQ(normalizeAlias("Home\n"), "Home");
    EXPECT_EQ(normalizeAlias(std::string(200, 'x'))->size(), kMaxAliasLength);
    std::string cjk;
    for (int i = 0; i < 130; ++i) cjk += "\xE5\xAE\xB6"; // 家
    EXPECT_EQ(normalizeAlias(cjk)->size(), kMaxAliasLength * 3);
}

TEST(Manage, GroupCycle) {
    V groups{"waystones:user/1", "waystones:user/2"};
    EXPECT_EQ(cycleGroup(groups, "", 1), "waystones:user/1");
    EXPECT_EQ(cycleGroup(groups, "waystones:user/1", 1), "waystones:user/2");
    EXPECT_EQ(cycleGroup(groups, "waystones:user/2", 1), "");
    EXPECT_EQ(cycleGroup(groups, "", -1), "waystones:user/2");
    EXPECT_EQ(cycleGroup(groups, "gone", 1), "waystones:user/1");
    EXPECT_EQ(cycleGroup({}, "", 1), "");
}

TEST(Manage, GroupMembership) {
    V groups{"waystones:user/1", "waystones:user/2"};
    V assigned{"waystones:favorites", "waystones:user/1"};
    EXPECT_EQ(userGroupOf(assigned, groups), "waystones:user/1");
    setUserGroup(assigned, groups, "waystones:user/2");
    EXPECT_EQ(assigned, (V{"waystones:favorites", "waystones:user/2"}));
    setUserGroup(assigned, groups, "");
    EXPECT_EQ(assigned, V{"waystones:favorites"});
    EXPECT_TRUE(setMember(assigned, "waystones:favorites", false));
    EXPECT_FALSE(setMember(assigned, "waystones:favorites", false));
    EXPECT_TRUE(setMember(assigned, "waystones:favorites", true));
    EXPECT_EQ(assigned, V{"waystones:favorites"});
}
