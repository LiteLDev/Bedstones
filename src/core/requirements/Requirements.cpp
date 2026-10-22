#include "core/requirements/Requirements.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <functional>
#include <limits>

namespace bs::core::requirements {

namespace {

// ---- argument and function tables ------------------------------------------------------------------

enum class Arg { Text, Number, Int, Variable, Cooldown, Item, Dimension, Entity };

enum class Family { Level, Xp, Cooldown, SoftCooldown, Item, Dismount, Refuse };
enum class Op { Add, Multiply, ScaledAdd, ScaledMultiply, Min, Max, None };

struct FunctionSpec {
    std::string      name;
    Family           family;
    Op               op;
    std::vector<Arg> args;
};

struct ConditionSpec {
    std::string      name;
    std::vector<Arg> args;
    std::string      negated{}; ///< filled when the table is built
};

constexpr std::string_view kVariables[] = {"distance", "leashed", "pets", "passengers"};

std::vector<FunctionSpec> const& functionTable() {
    static std::vector<FunctionSpec> const table = [] {
        std::vector<FunctionSpec> t;
        // Level cost (float amounts, integer clamps). Like Waystones, every step stores an int.
        t.push_back({"add_level_cost", Family::Level, Op::Add, {Arg::Number}});
        t.push_back({"multiply_level_cost", Family::Level, Op::Multiply, {Arg::Number}});
        t.push_back({"scaled_add_level_cost", Family::Level, Op::ScaledAdd, {Arg::Variable, Arg::Number}});
        t.push_back({"scaled_multiply_level_cost", Family::Level, Op::ScaledMultiply, {Arg::Variable, Arg::Number}});
        t.push_back({"min_level_cost", Family::Level, Op::Min, {Arg::Int}});
        t.push_back({"max_level_cost", Family::Level, Op::Max, {Arg::Int}});
        // XP point cost.
        t.push_back({"add_xp_cost", Family::Xp, Op::Add, {Arg::Int}});
        t.push_back({"multiply_xp_cost", Family::Xp, Op::Multiply, {Arg::Number}});
        t.push_back({"scaled_add_xp_cost", Family::Xp, Op::ScaledAdd, {Arg::Variable, Arg::Number}});
        t.push_back({"min_xp_cost", Family::Xp, Op::Min, {Arg::Int}});
        t.push_back({"max_xp_cost", Family::Xp, Op::Max, {Arg::Int}});
        // Cooldowns, soft cooldowns and item costs share the keyed shape.
        auto keyed = [&t](std::string const& suffix, Family f, Arg key) {
            t.push_back({"add_" + suffix, f, Op::Add, {key, Arg::Number}});
            t.push_back({"multiply_" + suffix, f, Op::Multiply, {key, Arg::Number}});
            t.push_back({"scaled_add_" + suffix, f, Op::ScaledAdd, {Arg::Variable, key, Arg::Number}});
            t.push_back({"scaled_multiply_" + suffix, f, Op::ScaledMultiply, {Arg::Variable, key, Arg::Number}});
            t.push_back({"min_" + suffix, f, Op::Min, {key, Arg::Number}});
            t.push_back({"max_" + suffix, f, Op::Max, {key, Arg::Number}});
        };
        keyed("cooldown", Family::Cooldown, Arg::Cooldown);
        keyed("soft_cooldown", Family::SoftCooldown, Arg::Cooldown);
        keyed("item_cost", Family::Item, Arg::Item);
        t.push_back({"dismount", Family::Dismount, Op::None, {}});
        t.push_back({"refuse", Family::Refuse, Op::None, {Arg::Text}});
        return t;
    }();
    return table;
}

std::vector<ConditionSpec> const& conditionTable() {
    static std::vector<ConditionSpec> const table = [] {
        std::vector<ConditionSpec> t{
            {"source_name_equals", {Arg::Text}},
            {"source_name_contains", {Arg::Text}},
            {"source_is_warp_plate", {}},
            {"source_is_portstone", {}},
            {"source_is_waystone", {}},
            {"source_is_sharestone", {}},
            {"source_is_inventory_button", {}},
            {"source_is_scroll", {}},
            {"source_is_bound_scroll", {}},
            {"source_is_return_scroll", {}},
            {"source_is_warp_scroll", {}},
            {"source_is_portal_scroll", {}},
            {"source_is_warp_stone", {}},
            {"source_is_dimension", {Arg::Dimension}},
            {"target_name_equals", {Arg::Text}},
            {"target_name_contains", {Arg::Text}},
            {"target_is_warp_plate", {}},
            {"target_is_warp_portal", {}},
            {"target_is_fleeting_memorial", {}},
            {"target_is_player", {}},
            {"target_is_global", {}},
            {"target_is_sharestone", {}},
            {"target_is_waystone", {}},
            {"target_is_dimension", {Arg::Dimension}},
            {"is_interdimensional", {}},
            {"involves_dimension", {Arg::Dimension}},
            {"is_within_distance", {Arg::Number}},
            {"is_on_any_vehicle", {}},
            {"is_on_vehicle", {Arg::Entity}},
            {"is_with_passengers", {}},
            {"is_with_pets", {}},
            {"is_with_leashed", {}},
            {"has_cooldown", {Arg::Cooldown}},
            {"has_cooldown_above", {Arg::Cooldown, Arg::Number}},
            {"has_item", {Arg::Item, Arg::Int}},
            {"has_empty_inventory", {}},
            {"is_wearing_any_armor", {}},
            {"has_tag", {Arg::Text}},
        };
        for (std::size_t i = 1; i < kDyeColors.size(); ++i) { // sharestones: the 15 non-white colors
            t.push_back({"source_is_" + std::string(kDyeColors[i]) + "_sharestone", {}});
            t.push_back({"target_is_" + std::string(kDyeColors[i]) + "_sharestone", {}});
        }
        for (auto& c : t) c.negated = negatedName(c.name);
        return t;
    }();
    return table;
}

FunctionSpec const* findFunction(std::string_view name) {
    for (auto const& f : functionTable()) {
        if (f.name == name) return &f;
    }
    return nullptr;
}

/// The spec of a condition (positive or negated name); `negated` tells which.
ConditionSpec const* findCondition(std::string_view name, bool* negated) {
    for (auto const& c : conditionTable()) {
        if (c.name == name) {
            if (negated) *negated = false;
            return &c;
        }
        if (c.negated == name) {
            if (negated) *negated = true;
            return &c;
        }
    }
    return nullptr;
}

// ---- small parsing helpers ----------------------------------------------------------------------------

std::string_view trim(std::string_view s) {
    auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!s.empty() && isSpace(s.front())) s.remove_prefix(1);
    while (!s.empty() && isSpace(s.back())) s.remove_suffix(1);
    return s;
}

std::optional<double> parseNumber(std::string_view s) {
    s = trim(s);
    if (s.empty()) return std::nullopt;
    if (s.front() == '+') s.remove_prefix(1);
    double v   = 0;
    auto   res = std::from_chars(s.data(), s.data() + s.size(), v);
    if (res.ec != std::errc{} || res.ptr != s.data() + s.size() || !std::isfinite(v)) return std::nullopt;
    return v;
}

std::optional<int> parseInt(std::string_view s) {
    s = trim(s);
    if (!s.empty() && s.front() == '+') s.remove_prefix(1);
    int  v   = 0;
    auto res = std::from_chars(s.data(), s.data() + s.size(), v);
    if (s.empty() || res.ec != std::errc{} || res.ptr != s.data() + s.size()) return std::nullopt;
    return v;
}

bool isNameChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == ':' || c == '/'
        || c == '.' || c == '-';
}

