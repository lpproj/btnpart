/*
  btnpart: `Better-Than-Nothing' partitioner for NEC PC-9801/9821.
  
  Copyright (C) 2015 sava (t.ebisawa)

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.


  (in short term: `under the ZLIB license') 

*/

#include <ctype.h>
#include <limits.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(LSI_C)
# define REGS2 REGS
# define int86x2 int86x
#else
# include "int86x2.h"
#endif

#if defined(__TURBOC__)
#include <alloc.h>
#endif

#if defined(_MSC_VER) || defined(__WATCOMC__)
#include <malloc.h>
#define _fmalloc  farmalloc
#define _ffree  farfree
#pragma pack(1)
#endif


#define VERSIONSTRING "0.0.1"

#define IPL_SIZE_MAX 1024
#define MBR_NAME "btnpart.mbr"
#define PBR_NAME "fat16hd.pbr"


typedef struct PTABLE_NEC98 {
  unsigned char bootable;
  unsigned char type;
  unsigned char dummy[2];
  unsigned char ipl_s;
  unsigned char ipl_h;
  unsigned short ipl_c;
  unsigned char start_s;
  unsigned char start_h;
  unsigned short start_c;
  unsigned char end_s;
  unsigned char end_h;
  unsigned short end_c;
  unsigned char name[16];
} PTABLE_NEC98;

typedef struct BPB_BOOTSECTOR {
  unsigned char jmpcode[3];
  char oemname[8];
  unsigned short bytes_per_sector;
  unsigned char sectors_per_cluster;
  unsigned short reserved_sectors;
  unsigned char fats;
  unsigned short rootdirs;
  unsigned short sectors16;
  unsigned char media_descriptor;
  unsigned short sectors_per_fat;
  unsigned short sectors_per_track;
  unsigned short heads;
  unsigned long hidden_sectors;
  unsigned long sectors32;
  unsigned char boot_drive_number;
  unsigned char boot_drive_head;
  unsigned char boot_signature;
  unsigned long volume_serial;
  char volume_label[11];
  char file_system[8];
} BPB_BOOTSECTOR;


typedef struct DISK_GEOMETRY {
  int daua;
  int medium_type;
  unsigned bps;
  unsigned long c;
  unsigned h;
  unsigned s;
} DISK_GEOMETRY;

enum {
  MEDIUM_TYPE_NONE = 0,
  MEDIUM_TYPE_HD
};


enum {
  DISK_PKT_MODE_CHS = 0,
  DISK_PKT_MODE_LBA
};

typedef struct DISK_PKT_CHS {
  int mode;
  unsigned short c;     /* CX */
  unsigned char s;      /* DL */
  unsigned char h;      /* DH */
} DISK_PKT_CHS;

typedef struct DISK_PKT_LBA {
  int mode;
  unsigned long lba;    /* DX:CX */
} DISK_PKT_LBA;

typedef union DISK_PKT {
  DISK_PKT_LBA  lba;
  DISK_PKT_CHS  chs;
} DISK_PKT;

int disp_debug = 0;
DISK_GEOMETRY diskinfo[16];


DISK_GEOMETRY *daua_to_diskinfo(int daua)
{
  DISK_GEOMETRY *g = NULL;
  int i;
  for(i=0; i<sizeof(diskinfo)/sizeof(diskinfo[0]); ++i) {
    if (diskinfo[i].daua == daua) {
      g = &diskinfo[i];
      break;
    }
  }
  return g;
}

unsigned long sectors_to_kilobytes(unsigned long sectors, unsigned short bytes_per_sector)
{
  /* convert count of sectors to kilobytes */
  int lshiftk;
  switch(bytes_per_sector) {
    case 128:   lshiftk = 3; break;
    case 256:   lshiftk = 2; break;
    case 512:   lshiftk = 1; break;
    case 1024:  lshiftk = 0; break;
    case 2048:  lshiftk = -1; break;
    case 4096:  lshiftk = -2; break;
    default:
      return 0;
  }
  return (lshiftk >= 0) ? sectors >> lshiftk : sectors << (-lshiftk);
}

unsigned long calc_kb_geom(unsigned long num_trk, const DISK_GEOMETRY *g)
{
  if (!num_trk || !g) return 0;
  return sectors_to_kilobytes(g->s * g->h * num_trk, g->bps);
}

