// Rendering approach adapted from W3ManaBars by Hr0ffT (MIT).
// 1.27a addresses and layouts cross-referenced with War3VK's public IDA exports.
#include <windows.h>
#include <winver.h>
#include <array>
#include <cstdint>
#include <cstring>

namespace {
using Address = std::uintptr_t;
using Alloc = void* (__stdcall*)(unsigned, const char*, unsigned, unsigned);
using Construct = void (__thiscall*)(Address, Address, Address, unsigned);
using SetPoint = void (__thiscall*)(Address, unsigned, Address, unsigned, float, float, unsigned);
using SetHeight = void (__thiscall*)(Address, float);
using Bind = void (__fastcall*)(Address, Address, Address);
using Destroy = Address (__fastcall*)(Address, Address, unsigned);
using SetVisible = int (__thiscall*)(Address);

constexpr Address StatBarTable = 0x98F52C;
constexpr Address StatBarConstructor = 0x383F60;
constexpr Address ConstructorTableAssignment = 0x383F94;
constexpr Address StormAllocate = 0x120628;
constexpr Address SetFramePoint = 0x0BD8A0;
constexpr Address SetFrameHeight = 0x0BD7C0;
constexpr Address GameWar3 = 0xBE4238;
constexpr unsigned BarSize = 0x158;
constexpr unsigned Parent = 0x6C, FrameLevel = 0x84, WantShown = 0x90, Shown = 0x94;
constexpr unsigned BarType = 0x138, BoundUnit = 0x154;
constexpr unsigned OwnerSlot = 0x58, HeroAbility = 0x1F0, LocalPlayerSlot = 0x28;
constexpr unsigned SmallHealth = 5, SmallMana = 4;
constexpr unsigned DestroySlot = 0x08 / 4, HideSlot = 0x64 / 4, ShowSlot = 0x68 / 4;
constexpr unsigned BindUnitSlot = 0x74 / 4, BindWidgetSlot = 0x78 / 4;

Address gameBase;
Bind bindUnit, bindWidget;
Destroy destroy;

struct BarPair { Address health, mana; };
std::array<BarPair, 64> bars{};

template<class T = Address>
T& at(Address object, unsigned offset) { return *reinterpret_cast<T*>(object + offset); }

Address* table(Address frame) { return reinterpret_cast<Address*>(at(frame, 0)); }

bool isWorldHealthBar(Address frame) {
    return at(frame, BarType) == SmallHealth && !at(frame, Parent);
}

bool isLocalHero(Address unit) {
    const Address game = at(gameBase, GameWar3);
    if (!unit || !game) return false;
    const auto local = at<unsigned>(game, LocalPlayerSlot);
    return local < 12 && at<unsigned>(unit, OwnerSlot) == local && at(unit, HeroAbility);
}

BarPair* findBar(Address health) {
    for (auto& bar : bars) if (bar.health == health) return &bar;
    return nullptr;
}

void show(Address bar, bool visible) {
    at(bar, WantShown) = visible;
    if (bool(at(bar, Shown)) != visible)
        reinterpret_cast<SetVisible>(table(bar)[visible ? ShowSlot : HideSlot])(bar);
}

Address createBar(Address health, Address unit) {
    for (auto& bar : bars) {
        if (bar.health) continue;
        const auto allocate = reinterpret_cast<Alloc>(gameBase + StormAllocate);
        const auto mana = reinterpret_cast<Address>(allocate(BarSize, "MagicKeyMana", 1, 0));
        if (!mana) return 0;
        reinterpret_cast<Construct>(gameBase + StatBarConstructor)(mana, health, unit, SmallMana);
        const auto point = reinterpret_cast<SetPoint>(gameBase + SetFramePoint);
        point(mana, 0, health, 6, 0.0f, -0.001f, 1);
        point(mana, 2, health, 8, 0.0f, -0.001f, 1);
        reinterpret_cast<SetHeight>(gameBase + SetFrameHeight)(mana, 0.004f);
        bar = {health, mana};
        return mana;
    }
    return 0;
}

void __fastcall onBindUnit(Address health, Address edx, Address unit) {
    bindUnit(health, edx, unit);
    if (!isWorldHealthBar(health)) return;
    const auto bar = findBar(health);
    if (!isLocalHero(unit)) {
        if (bar) show(bar->mana, false);
        return;
    }
    const auto mana = bar ? bar->mana : createBar(health, unit);
    if (!mana) return;
    if (at(mana, FrameLevel) != at(health, FrameLevel)) {
        show(mana, false);
        at(mana, FrameLevel) = at(health, FrameLevel);
    }
    show(mana, true);
    bindUnit(mana, 0, unit);
}

void __fastcall onBindWidget(Address health, Address edx, Address widget) {
    bindWidget(health, edx, widget);
    if (const auto bar = findBar(health); bar && (!widget || widget != at(bar->mana, BoundUnit)))
        show(bar->mana, false);
}

Address __fastcall onDestroy(Address health, Address edx, unsigned flags) {
    if (auto bar = findBar(health)) {
        const auto mana = bar->mana;
        *bar = {};
        destroy(mana, 0, 1);
    } else {
        for (auto& bar : bars) {
            if (bar.mana != health) continue;
            bar = {};
            break;
        }
    }
    return destroy(health, edx, flags);
}

bool matchesVersion(HMODULE game) {
    wchar_t path[MAX_PATH]{};
    const auto length = GetModuleFileNameW(game, path, MAX_PATH);
    if (!length || length >= MAX_PATH) return false;
    // Classic Game.dll has a small version resource. Reject larger or invalid resources.
    alignas(VS_FIXEDFILEINFO) unsigned char version[4096];
    DWORD ignored = 0;
    const auto size = GetFileVersionInfoSizeW(path, &ignored);
    if (!size || size > sizeof(version) || !GetFileVersionInfoW(path, 0, size, version)) return false;
    void* value = nullptr;
    UINT bytes = 0;
    if (!VerQueryValueW(version, L"\\", &value, &bytes) || bytes < sizeof(VS_FIXEDFILEINFO)) return false;
    const auto& info = *static_cast<VS_FIXEDFILEINFO*>(value);
    return info.dwSignature == 0xFEEF04BD && info.dwFileVersionMS == 0x0001001B &&
           info.dwFileVersionLS == 52240;
}

bool executableAddress(Address address) {
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region)) ||
        reinterpret_cast<Address>(region.AllocationBase) != gameBase || region.State != MEM_COMMIT)
        return false;
    return (region.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

void install(HMODULE self) {
    const auto game = GetModuleHandleW(L"Game.dll");
    if (!game || !matchesVersion(game)) return;
    gameBase = reinterpret_cast<Address>(game);
    if (!executableAddress(gameBase + StatBarConstructor) || !executableAddress(gameBase + SetFramePoint) ||
        !executableAddress(gameBase + SetFrameHeight) || !executableAddress(gameBase + StormAllocate))
        return;
    if (at<std::uint8_t>(gameBase, StatBarConstructor) != 0x55 ||
        at<std::uint8_t>(gameBase, SetFramePoint) != 0x55 ||
        at<std::uint8_t>(gameBase, SetFrameHeight) != 0x55 ||
        at<std::uint16_t>(gameBase, StormAllocate) != 0x25FF)
        return;
    const auto assignment = gameBase + ConstructorTableAssignment;
    Address tableAddress = 0;
    std::memcpy(&tableAddress, reinterpret_cast<const void*>(assignment + 2), sizeof(tableAddress));
    if (at<std::uint16_t>(assignment, 0) != 0x07C7 || tableAddress != gameBase + StatBarTable) return;
    auto slots = reinterpret_cast<Address*>(tableAddress);
    if (!executableAddress(slots[DestroySlot]) || !executableAddress(slots[BindUnitSlot]) ||
        !executableAddress(slots[BindWidgetSlot]) || !executableAddress(slots[HideSlot]) ||
        !executableAddress(slots[ShowSlot]))
        return;

    HMODULE pinned = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCWSTR>(self), &pinned))
        return;
    constexpr auto size = (BindWidgetSlot + 1) * sizeof(Address);
    DWORD protection = 0;
    if (!VirtualProtect(slots, size, PAGE_READWRITE, &protection)) return;
    bindUnit = reinterpret_cast<Bind>(slots[BindUnitSlot]);
    bindWidget = reinterpret_cast<Bind>(slots[BindWidgetSlot]);
    destroy = reinterpret_cast<Destroy>(slots[DestroySlot]);
    slots[DestroySlot] = reinterpret_cast<Address>(onDestroy);
    slots[BindUnitSlot] = reinterpret_cast<Address>(onBindUnit);
    slots[BindWidgetSlot] = reinterpret_cast<Address>(onBindWidget);
    VirtualProtect(slots, size, protection, &protection);
}
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) install(module);
    return TRUE;
}