/// Splits on `sep` at nesting depth 0, outside quotes. Returns false on unbalanced input.
bool splitTopLevel(std::string_view s, char sep, std::vector<std::string_view>& out) {
    int    depth = 0;
    char   quote = 0;
    size_t start = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (quote) {
            if (c == '\\' && i + 1 < s.size()) ++i;
            else if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '\'') quote = c;
        else if (c == '(') ++depth;
        else if (c == ')') {
            if (--depth < 0) return false;
        } else if (c == sep && depth == 0) {
            out.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    if (quote || depth != 0) return false;
    out.push_back(s.substr(start));
    return true;
}

std::string unquote(std::string_view s) {
    s = trim(s);
    if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front()) {
        std::string out;
        for (size_t i = 1; i + 1 < s.size(); ++i) {
            if (s[i] == '\\' && i + 2 < s.size()) ++i;
            out.push_back(s[i]);
        }
        return out;
    }
    return std::string(s);
}

/// Parses `name` or `name(args)`; strips a `waystones:` prefix from the name.
std::optional<Call> parseCall(std::string_view s, std::string& error) {
    s = trim(s);
    size_t i = 0;
    while (i < s.size() && isNameChar(s[i])) ++i;
    if (i == 0) {
        error = "expected a name in '" + std::string(s) + "'";
        return std::nullopt;
    }
    Call call;
    call.name = std::string(s.substr(0, i));
    if (call.name.starts_with("waystones:")) call.name.erase(0, 10);
    std::string_view rest = trim(s.substr(i));
    if (rest.empty()) return call;
    if (rest.front() != '(' || rest.back() != ')') {
        error = "unexpected '" + std::string(rest) + "' after " + call.name;
        return std::nullopt;
    }
    std::string_view inner = rest.substr(1, rest.size() - 2);
    if (trim(inner).empty()) return call;
    std::vector<std::string_view> parts;
    if (!splitTopLevel(inner, ',', parts)) {
        error = "unbalanced arguments of " + call.name;
        return std::nullopt;
    }
    for (auto p : parts) {
        if (trim(p).empty()) {
            error = "empty argument in " + call.name;
            return std::nullopt;
        }
        call.args.push_back(unquote(p));
    }
    return call;
}