int get_disk_geometry(int daua, DISK_GEOMETRY *g)
{
  int rc = 0;
  union REGS r;
  
  if (daua < 0 || daua > 0xff)
    return 0;
  r.x.bx = r.x.cx = r.x.dx = 0;
  r.h.al = daua;
  
  if ((daua & 0x50) == 0) {
    r.h.ah = 0x84;
    r.h.al = daua = (daua & 0x7f) | 0x80;
    int86(0x1b, &r, &r);
    if (!r.x.cflag && r.x.bx && r.h.dh && r.h.dl) {
      g->daua = daua;
      g->medium_type = rc = MEDIUM_TYPE_HD;
      g->bps = r.x.bx;
      g->c = r.x.cx;
      g->h = r.h.dh;
      g->s = r.h.dl;
#if 0
      printf("daua=0x%02x c=%u h=%u s=%u bps=%u (%luKB)\n", g->daua, g->c, g->h, g->s, g->bps, calc_kb_geom(g->c, g));
#endif
      r.h.ah = 0x8e;
      r.h.al = daua;
      int86(0x1b, &r, &r);  /* wake up, the HD */
    }
  }
  
  return rc;
}

int get_all_geometries(void)
{
  int n = 0;
  unsigned char eq;
  int i;
  
  /* SASI (IDE) HDs... */
  eq = *(unsigned char far *)0x0000055d;
  for(i=0; i<4; ++i) {
    if (eq & (1 << i)) {
      if (get_disk_geometry(0x80 + i, diskinfo + n))
        ++n;
    }
  }
  /* SCSI HDs... */
  eq = *(unsigned char far *)0x00000482;
  for(i=0; i<7; ++i) {
    if (eq & (1 << i)) {
      if (get_disk_geometry(0xa0 + i, diskinfo + n))
        ++n;
    }
  }
  
  return n;
}


int diskrw_hd(int iswrite, int daua, void *buf, DISK_PKT *pkt, unsigned bytecount)
{
  union REGS2 r;
  struct SREGS sr;
  void *tmpbuf;
  char far *prmbuf;
  char far *tmpdma;
  unsigned long tmpla;
  unsigned n;
  
  if (daua < 0 || daua >= 0xff || (daua & 0xd0) == 0 || bytecount > (INT_MAX/2)) return -1;
  
  prmbuf = (char far *)buf;
  
  /* allocate temporary buffer to avoid DMA boundary issue... */
  tmpbuf = malloc(bytecount * 2);
  if (!tmpbuf) return -1;
  tmpdma = (char far *)tmpbuf;
  tmpla = FP_SEG(tmpdma) * 16UL + FP_OFF(tmpdma);
  if ((tmpla & 0xffffUL) + bytecount > 0xffffUL) tmpla = (tmpla & 0xffff0000UL) + 0x10000UL;
  tmpdma = MK_FP((tmpla >> 4) & 0xffffU, tmpla & 0xfU);
  
  if (iswrite)
    for(n=0; n<bytecount; ++n)
      tmpdma[n] = prmbuf[n];
  
  r.h.ah = iswrite ? 0x05 : 0x06;
  r.x.bx = bytecount;
  if (pkt->lba.mode == DISK_PKT_MODE_LBA) {
    r.h.al = daua & 0x7f;
    r.x.cx = (pkt->lba.lba) & 0xffffU;
    r.x.dx = (pkt->lba.lba >> 16);
  } else {
    r.h.al = daua;
    r.x.cx = pkt->chs.c;
    r.h.dh = pkt->chs.h;
    r.h.dl = pkt->chs.s;
  }
  
  r.x.bp = FP_OFF(tmpdma);
  sr.es = FP_SEG(tmpdma);
  int86x2(0x1b, &r, &r, &sr);
  
  if (!r.x.cflag && !iswrite)
    for(n=0; n<bytecount; ++n)
      prmbuf[n] = tmpdma[n];
  
  if (tmpbuf) free(tmpbuf);
  
  return r.x.cflag ? r.x.ax : 0;
}

#define diskread_hd(d,b,p,bc)  diskrw_hd(0,d,b,p,bc)
#define diskwrite_hd(d,b,p,bc)  diskrw_hd(1,d,b,p,bc)


