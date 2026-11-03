#include "../src/core.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace mywarkey;
using Events = std::vector<std::pair<int,bool>>;
void require(bool value) { if (!value) throw std::runtime_error("Combination test failed"); }
int main() {
    Events sent; auto send = [&](int key,bool down) { sent.emplace_back(key,down); };
    Profile p{"Game",false,{},{{{'A','B'},'C'},{{'A','B','C'},'D'}}};
    Remapper engine;
    require(engine.event('A',true,p,true,send,0));
    require(engine.event('B',true,p,true,send,20)); require(sent.empty());
    require(engine.event('C',true,p,true,send,40)); require(sent == Events{{'D',true}});
    require(engine.event('B',false,p,true,send,50)); engine.tick(90,p,true,send); require(sent == Events{{'D',true},{'D',false}});
    engine.event('A',false,p,true,send,95); engine.event('C',false,p,true,send,95);
    sent.clear();
    engine.event('B',true,p,true,send,100); engine.event('A',true,p,true,send,110);
    engine.tick(250,p,true,send); require(sent == Events{{'C',true}});
    engine.event('A',false,p,true,send,260); engine.event('B',false,p,true,send,260); engine.tick(300,p,true,send);
    require(sent == Events{{'C',true},{'C',false}});
    // Incomplete taps retain normal behavior, including single-key mappings.
    sent.clear(); p.mappings['A'] = 'X';
    engine.event('A',true,p,true,send,300); require(sent.empty());
    engine.event('A',false,p,true,send,310); engine.tick(360,p,true,send); require(sent == Events{{'X',true},{'X',false}});
    sent.clear();
    engine.event('B',true,p,true,send,400); engine.tick(550,p,true,send);
    engine.event('B',false,p,true,send,560); engine.tick(600,p,true,send); require(sent == Events{{'B',true},{'B',false}});
    // Pause/profile changes discard unfinished chords, release targets once,
    // and consume physical releases without leaking original keys.
    sent.clear(); engine.event('A',true,p,true,send,600); engine.release(send);
    require(sent.empty() && engine.event('A',false,p,false,send,620));
    engine.event('A',true,p,true,send,700); engine.event('B',true,p,true,send,710); engine.event('C',true,p,true,send,720);
    engine.release(send); engine.release(send); require(sent == Events{{'D',true},{'D',false}});
    engine.event('A',false,p,false,send,730); engine.event('B',false,p,false,send,730); engine.event('C',false,p,false,send,730);
    // Shared targets pair with existing one-key remaps rather than releasing
    // another held mapping's destination prematurely.
    sent.clear(); p.mappings['Z'] = 'D';
    engine.event('Z',true,p,true,send,800); engine.event('A',true,p,true,send,810); engine.event('B',true,p,true,send,820); engine.event('C',true,p,true,send,830);
    engine.event('A',false,p,true,send,840); require(sent == Events{{'D',true}});
    engine.event('Z',false,p,true,send,850); require(sent == Events{{'D',true},{'D',false}});
    engine.release(send);
    Settings settings; settings.profiles[0].combinations = p.combinations; settings.quickChat[3] = "诸葛亮"; settings.quickChatSlot = 3;
    std::stringstream file; writeSettings(file,settings); auto loaded = readSettings(file);
    require(loaded.profiles[0].combinations.size() == 2 && loaded.profiles[0].combinations[1].keys == std::vector<int>{'A','B','C'});
    require(loaded.quickChat == settings.quickChat && loaded.quickChatSlot == 3);
    std::stringstream oldFile;
    oldFile << "MYWARKEY 3 1 2 1 1 1 \"First\" 0 1 65 90 \"Second\" 0 0 QUICKCHAT 10 ";
    for (int i = 0; i < 10; ++i) oldFile << std::quoted(i == 0 ? std::string("诸葛亮") : std::string{}) << ' ';
    auto old = readSettings(oldFile);
    require(old.selected == 1 && old.blockWindows && old.quickChat[0] == "诸葛亮" && old.profiles[0].combinations.empty());
    std::stringstream migrated; writeSettings(migrated,old); auto again = readSettings(migrated);
    require(again.quickChat == old.quickChat && again.profiles[0].mappings == old.profiles[0].mappings);
    for (auto keys : {std::vector<int>{'A'}, std::vector<int>{'A','A'}, std::vector<int>{'A','B','C','D'}}) {
        bool failed = false; try { combinationKeys(keys); } catch (...) { failed = true; } require(failed);
    }
    std::cout << "Combination order, overlap, fallback, cleanup, shared targets and persistence passed.\n";
}
