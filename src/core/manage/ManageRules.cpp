#include "core/manage/ManageRules.h"

#include <algorithm>

namespace bs::core::manage {

namespace {

bool contains(std::vector<std::string> const& v, std::string const& id) { return std::find(v.begin(), v.end(), id) != v.end(); }

bool isUserGroupId(std::string const& id, std::vector<std::string> const& userGroups) {
    return id.starts_with("waystones:user/") || contains(userGroups, id);
}

} // namespace

std::vector<std::string> ordered(std::vector<std::string> const& ids, std::vector<std::string> const& sortingIndex) {
    std::vector<std::string> out;
    out.reserve(ids.size());
    for (auto const& id : sortingIndex) {
        if (contains(ids, id) && !contains(out, id)) out.push_back(id);
    }
    for (auto const& id : ids) {
        if (!contains(out, id)) out.push_back(id);
    }
    return out;
}

void ensureIndexed(std::vector<std::string>& sortingIndex, std::vector<std::string> const& displayed) {
    for (auto const& id : displayed) {
        if (!contains(sortingIndex, id)) sortingIndex.push_back(id);
    }
}

bool swap(std::vector<std::string>& sortingIndex, std::vector<std::string> const& displayed, std::string const& a, std::string const& b) {
    if (a == b || !contains(displayed, a) || !contains(displayed, b)) return false;
    ensureIndexed(sortingIndex, displayed);
    auto ia = std::find(sortingIndex.begin(), sortingIndex.end(), a);
    auto ib = std::find(sortingIndex.begin(), sortingIndex.end(), b);
    std::iter_swap(ia, ib);
    return true;
}

bool moveToTop(std::vector<std::string>& sortingIndex, std::vector<std::string> const& displayed, std::string const& id) {
    if (!contains(displayed, id)) return false;
    ensureIndexed(sortingIndex, displayed);
    auto it = std::find(sortingIndex.begin(), sortingIndex.end(), id);
    if (it == sortingIndex.begin()) return false;
    std::rotate(sortingIndex.begin(), it, it + 1);
    return true;
}

bool moveToBottom(std::vector<std::string>& sortingIndex, std::vector<std::string> const& displayed, std::string const& id) {
    if (!contains(displayed, id)) return false;
    ensureIndexed(sortingIndex, displayed);
    auto it = std::find(sortingIndex.begin(), sortingIndex.end(), id);
    if (it + 1 == sortingIndex.end()) return false;
    std::rotate(it, it + 1, sortingIndex.end());
    return true;
}

bool canDelete(EntryFacts const& e, ActorFacts const& a) {
    if (e.sharestone) return a.creative;
    if (!e.waystone) return false;
    if (e.global && !(a.creative || a.canManageGlobal)) return false;
    if (e.team && !e.activated) return false;
    return true;
}

DeleteAction deleteAction(EntryFacts const& e, ActorFacts const& a) {
    if (!canDelete(e, a)) return DeleteAction::None;
    if (e.sharestone) return DeleteAction::Purge;
    if (e.global) return DeleteAction::Demote;
    return DeleteAction::Forget;
}

bool canHide(EntryFacts const& e, ActorFacts const& a) { return !canDelete(e, a) && (e.global || e.team || e.sharestone); }

bool deleteAffectsEveryone(EntryFacts const& e) { return e.global || e.sharestone; }

std::optional<std::string> normalizeAlias(std::string const& alias) {
    std::string out;
    std::size_t codePoints = 0;
    for (std::size_t i = 0; i < alias.size();) {
        auto c   = static_cast<unsigned char>(alias[i]);
        auto len = c < 0x80 ? 1u : (c >> 5) == 0x6 ? 2u : (c >> 4) == 0xE ? 3u : (c >> 3) == 0x1E ? 4u : 1u;
        if (i + len > alias.size()) break; // truncated sequence
        if (len == 1 && (c < 0x20 || c == 0x7F)) {
            ++i;
            continue;
        }
        if (codePoints == kMaxAliasLength) break;
        out.append(alias, i, len);
        ++codePoints;
        i += len;
    }
    auto first = out.find_first_not_of(' ');
    if (first == std::string::npos) return std::nullopt;
    return out;
}

std::string cycleGroup(std::vector<std::string> const& userGroups, std::string const& current, int dir) {
    if (userGroups.empty()) return {};
    // Position in the cycle: 0 = no group, i + 1 = userGroups[i].
    auto n   = static_cast<int>(userGroups.size()) + 1;
    auto it  = std::find(userGroups.begin(), userGroups.end(), current);
    int  pos = it == userGroups.end() ? 0 : static_cast<int>(it - userGroups.begin()) + 1;
    pos      = ((pos + (dir < 0 ? -1 : 1)) % n + n) % n;
    return pos == 0 ? std::string{} : userGroups[static_cast<std::size_t>(pos - 1)];
}

std::string userGroupOf(std::vector<std::string> const& assigned, std::vector<std::string> const& userGroups) {
    for (auto const& id : assigned) {
        if (contains(userGroups, id)) return id;
    }
    return {};
}

void setUserGroup(std::vector<std::string>& assigned, std::vector<std::string> const& userGroups, std::string const& group) {
    std::erase_if(assigned, [&](std::string const& id) { return isUserGroupId(id, userGroups); });
    if (!group.empty()) assigned.push_back(group);
}

bool setMember(std::vector<std::string>& assigned, std::string const& groupId, bool member) {
    bool has = contains(assigned, groupId);
    if (has == member) return false;
    if (member) assigned.push_back(groupId);
    else std::erase(assigned, groupId);
    return true;
}

} // namespace bs::core::manage