int format_hd(int daua)
{
  union REGS r;
  
  if ((daua & 0x50) != 0) return -1;  /* HD only */
  r.x.bx = 0;
  r.h.ah = 0x84;
  r.h.al = (unsigned char)daua;
  int86(0x1b, &r ,&r);
  if (r.x.cflag) return r.x.ax;
  if (r.x.bx == 0) return -1;
  
  r.h.ah = 0x0a;
  r.h.al = (unsigned char)daua;
  /* r.h.bh = r.x.bx / 256; */
  int86(0x1b, &r, &r);
  if (r.x.cflag && (daua & 0x70) == 0x20) return r.x.ax;
  
  r.h.ah = 0x8d;
  r.h.al = (unsigned char)daua;
  r.h.bh = 5; /* default interleave factor (SASI) */
  r.x.cx = r.x.dx = 0;
  int86(0x1b, &r, &r);
  
  return r.x.cflag ? r.x.ax : 0;
}



int isNEC98(void)
{
  union REGS r;
  r.x.ax = 0x0f00;
  int86(0x10, &r, &r);
  return r.h.ah == 0x0f;
}


/*
---------------------------------------------------------------
*/

typedef enum {
  NP_NOERR = 0,
  NP_READERR,
  NP_WRITEERR,
  NP_UNSUPPORTED_NECOLD,
  NP_UNSUPPORTED_FDISK,
  NP_UNFORMATTED,
  NP_BROKEN_PTABLE,
  NP_CANTBUILDFAT,
  NP_CANTBUILDPARTITION,
} NPRESULT;


enum {
  IPL_UNKNOWN = 0,
  IPL_NECOLD,      /* Old `Standard' format */
  IPL_NECNEW,      /* typical `Extended' format (DOS 3 or later) */
  IPL_FDISK,       /* IBM compatible fdisk format (not supported, for now) */
  
  IPL_BROKEN = 0
};

char *disk_buffer;
char *mbr_new;
char *mbr_org;
char *pbr;

int check_ipl(const void *diskbuf, unsigned size)
{
  const char *b = diskbuf;
  
  if (size < 256) return 0;
  
  if (memcmp(b+4, "IPL1", 4) == 0) {
    if ((b[0] == 0xeb && b[1] == 0x06) || (b[0] == 0xe9 && b[1] == 0x05 && b[2] == 0)) return IPL_NECOLD;
    if ((size >= 512 && b[0x1fe] == 0x55 && b[0x1ff] == 0xaa) ||
        (b[size-2] == 0x55 && b[size-1] == 0xaa))
      return IPL_NECNEW;
    return (b[0xfe] == 0x55 && b[0xff] == 0xaa) ? IPL_NECNEW : IPL_BROKEN;
  }
  
  /* Linux, GRUB: see GNU parted source (libparted/labels/pc98.c) */
  if (memcmp(b+4, "GRUB/98 ", 8) == 0 && memcmp(b+4, "Linux 98", 8) == 0) {
    if ((size >= 512 && b[0x1fe] == 0x55 && b[0x1ff] == 0xaa) ||
        (b[size-2] == 0x55 && b[size-1] == 0xaa))
      return IPL_NECNEW;
  }
  
  return IPL_UNKNOWN;
}

NPRESULT load_ptable(char *tmp_buffer, char *mbr_buffer, const DISK_GEOMETRY *g)
{
  DISK_PKT pkt;
  int rc;
  int i;
  
  pkt.lba.mode = DISK_PKT_MODE_LBA;
  pkt.lba.lba = 0;
  if (!mbr_buffer) mbr_buffer = tmp_buffer;
  rc = diskread_hd(g->daua, mbr_buffer, &pkt, g->bps);
  if (rc) return NP_READERR;
  switch(check_ipl(mbr_buffer, g->bps))
  {
    case IPL_NECNEW:
      break;
    case IPL_NECOLD: return NP_UNSUPPORTED_NECOLD;
    case IPL_FDISK: return NP_UNSUPPORTED_FDISK;
    default:
      return NP_UNFORMATTED;
  }
  pkt.lba.lba = 1;
  rc = diskread_hd(g->daua, tmp_buffer, &pkt, g->bps);
  if (rc) return NP_READERR;
  
  /* ptable quick check */
  for(i=0; i<g->bps; i +=32) {
    PTABLE_NEC98 *pt = (PTABLE_NEC98 *)(tmp_buffer + i);
    if (pt->type & 0x80) { /* check only active patition */
      if (pt->ipl_s > g->s || pt->ipl_h > g->h || pt->ipl_c > g->c ||
          pt->start_s > g->s || pt->start_h > g->h || pt->start_c > g->c ||
          pt->end_s > g->s || pt->end_h > g->h || pt->end_c > g->c)
      {
        return NP_BROKEN_PTABLE;
      }
    }
  }
  
  return NP_NOERR;
}