bool checkArgs(Call const& call, std::vector<Arg> const& spec, std::string& error) {
    if (call.args.size() != spec.size()) {
        error = call.name + " takes " + std::to_string(spec.size()) + " argument(s), got " + std::to_string(call.args.size());
        return false;
    }
    for (size_t i = 0; i < spec.size(); ++i) {
        auto const& a  = call.args[i];
        bool        ok = true;
        switch (spec[i]) {
        case Arg::Number:
            ok = parseNumber(a).has_value();
            break;
        case Arg::Int:
            ok = parseInt(a).has_value();
            break;
        case Arg::Variable:
            ok = std::find(std::begin(kVariables), std::end(kVariables), a) != std::end(kVariables);
            break;
        case Arg::Dimension:
            ok = dimensionId(a).has_value();
            break;
        case Arg::Cooldown:
        case Arg::Item: // plain ids only (item tags are not supported)
            ok = !a.empty() && std::all_of(a.begin(), a.end(), isNameChar);
            break;
        case Arg::Entity: // an entity id or a `#family`
            ok = !a.empty() && a != "#" && std::all_of(a.begin() + (a.front() == '#' ? 1 : 0), a.end(), isNameChar);
            break;
        case Arg::Text:
            break;
        }
        if (!ok) {
            error = "invalid argument " + std::to_string(i + 1) + " ('" + a + "') of " + call.name;
            return false;
        }
    }
    return true;
}

std::string stripNamespace(std::string_view id) {
    auto colon = id.find(':');
    return std::string(colon == std::string_view::npos ? id : id.substr(colon + 1));
}

double variable(std::string_view name, Context const& ctx) {
    if (name == "distance") return ctx.distance;
    if (name == "leashed") return ctx.leashed;
    if (name == "pets") return ctx.pets;
    if (name == "passengers") return ctx.passengers;
    return 0;
}

bool isScroll(WarpSourceKind k) {
    return k == WarpSourceKind::WarpScroll || k == WarpSourceKind::BoundScroll || k == WarpSourceKind::ReturnScroll
        || k == WarpSourceKind::PortalScroll;
}

/// A sharestone record type, or the generic list kind "sharestone".
bool isAnySharestone(std::string_view type) { return type == "sharestone" || types::isSharestone(type); }

/// Java's `(int)` cast of a double: truncation, saturating.
int toInt(double v) {
    if (std::isnan(v)) return 0;
    if (v >= static_cast<double>(std::numeric_limits<int>::max())) return std::numeric_limits<int>::max();
    if (v <= static_cast<double>(std::numeric_limits<int>::min())) return std::numeric_limits<int>::min();
    return static_cast<int>(v);
}

double applyOp(Op op, double value, double var, double amount) {
    switch (op) {
    case Op::Add:
        return value + amount;
    case Op::Multiply:
        return value * amount;
    case Op::ScaledAdd:
        return value + var * amount;
    case Op::ScaledMultiply:
        return value * (var * amount);
    case Op::Min: // a lower bound: the cost is at least `amount`
        return std::max(value, amount);
    case Op::Max: // an upper bound
        return std::min(value, amount);
    case Op::None:
        break;
    }
    return value;
}

} // namespace

// ---- names ---------------------------------------------------------------------------------------------

