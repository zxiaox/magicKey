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
    // One physical D produces multiple output keys; injected outputs are not
    // fed through the profile's ordinary A -> X mapping.
    sent.clear(); Remapper together;
    Profile output{"Output",false,{{'A','X'}},{{{'A','B','C'},'D',CombinationMode::Together}}};
    require(together.event('D',true,output,true,send,0));
    require(sent == Events{{'A',true},{'B',true},{'C',true}});
    together.event('D',true,output,true,send,10); together.event('D',false,output,true,send,20);
    together.tick(60,output,true,send);
    require(sent == Events{{'A',true},{'B',true},{'C',true},{'C',false},{'B',false},{'A',false}});
    sent.clear(); Remapper sequence;
    output.combinations = {{{'B','A','B'},'D',CombinationMode::Sequence}};
    sequence.event('D',true,output,true,send,0); sequence.event('D',false,output,true,send,10);
    sequence.tick(60,output,true,send); sequence.tick(80,output,true,send);
    sequence.tick(140,output,true,send); sequence.tick(160,output,true,send); sequence.tick(220,output,true,send);
    require(sent == Events{{'B',true},{'B',false},{'A',true},{'A',false},{'B',true},{'B',false}});
    sent.clear(); sequence.event('D',true,output,true,send,300); sequence.release(send);
    sequence.tick(1000,output,true,send); require(sent == Events{{'B',true},{'B',false}});
    sent.clear(); Remapper modifiers;
    output.combinations = {{{'C',0xA2},'D',CombinationMode::Together}};
    modifiers.event('D',true,output,true,send,0); modifiers.tick(60,output,true,send);
    require(sent == Events{{0xA2,true},{'C',true},{'C',false},{0xA2,false}});
    output.combinations = {{{'B','A','B'},'D',CombinationMode::Sequence}};
    Settings macroSettings; macroSettings.profiles[0].combinations = output.combinations;
    std::stringstream macroFile; writeSettings(macroFile,macroSettings); auto macroRead = readSettings(macroFile);
    require(macroRead.profiles[0].combinations[0].mode == CombinationMode::Sequence && macroRead.profiles[0].combinations[0].keys == std::vector<int>{'B','A','B'});
    require(combinationKeys({0xA2,'C'},CombinationMode::Together) == std::vector<int>{0xA2,'C'});
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
    std::stringstream legacyChord;
    legacyChord << "MYWARKEY 4 0 1 0 1 0 0 \"Old\" 0 0 1 2 65 66 68 QUICKCHAT 10 ";
    for (int i=0;i<10;++i) legacyChord << "\"\" ";
    require(readSettings(legacyChord).profiles[0].combinations[0].mode == CombinationMode::InputChord);
    for (auto keys : {std::vector<int>{'A'}, std::vector<int>{'A','A'}}) {
        bool failed = false; try { combinationKeys(keys); } catch (...) { failed = true; } require(failed);
    }
    // Five-key sequences preserve order, use the configured onset interval,
    // and still pair every down/up. Blank UI fields survive round-tripping.
    sent.clear(); Remapper five;
    Profile fiveProfile{"Five",false,{},{{{'A','B','C','D','E'},'X',CombinationMode::Sequence}}};
    fiveProfile.sequenceIntervalMs=180;
    five.event('X',true,fiveProfile,true,send,0); five.event('X',false,fiveProfile,true,send,10);
    for (int i=0;i<5;++i) {
        five.tick(i*180+60,fiveProfile,true,send);
        require(sent.size() == std::size_t((i+1)*2));
        if (i<4) { five.tick((i+1)*180-1,fiveProfile,true,send); require(sent.size()==std::size_t((i+1)*2)); five.tick((i+1)*180,fiveProfile,true,send); }
    }
    require(sent == Events{{'A',true},{'A',false},{'B',true},{'B',false},{'C',true},{'C',false},{'D',true},{'D',false},{'E',true},{'E',false}});
    Settings fiveSettings; fiveSettings.sequenceIntervalMs=180;
    fiveSettings.profiles[0].combinations={{{'A','B','D'},'X',CombinationMode::Sequence,{'A','B',0,'D',0}}};
    std::stringstream fiveFile; writeSettings(fiveFile,fiveSettings); auto fiveRead=readSettings(fiveFile);
    require(fiveRead.sequenceIntervalMs==180 && fiveRead.profiles[0].combinations[0].fields==std::vector<int>{'A','B',0,'D',0});
    require(fiveRead.profiles[0].combinations[0].keys==std::vector<int>{'A','B','D'});
    std::stringstream oldMacro;
    oldMacro << "MYWARKEY 5 0 1 0 1 0 0 \"Old\" 0 0 1 2 2 65 66 88 QUICKCHAT 10 ";
    for (int i=0;i<10;++i) oldMacro << "\"\" ";
    auto oldMacroRead=readSettings(oldMacro);
    require(oldMacroRead.sequenceIntervalMs==80 && oldMacroRead.profiles[0].combinations[0].keys==std::vector<int>{'A','B'});
    for (int invalid : {79,181}) {
        std::stringstream badSpeed; badSpeed << "MYWARKEY 6 0 1 0 1 0 0 " << invalid;
        bool rejected=false; try { (void)readSettings(badSpeed); } catch (...) { rejected=true; } require(rejected);
    }
    // Slot growth has no five-key ceiling; long sequences may repeat keys.
    std::vector<int> longKeys(40,'A');
    require(combinationKeys(longKeys,CombinationMode::Sequence).size()==40);
    Settings longSettings; longSettings.warcraftPath=u8"C:\\Warcraft 诸葛亮\\war3.exe";
    longSettings.profiles[0].combinations={{{'A','B','A','B','A','B'},'X',CombinationMode::Sequence,{'A','B','A',0,'B','A','B',0,0,0}}};
    std::stringstream longFile; writeSettings(longFile,longSettings); auto longRead=readSettings(longFile);
    require(longRead.warcraftPath==longSettings.warcraftPath);
    require(longRead.profiles[0].combinations[0].fields.size()==10 && longRead.profiles[0].combinations[0].keys.size()==6);
    sent.clear(); Remapper longRun;
    Profile longProfile{"Long",false,{},{{{'A','B','A','B','A','B'},'X',CombinationMode::Sequence}}};
    longRun.event('X',true,longProfile,true,send,0); longRun.event('X',false,longProfile,true,send,1);
    for (int i=0;i<6;++i) { longRun.tick(i*80+60,longProfile,true,send); if (i<5) longRun.tick((i+1)*80,longProfile,true,send); }
    require(sent.size()==12 && sent.back()==std::make_pair(int('B'),false));
    std::cout << "Combination order, overlap, fallback, cleanup, shared targets and persistence passed.\n";
}