NPRESULT list_hd(const DISK_GEOMETRY *g)
{
  NPRESULT rc;
  unsigned long kb;
  unsigned char da, ua;
  char schs[32];
  char *s;
  
  s = "";
  da = g->daua & 0x70;
  ua = g->daua & 0x0f;
  printf("   %02X  ", g->daua);
  if (da == 0x00) printf("SASI%x", ua+1);
  if (da == 0x20) printf("SCSI%x", ua);
  printf(":%3u", g->bps);
  kb = calc_kb_geom(g->c, g);
  printf("%8luM", kb / 1024);
  sprintf(schs,"(C=%lu, H=%u, S=%u)", g->c, g->h, g->s);
  printf(" %-24s", schs);
  rc = load_ptable(disk_buffer, mbr_org, g);
  switch(rc) {
    case NP_READERR:
      s = "*読み込みエラー発生*";
      break;
    case NP_NOERR:
      s = "初期化済(PC-98拡張)";
      break;
    case NP_UNSUPPORTED_NECOLD:
      s = "初期化済(PC-98標準)";
      break;
    case NP_UNSUPPORTED_FDISK:
      s = "初期化済(FDISK)";
      break;
    default:
      s = "フォーマット不明";
  }
  printf("  %s", s);
  printf("\n");
  
  return rc;
}

int list_hds(int numofhds)
{
  NPRESULT rc;
  int i;

  printf(" DA/UA  HD type    Size     Geometry                  Status\n");
  for(i=0; i<numofhds; ++i) {
    rc = list_hd(&diskinfo[i]);
  }
  
  return 0;
}

enum {
  BLDFAT_NOERR = 0,
  BLDFAT_TOOSMALL,
  BLDFAT_TOOBIG,
};

int build_bpb_for_necfat16(void *boot_sector_buffer, unsigned long physical_sectors_in_partition, unsigned size_of_physical_sector)
{
  /*
    todo:
      - NEC DOS 3.x partition
      - FAT12
    
    note:
      IBM PC 系の DOS 標準 format だとルートディレクトリエントリは 
      最大 512 のようだ 
      NEC だと 3072 のようだ（サイズが小さい場合は 1024 のようだ）
      予約セクタは最小 1024 バイト分
      （論理セクタサイズが 256なら 4、512なら 2、1024以上なら 1 でいいのかな）
      ※ちなみに Win9x の fdisk + format では 1 セクタ固定となるため、
        256バイトセクタの場合ブート用コードが途中で途切れる（当然起動不可）
      DOS 3.x 領域は物理セクタサイズと DOS 内部用の論理セクタ（レコード）サイズが
      異なる場合がある、というか 3.3 までの DOS 領域では必ず異なる
      （領域が 65M 以上なら 2048、それ未満は 1024）
  */
  BPB_BOOTSECTOR *bpb = boot_sector_buffer;
  unsigned bps;
  unsigned rootdirs;
  unsigned long scts_total;
  unsigned scts_reserved;
  unsigned scts_rootdir;
  unsigned scts_onefat;
  int i;
  unsigned long scts;
  unsigned long clsts;
  unsigned clst_sct;
  
  /* todo: DOS3.x の論理セクタサイズ対応 */
  bps = size_of_physical_sector;
  scts_total = physical_sectors_in_partition;
  scts_reserved = bps > 1024 ? 1 : 1024 / bps;
  /* todo: rootdirs は領域サイズ次第で加減 */
  rootdirs = 3072;
  scts_rootdir = ((unsigned long)rootdirs * 32UL + (bps - 1)) / bps;
  rootdirs = (bps/32) * scts_rootdir;
  
  if (scts_total <= (scts_reserved + scts_rootdir))
    return BLDFAT_TOOSMALL;
  scts = scts_total - scts_reserved - scts_rootdir; /* todo: re-calc with adding FAT size... */
  for(i = 0, clst_sct = 0; i < 8; ++i) {
    if ((unsigned short)(bps << i) < bps)
      break;
    clsts = scts >> i;
    if (clsts <= 0xfff6UL) {
      clst_sct = 1 << i;
      break;
    }
  }
  
  scts_onefat = (clsts*2 + bps-1) / bps;
  
  if (scts_total<= scts_reserved + scts_rootdir + scts_onefat * 2)
    return BLDFAT_TOOSMALL;
  
  bpb->bytes_per_sector = bps;
  bpb->sectors_per_cluster = clst_sct;
  bpb->reserved_sectors = scts_reserved;
  bpb->fats = 2;
  bpb->rootdirs = rootdirs;
  bpb->sectors16 = 0;
  bpb->sectors32 = 0;
  if (scts_total > 0xffffUL)
    bpb->sectors32 = scts_total;
  else
    bpb->sectors16 = (unsigned short)scts_total;
  bpb->media_descriptor = 0xf8;
  bpb->sectors_per_fat = scts_onefat;
  
  /* 
  bpb->sectors_per_track = ;
  bpb->heads = ;
  bpb->hidden_sectors = ;
  */
  
  bpb->boot_drive_number = 0x80;
  bpb->boot_drive_head = 0;
  bpb->boot_signature = 0x29; /* extended BPB */
  
  if (disp_debug) {
    printf("total sectors (physical) = %lu (%luMB bps=%u)\n", physical_sectors_in_partition, sectors_to_kilobytes(physical_sectors_in_partition, size_of_physical_sector)/1024UL, size_of_physical_sector);
    printf("total sectors (logical)  = %lu (%luMB bps=%u)\n", scts_total, sectors_to_kilobytes(scts_total, bps)/1024UL, bps);
    printf("cluster = %u sectors (%lu bytes)\n", clst_sct, (unsigned long)clst_sct * bps);
    printf("total clusters = %u (0x%04x)\n", (unsigned)clsts, (unsigned)clsts);
    printf("reserved sectors = %u\n", scts_reserved);
    printf("number of rootdirs = %u (%u sectors)\n", rootdirs, scts_rootdir);
    printf("sectors per a FAT = %u (x%u : total %u)\n", scts_onefat, bpb->fats, scts_onefat * bpb->fats);
  }
  
  return BLDFAT_NOERR;
}


