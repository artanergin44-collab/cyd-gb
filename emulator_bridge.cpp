#include "emulator_bridge.h"
#include "display.h"
#include "hw_config.h"
#include <Arduino.h>
#include <string.h>
#include <SD.h>
#include <SPIFFS.h>

#define ENABLE_LCD 1
#define ENABLE_SOUND 0
#define PEANUT_GB_HIGH_LCD_ACCURACY 0
#include "peanut_gb.h"

// ─── Page cache ─────────────────────────────────────────────────────────────
#define PG_SZ 4096
#define PG_N  16
#define PG_MASK (PG_SZ-1)
#define HASH_SZ 32
#define HASH_M (HASH_SZ-1)

struct Pg { uint32_t addr, acc; uint8_t* d; bool v; };
static Pg pg[PG_N];
static int8_t ht[HASH_SZ];
static uint32_t acc = 0;
static int npg = 0;
static File romf;
static uint32_t romlen = 0;

#define B0SZ (32*1024)
static uint8_t* b0 = nullptr;

static inline uint8_t* IRAM_ATTR cget(uint32_t a) {
    uint32_t pb = a & ~PG_MASK;
    int8_t i = ht[(pb>>12)&HASH_M];
    if (i >= 0 && pg[i].v && pg[i].addr == pb) { pg[i].acc = ++acc; return &pg[i].d[a&PG_MASK]; }
    for (int j=0;j<npg;j++) if (pg[j].v && pg[j].addr==pb) {
        pg[j].acc=++acc; ht[(pb>>12)&HASH_M]=j; return &pg[j].d[a&PG_MASK];
    }
    int lru=0; uint32_t old=UINT32_MAX;
    for (int j=0;j<npg;j++) { if (!pg[j].v){lru=j;break;} if(pg[j].acc<old){old=pg[j].acc;lru=j;} }
    if (pg[lru].v) { int8_t oh=(pg[lru].addr>>12)&HASH_M; if(ht[oh]==lru) ht[oh]=-1; }
    romf.seek(pb); size_t r=romf.read(pg[lru].d, min((uint32_t)PG_SZ,romlen-pb));
    if (r<PG_SZ) memset(pg[lru].d+r,0xFF,PG_SZ-r);
    pg[lru].addr=pb; pg[lru].acc=++acc; pg[lru].v=true; ht[(pb>>12)&HASH_M]=lru;
    return &pg[lru].d[a&PG_MASK];
}

// ─── State ──────────────────────────────────────────────────────────────────
static struct gb_s* gb = nullptr;
#define MAXRAM (32*1024)
static uint8_t* cram = nullptr;
static uint16_t lbuf[GB_SCREEN_W];
static uint8_t fskip = 0, fcnt = 0;
static uint32_t fpsc = 0, fpst = 0, cfps = 0;
static uint8_t jpad = 0;

// ─── 20 Palettes (byte-swapped for pushImage) ──────────────────────────────
#define SW(c) (uint16_t)(((c)>>8)|((c)<<8))