std::vector<std::string> defaultRules() {
    return {
        "[is_not_interdimensional] scaled_add_xp_cost(distance, 0.01)",
        "[is_interdimensional] add_xp_cost(27)",
        "[source_is_warp_plate] multiply_xp_cost(0)",
        "[source_is_scroll] multiply_xp_cost(0)",
        "[target_is_global] multiply_xp_cost(0)",
        "[target_is_fleeting_memorial] multiply_xp_cost(0)",
        "min_xp_cost(0)",
        "max_xp_cost(27)",
        "[source_is_inventory_button] add_cooldown(inventory_button, 300)",
    };
}

std::string negatedName(std::string_view condition) {
    auto pos = condition.find("is_");
    if (pos == std::string_view::npos) return "not_" + std::string(condition);
    std::string out(condition);
    out.insert(pos + 3, "not_");
    return out;
}

std::vector<std::string> conditionNames() {
    std::vector<std::string> out;
    for (auto const& c : conditionTable()) out.push_back(c.name);
    return out;
}

std::vector<std::string> functionNames() {
    std::vector<std::string> out;
    for (auto const& f : functionTable()) out.push_back(f.name);
    return out;
}

std::string cooldownId(std::string_view id) {
    id = trim(id);
    if (id.find(':') != std::string_view::npos) return std::string(id);
    return "waystones:" + std::string(id);
}

std::string itemId(std::string_view id) {
    id = trim(id);
    if (id.starts_with("waystones:")) return "bedstones:" + std::string(id.substr(10));
    if (id.find(':') != std::string_view::npos) return std::string(id);
    return "minecraft:" + std::string(id);
}

std::string entityId(std::string_view id) {
    id = trim(id);
    if (id.find(':') != std::string_view::npos) return std::string(id);
    return "minecraft:" + std::string(id);
}

std::optional<int> dimensionId(std::string_view id) {
    id = trim(id);
    if (auto n = parseInt(id)) return *n;
    auto path = stripNamespace(id);
    if (path == "overworld") return 0;
    if (path == "the_nether" || path == "nether") return 1;
    if (path == "the_end" || path == "end") return 2;
    return std::nullopt;
}

// ---- parsing --------------------------------------------------------------------------------------------

std::optional<Rule> parseRule(std::string_view line, std::string* error) {
    std::string err;
    auto        fail = [&](std::string msg) -> std::optional<Rule> {
        if (error) *error = std::move(msg);
        return std::nullopt;
    };
    std::string_view s = trim(line);
    Rule             rule;
    rule.source = std::string(s);
    if (s.empty()) return fail("empty rule");
    if (s.front() == '[') {
        // Find the closing bracket outside quotes.
        char   quote = 0;
        size_t close = std::string_view::npos;
        for (size_t i = 1; i < s.size(); ++i) {
            char c = s[i];
            if (quote) {
                if (c == '\\' && i + 1 < s.size()) ++i;
                else if (c == quote) quote = 0;
            } else if (c == '"' || c == '\'') {
                quote = c;
            } else if (c == ']') {
                close = i;
                break;
            }
        }
        if (close == std::string_view::npos) return fail("missing ']'");
        std::string_view conds = trim(s.substr(1, close - 1));
        if (!conds.empty()) {
            std::vector<std::string_view> parts;
            if (!splitTopLevel(conds, ',', parts)) return fail("unbalanced conditions");
            for (auto p : parts) {
                auto call = parseCall(p, err);
                if (!call) return fail(err);
                bool negated = false;
                auto spec    = findCondition(call->name, &negated);
                if (!spec) return fail("unknown condition '" + call->name + "'");
                if (!checkArgs(*call, spec->args, err)) return fail(err);
                rule.conditions.push_back(std::move(*call));
            }
        }
        s = trim(s.substr(close + 1));
    }
    auto fn = parseCall(s, err);
    if (!fn) return fail(err);
    auto spec = findFunction(fn->name);
    if (!spec) return fail("unknown function '" + fn->name + "'");
    if (!checkArgs(*fn, spec->args, err)) return fail(err);
    rule.function = std::move(*fn);
    return rule;
}

Program parse(std::vector<std::string> const& lines) {
    Program p;
    for (auto const& line : lines) {
        if (trim(line).empty()) continue;
        std::string err;
        if (auto rule = parseRule(line, &err)) p.rules.push_back(std::move(*rule));
        else p.errors.push_back("'" + line + "': " + err);
    }
    return p;
}

// ---- evaluation -----------------------------------------------------------------------------------------