unsigned long
tm_to_volume_serial(const struct tm *tm)
{
    /*
      reference: disktut.txt 1993-03-26 (line 1919-)
      serial: xxyy-zzzz
      xx: month + second
      yy: day
      zzzz: year + hour * 0x100 + minute
      
    */
    unsigned xx, yy, zzzz;
    xx = (tm->tm_mon + 1) + tm->tm_sec;
    yy = tm->tm_mday;
    zzzz = tm->tm_year >= 1900 ? tm->tm_year : tm->tm_year + 1900;
    zzzz = zzzz + (tm->tm_hour * 256) + tm->tm_min;
    
    return ((unsigned long)zzzz << 16) | (xx << 8) | yy;
}

unsigned long
current_time_to_volume_serial(void)
{
    time_t t;
    struct tm *tm;
    t = time(NULL);
    tm = localtime(&t);
    return tm_to_volume_serial(tm);
}


int build_partition(const DISK_GEOMETRY *g, const char *partname, unsigned long cylinders)
{
  PTABLE_NEC98 *pt;
  BPB_BOOTSECTOR *bpb;
  DISK_PKT pkt;
  unsigned long lba;
  size_t n;
  int i, j;
  int rc;
  
  if (cylinders == 0) return NP_CANTBUILDPARTITION;
  
  memset(disk_buffer, 0, g->bps);
  /* setup (only) 1st partition */
  pt = (PTABLE_NEC98 *)disk_buffer;
  pt->bootable = 0x80 | (0x20 + 1); /* bootable | DOS 5 1st */
  pt->type = 0x80 | 0x21; /* active & DOS 5 */
  pt->ipl_s = pt->start_s = 0;
  pt->ipl_h = pt->start_h = 0;
  pt->ipl_c = pt->start_c = 1;
  pt->end_s = g->s - 1;
  pt->end_h = g->h - 1;
  pt->end_c = pt->start_c + cylinders - 1;
  if (pt->end_c >= g->c) pt->end_c = g->c - 1;
  n = partname ? strlen(partname) : 0;
  if (n > sizeof(pt->name)) n = sizeof(pt->name);
  memset(pt->name, ' ', sizeof(pt->name));
  if (n > 0) memcpy(pt->name, partname, n);

  bpb = (BPB_BOOTSECTOR *)pbr;
  /* memset(pbr, 0, IPL_SIZE_MAX); */
  rc = build_bpb_for_necfat16(pbr, (unsigned long)(g->s * g->h) * (pt->end_c - pt->start_c + 1), g->bps);
  if (rc != BLDFAT_NOERR)
    return NP_CANTBUILDFAT;

  lba = (unsigned long)(pt->start_c) * (g->s * g->h) +
        (unsigned long)(pt->start_h) * g->s +
        pt->start_s;

  pkt.chs.mode = DISK_PKT_MODE_CHS;
  pkt.chs.c = 0;
  pkt.chs.h = 0;
  pkt.chs.s = 1;
  rc = diskwrite_hd(g->daua, disk_buffer, &pkt, g->bps);
  if (rc != 0)
    return NP_WRITEERR;

  
  /*
    build fs
  */
  
  /* build and write boot record (PBR) */
  bpb->hidden_sectors = lba;
  bpb->sectors_per_track = g->s;
  bpb->heads = g->h;
  if (bpb->volume_label[0] == '\0')
    memcpy(bpb->volume_label, "NO NAME    ", 11);
  if (bpb->file_system[0] == '\0')
    memcpy(bpb->file_system, "FAT16   ", 8);
  if (bpb->volume_serial == 0)
    *(unsigned long *)&(bpb->volume_serial) = current_time_to_volume_serial();

  pkt.chs.s = pt->ipl_s;
  pkt.chs.h = pt->ipl_h;
  pkt.chs.c = pt->ipl_c;
  rc = diskwrite_hd(g->daua, pbr, &pkt, bpb->bytes_per_sector * bpb->reserved_sectors);
  if (rc != 0)
    return NP_WRITEERR;
  
  /* initialize fat */
  memset(disk_buffer, 0, bpb->bytes_per_sector);
  n = bpb->bytes_per_sector / g->bps;
  pkt.lba.mode = DISK_PKT_MODE_LBA;
  pkt.lba.lba = lba + (bpb->reserved_sectors * n);
  for(j=0; j<bpb->fats; ++j) {
    for(i=0; i<bpb->sectors_per_fat; ++i) {
      if (i==0) {
        disk_buffer[0] = 0xfe;
        disk_buffer[1] = 0xff;
        disk_buffer[2] = 0xff;
        disk_buffer[3] = 0xff;
      } else {
        disk_buffer[0] = 0;
        disk_buffer[1] = 0;
        disk_buffer[2] = 0;
        disk_buffer[3] = 0;
      }
      rc = diskwrite_hd(g->daua, disk_buffer, &pkt, bpb->bytes_per_sector);
      if (rc != 0)
        return NP_WRITEERR;
      pkt.lba.lba += n;
    }
  }
  /* initialize rootdirs */
  memset(disk_buffer, 0, bpb->bytes_per_sector);
  for(i=0; i<(32UL * bpb->rootdirs + bpb->bytes_per_sector-1)/bpb->bytes_per_sector; ++i) {
    rc = diskwrite_hd(g->daua, disk_buffer, &pkt, bpb->bytes_per_sector);
    if (rc != 0)
      return NP_WRITEERR;
    pkt.lba.lba += n;
  }
  
  return NP_NOERR;
}