static const uint16_t pals[NUM_PALETTES][4] = {
    {SW(0x9FE5),SW(0x4F64),SW(0x2542),SW(0x0261)}, //  0 Classic Green
    {SW(0xFFFF),SW(0xAD55),SW(0x52AA),SW(0x0000)}, //  1 Original DMG
    {SW(0xFFFF),SW(0xB596),SW(0x6B4D),SW(0x0000)}, //  2 Pocket Gray
    {SW(0xFFDF),SW(0xD68F),SW(0x7A4B),SW(0x1082)}, //  3 Warm Sepia
    {SW(0xBF5F),SW(0x6CDF),SW(0x339F),SW(0x0019)}, //  4 Cool Blue
    {SW(0xFFF0),SW(0xFC00),SW(0x8800),SW(0x2000)}, //  5 Autumn
    {SW(0xE71C),SW(0x9CD3),SW(0x4228),SW(0x0000)}, //  6 Grayscale
    {SW(0xFFFF),SW(0xFE20),SW(0xC800),SW(0x4000)}, //  7 Lava
    {SW(0xAFFF),SW(0x5F5F),SW(0x2D1F),SW(0x0019)}, //  8 Ocean
    {SW(0xFFF0),SW(0xBDE0),SW(0x5AE0),SW(0x0120)}, //  9 Forest
    {SW(0xFFFF),SW(0xFD20),SW(0xAB00),SW(0x4000)}, // 10 Sunset
    {SW(0xFFDF),SW(0xF71C),SW(0xAA13),SW(0x3808)}, // 11 Cherry
    {SW(0xCFFF),SW(0x867F),SW(0x433F),SW(0x0019)}, // 12 Ice
    {SW(0xFFB6),SW(0xD52A),SW(0x8A08),SW(0x3000)}, // 13 Chocolate
    {SW(0xFFFF),SW(0xBF5F),SW(0x5F1F),SW(0x0019)}, // 14 Mint
    {SW(0xFFF8),SW(0xFCC0),SW(0xC880),SW(0x6000)}, // 15 Peach
    {SW(0xEF3C),SW(0x867F),SW(0x4179),SW(0x0000)}, // 16 Lavender
    {SW(0xFFFF),SW(0x07FF),SW(0x001F),SW(0x0000)}, // 17 Neon
    {SW(0x0000),SW(0x4228),SW(0xAD55),SW(0xFFFF)}, // 18 Inverted
    {SW(0xFE60),SW(0xAB00),SW(0x5000),SW(0x0000)}, // 19 Gold
};
static const char* palnames[NUM_PALETTES] = {
    "Classic Green","Original DMG","Pocket Gray","Warm Sepia","Cool Blue",
    "Autumn","Grayscale","Lava","Ocean","Forest",
    "Sunset","Cherry","Ice","Chocolate","Mint",
    "Peach","Lavender","Neon","Inverted","Gold"
};
static uint8_t curpal = 0;

void emu_set_palette(uint8_t i) { if (i<NUM_PALETTES) curpal=i; }
uint8_t emu_get_palette() { return curpal; }
const char* emu_get_palette_name(uint8_t i) { return (i<NUM_PALETTES)?palnames[i]:"?"; }

// ─── Callbacks ──────────────────────────────────────────────────────────────
static uint8_t IRAM_ATTR gb_rom_read(struct gb_s* g, const uint_fast32_t a) {
    (void)g; if(a>=romlen) return 0xFF; if(a<B0SZ) return b0[a]; return *cget(a);
}
static uint8_t IRAM_ATTR gb_cram_r(struct gb_s* g, const uint_fast32_t a) {
    (void)g; return (a<MAXRAM)?cram[a]:0xFF;
}
static void IRAM_ATTR gb_cram_w(struct gb_s* g, const uint_fast32_t a, const uint8_t v) {
    (void)g; if(a<MAXRAM) cram[a]=v;
}
static void gb_err(struct gb_s* g, const enum gb_error_e e, const uint16_t a) {
    (void)g; Serial.printf("[EMU] Err %d @0x%04X\n",(int)e,a);
}
static void IRAM_ATTR lcd_line(struct gb_s* g, const uint8_t px[160], const uint_fast8_t ln) {
    (void)g;
    if (fskip>0 && (fcnt%(fskip+1))!=0) return;
    const uint16_t* p = pals[curpal];
    for (int x=0;x<GB_SCREEN_W;x++) lbuf[x]=p[px[x]&3];
    display_push_gb_line(ln, lbuf);
}

// ─── SPIFFS copy ────────────────────────────────────────────────────────────
static bool cp2spiffs(const char* sp, const char* dp) {
    File s=SD.open(sp,FILE_READ); if(!s) return false;
    File d=SPIFFS.open(dp,FILE_WRITE); if(!d){s.close();return false;}
    uint8_t buf[512]; uint32_t tot=0;
    while(s.available()){size_t r=s.read(buf,512);d.write(buf,r);tot+=r;
        if(tot%65536==0) Serial.printf("[SPIFFS] %uKB\n",tot/1024);}
    d.close();s.close();
    Serial.printf("[SPIFFS] Done %u bytes\n",tot); return true;
}