bool testCondition(Call const& condition, Context const& ctx) {
    bool negated = false;
    auto spec    = findCondition(condition.name, &negated);
    if (!spec) return false;
    auto const& n    = spec->name;
    auto const& args = condition.args;
    auto        arg  = [&](size_t i) -> std::string const& {
        static std::string const empty;
        return i < args.size() ? args[i] : empty;
    };
    bool result = false;
    if (n == "source_name_equals") result = ctx.sourceName == arg(0);
    else if (n == "source_name_contains") result = ctx.sourceName.find(arg(0)) != std::string::npos;
    else if (n == "source_is_warp_plate") result = ctx.sourceType == types::kWarpPlate || ctx.sourceKind == WarpSourceKind::WarpPlate;
    else if (n == "source_is_portstone") result = ctx.sourceKind == WarpSourceKind::Portstone;
    else if (n == "source_is_waystone") result = ctx.sourceType == types::kWaystone;
    else if (n == "source_is_sharestone") result = isAnySharestone(ctx.sourceType) || ctx.sourceKind == WarpSourceKind::Sharestone;
    else if (n == "source_is_inventory_button") result = ctx.sourceKind == WarpSourceKind::InventoryButton;
    else if (n == "source_is_scroll") result = isScroll(ctx.sourceKind);
    else if (n == "source_is_bound_scroll") result = ctx.sourceKind == WarpSourceKind::BoundScroll;
    else if (n == "source_is_return_scroll") result = ctx.sourceKind == WarpSourceKind::ReturnScroll;
    else if (n == "source_is_warp_scroll") result = ctx.sourceKind == WarpSourceKind::WarpScroll;
    else if (n == "source_is_portal_scroll") result = ctx.sourceKind == WarpSourceKind::PortalScroll;
    else if (n == "source_is_warp_stone") result = ctx.sourceKind == WarpSourceKind::WarpStone;
    else if (n == "source_is_dimension") result = dimensionId(arg(0)) == ctx.sourceDim;
    else if (n == "target_name_equals") result = ctx.targetName == arg(0);
    else if (n == "target_name_contains") result = ctx.targetName.find(arg(0)) != std::string::npos;
    else if (n == "target_is_warp_plate") result = ctx.targetType == types::kWarpPlate;
    else if (n == "target_is_warp_portal") result = ctx.targetType == types::kWarpPortal;
    else if (n == "target_is_fleeting_memorial") result = ctx.targetType == types::kFleetingMemorial;
    else if (n == "target_is_player") result = ctx.targetType == "player";
    else if (n == "target_is_global") result = ctx.targetGlobal;
    else if (n == "target_is_sharestone") result = isAnySharestone(ctx.targetType);
    else if (n == "target_is_waystone") result = ctx.targetType == types::kWaystone;
    else if (n == "target_is_dimension") result = dimensionId(arg(0)) == ctx.targetDim;
    else if (n == "is_interdimensional") result = ctx.playerDim != ctx.targetDim;
    else if (n == "involves_dimension") {
        auto d = dimensionId(arg(0));
        result = d == ctx.sourceDim || d == ctx.targetDim;
    } else if (n == "is_within_distance") {
        result = ctx.playerDim == ctx.targetDim && ctx.distance <= parseNumber(arg(0)).value_or(0);
    } else if (n == "is_on_any_vehicle") result = ctx.onVehicle;
    else if (n == "is_on_vehicle") {
        if (!ctx.onVehicle) result = false;
        else if (arg(0).starts_with("#")) {
            auto family = stripNamespace(std::string_view(arg(0)).substr(1));
            result      = std::find(ctx.vehicleFamilies.begin(), ctx.vehicleFamilies.end(), family) != ctx.vehicleFamilies.end();
        } else {
            result = ctx.vehicleType == entityId(arg(0));
        }
    } else if (n == "is_with_passengers") result = ctx.passengers > 0;
    else if (n == "is_with_pets") result = ctx.pets > 0;
    else if (n == "is_with_leashed") result = ctx.leashed > 0;
    else if (n == "has_cooldown" || n == "has_cooldown_above") {
        auto        it        = ctx.cooldownRemainingMs.find(cooldownId(arg(0)));
        std::int64_t remaining = it == ctx.cooldownRemainingMs.end() ? 0 : it->second;
        double       above     = n == "has_cooldown" ? 0.0 : parseNumber(arg(1)).value_or(0) * 1000.0;
        result                 = remaining > 0 && static_cast<double>(remaining) > above;
    } else if (n == "has_item") {
        auto it = ctx.itemCounts.find(itemId(arg(0)));
        result  = (it == ctx.itemCounts.end() ? 0 : it->second) >= parseInt(arg(1)).value_or(1);
    } else if (n == "has_empty_inventory") result = ctx.emptyInventory;
    else if (n == "is_wearing_any_armor") result = ctx.wearingArmor;
    else if (n == "has_tag") result = std::find(ctx.tags.begin(), ctx.tags.end(), arg(0)) != ctx.tags.end();
    else {
        // Per-color sharestones: "<source|target>_is_<color>_sharestone".
        bool source = n.starts_with("source_is_");
        auto type   = n.substr(10); // "source_is_" and "target_is_" are both 10 characters
        result      = (source ? ctx.sourceType : ctx.targetType) == type;
    }
    return negated ? !result : result;
}