int write_mbr(const DISK_GEOMETRY *g, const void *buffer)
{
  int rc;
  DISK_PKT pkt;
  pkt.lba.mode = DISK_PKT_MODE_LBA;
  pkt.lba.lba = 0;
  rc = diskwrite_hd(g->daua, (void *)buffer, &pkt, g->bps);
  
  return rc ? NP_WRITEERR : NP_NOERR;
}

int fill_track(const DISK_GEOMETRY *g, char *tmp_buffer, unsigned long track, unsigned char fillvalue)
{
  DISK_PKT pkt;
  unsigned sct, hds;
  
  memset(tmp_buffer, fillvalue, g->bps);
  pkt.chs.mode = DISK_PKT_MODE_CHS;
  pkt.chs.c = track;
  for(hds = 0; hds < g->h; ++hds) {
    pkt.chs.h = hds;
    for(sct = 0; sct < g->s; ++sct) {
      pkt.chs.s =sct;
      if (diskwrite_hd(g->daua, tmp_buffer, &pkt, g->bps))
        return NP_WRITEERR;
    }
  }
  return NP_NOERR;
}


enum {
  YN_CANCEL = 0,
  YN_NO,
  YN_YES
};
int getync(int do_echo)
{
  union REGS r;
  
  r.x.ax = do_echo ? 0x0c01 : 0x0c08;
  intdos(&r, &r);
  
  if (r.h.al == 'y' || r.h.al == 'Y') return YN_YES;
  if (r.h.al == 'n' || r.h.al == 'N') return YN_NO;
  return YN_CANCEL;
}