// ─── API ────────────────────────────────────────────────────────────────────
bool emu_open_rom(const char* path) {
    if(!SPIFFS.begin(true)) Serial.println("[SPIFFS] format");
    String sn="/rom.gb";
    if(SPIFFS.exists(sn)){
        File sc=SD.open(path,FILE_READ); uint32_t ssz=sc?sc.size():0; if(sc)sc.close();
        romf=SPIFFS.open(sn,FILE_READ);
        if(romf && romf.size()==ssz){romlen=romf.size();Serial.printf("[EMU] SPIFFS %uKB\n",romlen/1024);return true;}
        if(romf) romf.close();
    }
    File sf=SD.open(path,FILE_READ); if(!sf) return false;
    uint32_t sz=sf.size(); sf.close();
    if(sz<=SPIFFS.totalBytes()-SPIFFS.usedBytes()){
        Serial.println("[EMU] Copying to SPIFFS...");
        if(SPIFFS.exists(sn)) SPIFFS.remove(sn);
        if(cp2spiffs(path,sn.c_str())){
            romf=SPIFFS.open(sn,FILE_READ);
            if(romf){romlen=romf.size();return true;}
        }
    }
    romf=SD.open(path,FILE_READ); if(!romf) return false;
    romlen=romf.size(); return true;
}
void emu_close_rom(){if(romf)romf.close();romlen=0;}

bool emu_init(uint8_t*,uint32_t) {
    if(!romf||!romlen) return false;
    memset(ht,-1,sizeof(ht));
    npg=0;
    for(int i=0;i<PG_N;i++){pg[i].v=false;if(!pg[i].d)pg[i].d=(uint8_t*)malloc(PG_SZ);if(!pg[i].d)break;npg++;}
    Serial.printf("[EMU] %d pages\n",npg);
    if(!b0) b0=(uint8_t*)malloc(B0SZ); if(!b0) return false;
    romf.seek(0); romf.read(b0,min(romlen,(uint32_t)B0SZ));
    if(!cram) cram=(uint8_t*)malloc(MAXRAM); if(!cram) return false;
    memset(cram,0xFF,MAXRAM);
    if(!gb) gb=(struct gb_s*)malloc(sizeof(struct gb_s)); if(!gb) return false;
    memset(gb,0,sizeof(struct gb_s));
    enum gb_init_error_e r=gb_init(gb,gb_rom_read,gb_cram_r,gb_cram_w,gb_err,nullptr);
    if(r!=GB_INIT_NO_ERROR){Serial.printf("[EMU] init fail %d\n",(int)r);return false;}
    gb_init_lcd(gb,lcd_line);
    fcnt=fpsc=cfps=0; fpst=millis(); acc=0;
    char t[17]={0}; for(int i=0;i<16;i++){char c=(char)b0[0x134+i];t[i]=(c>=32&&c<127)?c:0;}
    Serial.printf("[EMU] '%s' %uKB heap:%u\n",t,romlen/1024,ESP.getFreeHeap());
    return true;
}

void emu_run_frame() {
    gb->direct.joypad_bits.a=!(jpad&0x10); gb->direct.joypad_bits.b=!(jpad&0x20);
    gb->direct.joypad_bits.select=!(jpad&0x40); gb->direct.joypad_bits.start=!(jpad&0x80);
    gb->direct.joypad_bits.right=!(jpad&0x01); gb->direct.joypad_bits.left=!(jpad&0x02);
    gb->direct.joypad_bits.up=!(jpad&0x04); gb->direct.joypad_bits.down=!(jpad&0x08);
    gb_run_frame(gb); fcnt++; fpsc++;
    uint32_t n=millis(); if(n-fpst>=1000){cfps=fpsc;fpsc=0;fpst=n;}
}

void emu_set_joypad(uint8_t b){jpad=b;}
uint8_t* emu_get_cart_ram(uint32_t* s){uint_fast32_t r=0;gb_get_save_size_s(gb,&r);if(s)*s=(uint32_t)r;return cram;}
void emu_set_cart_ram(const uint8_t* d,uint32_t s){if(s>MAXRAM)s=MAXRAM;memcpy(cram,d,s);}
void emu_set_frame_skip(uint8_t s){fskip=s;}
uint8_t emu_get_frame_skip(){return fskip;}
uint32_t emu_get_fps(){return cfps;}
uint16_t* emu_get_line_buffer(){return lbuf;}
void emu_reset(){gb_reset(gb);fcnt=0;}