Evaluated evaluate(Program const& program, Context const& ctx, Options const& options) {
    Evaluated out;
    for (auto const& rule : program.rules) {
        auto spec = findFunction(rule.function.name);
        if (!spec) continue;
        bool isCost     = spec->family == Family::Level || spec->family == Family::Xp || spec->family == Family::Item;
        bool isCooldown = spec->family == Family::Cooldown || spec->family == Family::SoftCooldown;
        if (isCost && !options.enableCosts) continue;
        if (isCooldown && !options.enableCooldowns) continue;
        bool all = std::all_of(rule.conditions.begin(), rule.conditions.end(), [&](Call const& c) { return testCondition(c, ctx); });
        if (!all) continue;

        auto const& a = rule.function.args;
        // Argument layout: [variable,] [key,] amount.
        bool   scaled = spec->op == Op::ScaledAdd || spec->op == Op::ScaledMultiply;
        double var    = scaled ? variable(a[0], ctx) : 0.0;
        switch (spec->family) {
        case Family::Level:
            out.levels = toInt(applyOp(spec->op, out.levels, var, parseNumber(a.back()).value_or(0)));
            break;
        case Family::Xp:
            out.xpPoints = toInt(applyOp(spec->op, out.xpPoints, var, parseNumber(a.back()).value_or(0)));
            break;
        case Family::Item: {
            auto id = itemId(a[scaled ? 1 : 0]);
            auto it = std::find_if(out.items.begin(), out.items.end(), [&](auto const& p) { return p.first == id; });
            if (it == out.items.end()) it = out.items.insert(out.items.end(), {id, 0});
            it->second = toInt(applyOp(spec->op, it->second, var, parseNumber(a.back()).value_or(0)));
            break;
        }
        case Family::Cooldown:
        case Family::SoftCooldown: {
            bool soft = spec->family == Family::SoftCooldown;
            auto id   = cooldownId(a[scaled ? 1 : 0]);
            auto it   = std::find_if(out.cooldowns.begin(), out.cooldowns.end(), [&](auto const& c) {
                return c.id == id && c.soft == soft;
            });
            if (it == out.cooldowns.end()) it = out.cooldowns.insert(out.cooldowns.end(), {id, 0.0, soft});
            it->seconds = applyOp(spec->op, it->seconds, var, parseNumber(a.back()).value_or(0));
            break;
        }
        case Family::Dismount:
            out.dismount = true;
            break;
        case Family::Refuse:
            if (out.refuse.empty()) out.refuse = a[0]; // the first refusal wins
            break;
        }
    }
    return out;
}

Requirement Evaluated::toRequirement() const {
    Requirement r;
    r.xpPoints = std::max(0, xpPoints);
    r.levels   = std::max(0, levels);
    for (auto const& [id, count] : items) {
        if (count > 0) r.items.push_back({id, count});
    }
    for (auto const& c : cooldowns) {
        if (c.seconds > 0) r.cooldowns.push_back({c.id, c.seconds, c.soft});
    }
    r.dismount = dismount;
    r.refuse   = refuse;
    return r;
}

double activeCooldownSeconds(Requirement const& req, std::map<std::string, std::int64_t> const& remainingMs) {
    double longest = 0;
    for (auto const& c : req.cooldowns) {
        if (c.soft) continue;
        if (auto it = remainingMs.find(c.id); it != remainingMs.end() && it->second > 0) {
            longest = std::max(longest, static_cast<double>(it->second) / 1000.0);
        }
    }
    return longest;
}

} // namespace bs::core::requirements