char *getline(char *buf, int maxcount)
{
#define DEFBUFLEN 128
  static char defbuffer[2 + DEFBUFLEN + 1];
  union REGS r;
  struct SREGS sr;
  
  if (!buf) {
    if (maxcount <= 0 || maxcount >= DEFBUFLEN) {
      fprintf(stderr, "getline: buffer not enough\n");
      return NULL;
    }
    buf = defbuffer;
  }
  buf[0] = (unsigned char)maxcount+1;
  buf[1] = 0;
  buf[2] = 0x0d;
  r.x.ax = 0x0c0a;
  r.x.dx = FP_OFF(buf);
  sr.ds = FP_SEG(buf);
  intdosx(&r, &r, &sr);
  buf[(unsigned char)(buf[1]) + 2] = '\0';
  return buf + 2;
}

int load_file(void *buf, const char *fname, unsigned maxlen)
{
  FILE *fi;
  int cnt;
  
  fi = fopen(fname, "rb");
  if (!fi) {
    fprintf(stderr, "ファイル %s が存在しません（もしくは読み込みエラー）\n", fname);
    exit(1);
  }
  cnt = fread(buf, 1, maxlen, fi);
  fclose(fi);
  return cnt;
}

unsigned trim_filename(char *s)
{
  char *sl, *bsl;
  
  sl = strrchr(s, '/');
  sl = sl ? sl + 1 : s;
  bsl = strrchr(s, '\\');
  bsl = bsl ? bsl + 1 : s;
  if (sl < bsl) sl = bsl;
  
  *sl = '\0';
  return sl - s;
}



int optD;
int optF;
int optHelp;
int optV;
int optVersion;
int optTrack0;

int getopt(int argc, char *argv[])
{
  int n = 0;
  char *s, c;
  
  while(--argc) {
    s = *++argv;
    c = *s;
    if ((c == '-' && s[1] != '-') || c == '/') {
      switch(*++s) {
        case 'H': case 'h': case '?':
          optHelp = 1;
          break;
        case 'V': case 'v':
          optV = 1;
          break;
        case 'F': case 'f':
          optF = 1;
          break;
        case 'd':
          optD = 1;
          break;
      }
    }
    else if (c == '-' && s[1] == '-') {
      s += 2;
      if (strcmp(s, "help")==0) optHelp = 1;
      if (strcmp(s, "debug")==0) optD = 1;
      if (strcmp(s, "verbose")==0) optV = 1;
      if (strcmp(s, "version")==0) optVersion = 1;
      if (strcmp(s, "format")==0) { optF = 1; optTrack0 = 1; }
      if (strcmp(s, "erase-track0")==0) optTrack0 = 1;
      if (strcmp(s, "format-track0")==0) optTrack0 = 1; /* obsolete */
    }
    else {
      /* not option */
      ++n;
    }
  }
  return n;
}


#define def_to_str(str) #str
#if defined(GITHASH)
# define REVSTR \
  "git: " \
  def_to_str(GITHASH)
#else
# define REVSTR \
  "built at " \
  __DATE__ 
#endif

char *buildrev(void)
{
  return REVSTR;
}

void banner(void)
{
  printf("Better-Than-Nothing partitioner for NEC PC-9801/9821\n");
  printf("version %s (%s)\n", VERSIONSTRING, buildrev());
  
  printf("\n");
}

void *myalloc(size_t n)
{
  void *p;
  if (n == 0) n = 1;
  
  p = malloc(n);
  if (!p) {
    fprintf(stderr, "\nNot enough memory.\n");
    exit(-1);
  }
  memset(p, 0, n);
  
  return p;
}


int chk_err(int rc)
{
  if (rc != NP_NOERR) {
    printf("エラー発生\n");
    exit(1);
  }
  printf("OK\n");
  return rc;
}


