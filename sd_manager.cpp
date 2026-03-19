#include "sd_manager.h"
#include "hw_config.h"
#include <SD.h>
#include <SPI.h>
#include <Arduino.h>

static SPIClass sdSPI(VSPI);
static bool ready = false;

bool sd_init() {
    sdSPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
    if(!SD.begin(SD_PIN_CS, sdSPI, 20000000)){Serial.println("[SD] Mount fail!");return false;}
    Serial.printf("[SD] Type:%d Size:%lluMB\n",SD.cardType(),SD.cardSize()/(1024*1024));
    if(!SD.exists(ROM_PATH_GB)) SD.mkdir(ROM_PATH_GB);
    if(!SD.exists(ROM_PATH_GBC)) SD.mkdir(ROM_PATH_GBC);
    if(!SD.exists(SAVE_PATH)) SD.mkdir(SAVE_PATH);
    ready=true; return true;
}

static int scan_dir(const char* dir, bool gbc, RomEntry* l, int si, int mx) {
    int c=si; File d=SD.open(dir); if(!d||!d.isDirectory()) return c;
    File e;
    while((e=d.openNextFile())&&c<mx) {
        if(e.isDirectory()){e.close();continue;}
        String n=e.name(); String lo=n; lo.toLowerCase();
        if(lo.endsWith(".gb")||lo.endsWith(".gbc")){
            strncpy(l[c].filename,n.c_str(),MAX_FILENAME-1);
            snprintf(l[c].full_path,80,"%s/%s",dir,n.c_str());
            l[c].size=e.size(); l[c].is_gbc=gbc; c++;
        }
        e.close();
    }
    d.close(); return c;
}

int sd_scan_roms(RomEntry* l, int mx) {
    if(!ready) return 0;
    int c=scan_dir(ROM_PATH_GB,false,l,0,mx);
    c=scan_dir(ROM_PATH_GBC,true,l,c,mx);
    // Sort
    for(int i=0;i<c-1;i++) for(int j=i+1;j<c;j++)
        if(strcasecmp(l[i].filename,l[j].filename)>0){RomEntry t=l[i];l[i]=l[j];l[j]=t;}
    Serial.printf("[SD] Found %d ROMs\n",c);
    return c;
}

bool sd_load_rom(const char* p, uint8_t** buf, uint32_t* sz) { return false; /* unused now */ }
void sd_free_rom(uint8_t* b) { if(b) free(b); }

void sd_get_save_path(const char* rp, char* sp, int mx) {
    const char* fn=strrchr(rp,'/'); if(!fn)fn=rp; else fn++;
    char base[MAX_FILENAME]; strncpy(base,fn,MAX_FILENAME-1); base[MAX_FILENAME-1]=0;
    char* dot=strrchr(base,'.'); if(dot)*dot=0;
    snprintf(sp,mx,"%s/%s.sav",SAVE_PATH,base);
}

bool sd_save_state(const char* rp, const uint8_t* data, uint32_t sz) {
    if(!ready||!data||!sz) return false;
    char sp[96]; sd_get_save_path(rp,sp,96);
    File f=SD.open(sp,FILE_WRITE); if(!f) return false;
    size_t w=f.write(data,sz); f.close();
    Serial.printf("[SD] Save: %s (%u)\n",sp,w);
    return w==sz;
}

bool sd_load_state(const char* rp, uint8_t* data, uint32_t sz) {
    if(!ready||!data||!sz) return false;
    char sp[96]; sd_get_save_path(rp,sp,96);
    if(!SD.exists(sp)) return false;
    File f=SD.open(sp,FILE_READ); if(!f) return false;
    size_t r=f.read(data,sz); f.close();
    Serial.printf("[SD] Load: %s (%u)\n",sp,r);
    return r==sz;
}
