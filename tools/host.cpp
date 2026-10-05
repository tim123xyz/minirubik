#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <array>
#include <errno.h>
#include <stddef.h>
#include <chrono>
#include <vector>
namespace oracle {
#define main oracle_main
#include "../solver.c"
#undef main
}
static std::vector<unsigned char> returned;
static int capture(const char *, const char *, const char *name) {
    static const char *names[] = {"R","R2","R'","B","B2","B'","D","D2","D'"};
    for(int i=0;i<9;i++) if(!strcmp(name,names[i])) returned.push_back(i);
    return 0;
}
static int discard(int) {return 0;}
namespace candidate {
#define main candidate_main
#define printf capture
#define putchar discard
// The existing builders call non-constexpr helpers. Build the same transition
// tables during host startup; leave all search and state operations unchanged.
#define consteval
#define constexpr const
#include "../min.cpp"
#undef constexpr
#undef consteval
#undef putchar
#undef printf
#undef main
}
int main(int argc,char **argv) {
    setbuf(stdout,nullptr);
    auto start=std::chrono::steady_clock::now();
    uint8_t diameter;
    auto table=oracle::build_table(&diameter);
    if(!table) return 1;
    std::vector<uint8_t> distance(oracle::STATES,255);
    distance[0]=0;
    unsigned maximum=0;
    for(unsigned r=1;r<oracle::STATES;r++) {
        oracle::state_t s; oracle::unrank_state(r,&s);
        unsigned n=0;
        while(oracle::rank_state(&s)) {
            auto k=oracle::rank_state(&s);
            if(table[k]>=9||++n>11) return 2;
            s=oracle::apply_move(s,table[k]);
        }
        distance[r]=n; if(n>maximum) maximum=n;
    }
    printf("Oracle: states=%u diameter=%u reconstructed maximum=%u\n",oracle::STATES,diameter,maximum);
    for(unsigned p=0;p<5040;p++) for(unsigned o=0;o<729;o++) {
        // Every independent transition entry is checked against solver.c.
        if(o && p) continue;
        oracle::state_t s; oracle::unrank_state(p*729+o,&s);
        for(unsigned f=0;f<3;f++) {
            auto n=oracle::quarter_turn(s,f); auto r=oracle::rank_state(&n);
            if(candidate::permutation[f][p]!=r/729 || candidate::orientation[f][o]!=r%729) return 3;
        }
    }
    printf("H1: N/A (no heuristic); H2: all 15120 permutation and 2187 orientation transitions match oracle, solved transitions verified\n");
    unsigned mp=0,mo=0;
    for(auto &a:candidate::permutation) for(auto v:a) if(v>mp) mp=v;
    for(auto &a:candidate::orientation) for(auto v:a) if(v>mo) mo=v;
    printf("H2: transition maxima p=%u o=%u; bidirectional table intentionally partial, not a full distance table\n",mp,mo);
    std::vector<uint8_t> ref(oracle::STATES);
    for(unsigned i=0;i<ref.size();i++) candidate::set_table(ref.data(),i,i%256);
    for(unsigned i=0;i<ref.size();i++) if(candidate::get_table(ref.data(),i)!=i%256) return 4;
    printf("H4: byte accessor agrees at all even/odd indices; no packed accessor\n");
    unsigned limit=argc>1?strtoul(argv[1],nullptr,10):oracle::STATES;
    auto search_start=std::chrono::steady_clock::now();
    for(unsigned r=0;r<limit;r++) {
        oracle::state_t s; oracle::unrank_state(r,&s);
        char code[15]; for(int i=0;i<7;i++) {code[i]='1'+s.p[i];code[7+i]='1'+s.o[i];} code[14]=0;
        char program[]="min"; char *args[]={program,code,nullptr};
        returned.clear();
        int rc=candidate::candidate_main(2,args);
        if(rc||returned.size()!=distance[r]) {printf("H3 FAIL rank=%u state=%s rc=%d length=%zu exact=%u\n",r,code,rc,returned.size(),distance[r]);return 5;}
        for(auto m:returned) s=oracle::apply_move(s,m);
        if(oracle::rank_state(&s)) {printf("H3 FAIL path rank=%u\n",r); return 6;}
        if(r%10000==0) printf("H3 progress %u/%u elapsed=%.3fs\n",r,limit,std::chrono::duration<double>(std::chrono::steady_clock::now()-search_start).count());
    }
    printf("H3 %s checked=%u search wall=%.3fs total wall=%.3fs diameter=%u\n",limit==oracle::STATES?"PASS":"PARTIAL",limit,std::chrono::duration<double>(std::chrono::steady_clock::now()-search_start).count(),std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(),maximum);
}