int main(int argc, char *argv[])
{
  DISK_GEOMETRY *geo;
  int rc;
  int hds;
  int daua;
  char partname[17];
  unsigned char *progpath;
  unsigned ppathlen;
  int rewrite_mbr = 1;
  unsigned long max_fat16part_sectors;
  unsigned long max_fat16part_cylinders;
  
  setvbuf(stdout, NULL, _IONBF, 1);
  getopt(argc, argv);
  if (optVersion) {
    printf("btnpart %s (%s)\n", VERSIONSTRING, buildrev());
    return 0;
  }
  
  disp_debug = optD;
  
  banner();
  
  
  if (!isNEC98()) {
    printf("This program only works on NEC PC-9801/9821 series.\n");
    return 1;
  }
  
  hds = get_all_geometries();
  if (hds <= 0) {
    printf("（利用可能な）ハードディスクがありません…");
    return 0;
  }
  
  disk_buffer = myalloc(4096);
  mbr_new = myalloc(IPL_SIZE_MAX);
  mbr_org = myalloc(IPL_SIZE_MAX);
  pbr = myalloc(IPL_SIZE_MAX);
  ppathlen = 66;
  if (argv[0] && argv[0][0]) {
    ppathlen = strlen(argv[0]) + 12;
    if (ppathlen < 66) ppathlen = 66;
  }
  progpath = myalloc(ppathlen + 1);
  strcpy(progpath, argv[0] ? argv[0] : "");
  ppathlen = trim_filename(progpath);
  
  list_hds(hds);
  
  progpath[ppathlen] = '\0';
  strcat(progpath, MBR_NAME);
  load_file(mbr_new, progpath, IPL_SIZE_MAX);
  progpath[ppathlen] = '\0';
  strcat(progpath, PBR_NAME);
  load_file(pbr, progpath, IPL_SIZE_MAX);
  
  printf("\n初期化するディスクの DA/UA を入力してください: ");
  daua = (int)strtol(getline(NULL, 2), NULL, 16);
  if (daua <= 0) {
    printf("\nDA/UA が正しくありません");
    return 1;
  }
  geo = daua_to_diskinfo(daua);
  if (!geo) {
    printf("\nそのハードディスクは存在しません");
    exit(1);
  }
  
  printf("\n\n");
  
  rc = list_hd(geo);
  
  /* fat16 (with 32bit sector count) limitation... */
  max_fat16part_sectors = (32768UL / geo->bps) * 0xffffUL;
  max_fat16part_cylinders = max_fat16part_sectors / ((unsigned long)(geo->s) * geo->h);
  if (disp_debug) {
    printf("max_fat16part_sectors = %lu\n", max_fat16part_sectors);
    printf("max_fat16part_cylinders = %lu\n", max_fat16part_cylinders);
  }
  printf("\nこのハードディスク内に FreeDOS(98) 用の DOS 領域をひとつ作成します\n");
  printf("（ディスク内にいま存在する領域はすべて消去されます）\n");
  printf("よろしいですか？(y/N) ");
  if (getync(1) != YN_YES) {
    printf("\n");
    return 0;
  }
  printf("\nディスクの領域名を設定してください（最大16文字）");
  while(1) {
    int need_retry;
    int i;
    
    need_retry = 0;
    printf("\n領域名: ");
    strcpy(partname, getline(NULL, 16));
    for(i=0; i<16; ++i) {
      if (partname[i] == '\0') break;
      if ((unsigned char)(partname[i]) < 0x20) {
        need_retry = 1;
        break;
      }
    }
    if (need_retry) {
      printf("\n不正な文字が使われています");
      continue;
    }
    for(; i<16; ++i) partname[i] = ' ';
    partname[16] = '\0';
    if (!need_retry)
      break;
  }
  if (strcmp(partname, "                ") == 0) {
    strcpy(partname, "FreeDOS(98) boot");
    printf("\r\x1b[8C%s", partname);
  }
  
  printf("\n\n");
  
  if (optF) {
    printf("ハードディスク全体のフォーマットを行いますか？(y/N) ");
    optF = getync(1) == YN_YES;
    printf("\n");
  }
  
  if (rc == NP_NOERR && mbr_org[0xfc] != 0 && !optF) {
    printf("おそらく、拡張起動メニューがインストールされています\n");
    printf("IPL を書き換えると起動メニューが使えなくなります\n");
    printf("IPL を書き換えますか（非推奨）？(y/N) ");
    rewrite_mbr = getync(1) == YN_YES;
    printf("\n");
  }
  
  /* write into the disk... */
  
  if (optF) {
    rewrite_mbr = 1;
    optTrack0 = 1;
    printf("ハードディスクのフォーマット中（時間がかかるかもしれません）…");
    fflush(stdout);
    chk_err(format_hd(geo->daua));
  }
  
  if (rewrite_mbr && optTrack0) {
    printf("トラック 0 初期化中…");
    chk_err(fill_track(geo, disk_buffer, 0, 0xe5));
  }
  
  printf("DOS 領域作成中…");
  chk_err(build_partition(geo, partname, max_fat16part_cylinders));
  
  if (rewrite_mbr) {
    printf("IPL(MBR) 書き換え…");
    chk_err(write_mbr(geo, mbr_new));
  }
  
  printf("\n");
  printf("DOS 領域を作成しました\n");
  printf("システムを再起動した後、FreeDOS の SYS コマンドでシステムを転送してください\n");
  
  return 0;
}


