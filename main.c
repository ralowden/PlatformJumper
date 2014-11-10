//main.c for finalproject

//*************************Macros************************
#define WALKSPEED 8
#define JUMPSPEED 8
#define PLATTHICKNESS 1
#define LEFTBOUND -8
#define RIGHTBOUND 168
#define TOPBOUND 128
#define BOTTOMBOUND 0
#define DIFFICULTYMARGIN 6
#define BACKGROUNDCOLOR YELLOW
#define PLATCOLOR RED
#define AUDIOBUFSIZE 128
#define TIMER 20000
#define WORLDWIDTH 5
#define WORLDHEIGHT 5
//***********************End Macros**********************

#include <stm32f30x.h>  // Pull in include files for F30x standard drivers 
#include <f3d_led.h>     // Pull in include file for the local drivers
#include <f3d_lcd_sd.h>
#include <f3d_i2c.h>
#include <f3d_i2c2.h>
#include <f3d_accel.h>
#include <f3d_mag.h>
#include <f3d_nunchuk.h>
#include <f3d_rtc.h>
#include <f3d_systick.h>
#include <ff.h>
#include <diskio.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <loadimg.h>

//*********************Structures***************************
typedef struct rect_data{
  int l,pl,r,pr,t,pt,b,pb;
} rect_t;

typedef struct plat_data{
  rect_t crect;
} plat_t;

typedef struct pickup_data{
  rect_t crect;
  int type;
  int vis_flag;
} pickup_t;

typedef struct sspace_data{
  plat_t plats[4];
  pickup_t pickup;
  int platnum;
} sspace_t;

typedef struct player_data{
  int wx,wy;
  rect_t crect;
  int state;
  int drawFlag;
  int jump_flag;
  int jump_tick;
  uint16_t c;
  int anim_tick;
  int dir;
  int deltaW;
  int maxJumps;
  int jumps;
  int jet;
  int key;
  int fuel;
} player_t;

struct bmpfile_magic {
  unsigned char magic [2];
};
struct bmpfile_header {
  uint32_t filesz ;
  uint16_t creator1 ;
  uint16_t creator2 ;
  uint32_t bmp_offset ;
};
typedef struct {
  uint32_t header_sz ;
  int32_t width ;
  int32_t height ;
  uint16_t nplanes ;
  uint16_t bitspp ;
  uint32_t compress_type ;
  uint32_t bmp_bytesz ;
  int32_t hres;
  int32_t vres;
  uint32_t ncolors ;
  uint32_t nimpcolors ;
} BITMAPINFOHEADER ;
typedef struct { // little endian byte order
  uint8_t b;
  uint8_t g;
  uint8_t r;
}bmppixel;

struct ckhd {
  uint32_t ckID;
  uint32_t cksize;
};

struct fmtck {
  uint16_t wFormatTag;      
  uint16_t nChannels;
  uint32_t nSamplesPerSec;
  uint32_t nAvgBytesPerSec;
  uint16_t nBlockAlign;
  uint16_t wBitsPerSample;
};



//********************End Structures************************

//******************Function Prototypes*********************
void init();
int ctoggle(nunchuk_t * chuk);
int ztoggle(nunchuk_t * chuk);
void binit_crect(int l, int r, int t, int b, rect_t * rect);
int rect_collide(rect_t *one, rect_t *two);
void printRect(rect_t * r);
void player1_init(player_t * play);
void update_player(player_t * play, nunchuk_t * chuk);
void draw_player(player_t * play, uint16_t * buffer);
void die (FRESULT rc);
void pickupInit(pickup_t * one, int ty, int l, int r, int t, int b);
void LOADWORLDONE(void);
int playsound(char * name);
void readckhd(FIL *fid, struct ckhd *hd, uint32_t ckID);
int gameover(int state);
int maingameloop(nunchuk_t * chuk1, player_t * pone, uint16_t * playerbuffer);
int restartloop(int state);
void pickupinit(void);
//****************End Function Prototypes*******************

//***********************Globals****************************
sspace_t world[5][5];      //5x5 world 
pickup_t inventory[4];     //Max of 4 items
FATFS Fatfs;
extern uint8_t Audiobuf[AUDIOBUFSIZE];
extern int audioplayerHalf;
extern int audioplayerWhole;
FIL fid;		/* File object */
BYTE Buff[512];		/* File read buffer */
int ret;
//*********************End Globals**************************


void loadPlayer(char * FILENAME, uint16_t * buffer){
  FIL fimg;
  FRESULT rc;
  UINT r;
  struct bmpfile_magic magic;
  struct bmpfile_header header;
  BITMAPINFOHEADER info;
  bmppixel pixelbuffer;
  
  rc = f_open(&fimg, FILENAME, FA_READ);
  if (rc) die(rc);
  f_lseek(&fimg,0x36);
  int count = 0;
  while (count++ <= 384){//384
    f_read(&fimg, &pixelbuffer, sizeof pixelbuffer,&r);
    if ((pixelbuffer.r == 0xFF) && (pixelbuffer.g == 0xFF) && (pixelbuffer.b == 0xFF)){ //WHITE
      buffer[count] = BACKGROUNDCOLOR;
    }else{
      buffer[count] = (((pixelbuffer.r >> 3) << 11) + ((pixelbuffer.g >> 2) << 5) + (pixelbuffer.b >> 3));
    }
  }

  f_close(&fimg);
  
}
void startscreen(char * FILENAME){
  FIL fimg;
  FRESULT rc;
  UINT r;
  struct bmpfile_magic magic;
  struct bmpfile_header header;
  BITMAPINFOHEADER info;
  bmppixel pixelbuffer;
  uint16_t buffer[128];
  
  rc = f_open(&fimg, FILENAME, FA_READ);
  if (rc) die(rc);
  f_lseek(&fimg,0x36);
  int count;
  int rowcount = 0;
  while (rowcount < 160){
    count = 0;
    while (count++ < 128){//384
      f_read(&fimg, &pixelbuffer, sizeof pixelbuffer,&r);
      buffer[count] = (((pixelbuffer.r >> 3) << 11) + ((pixelbuffer.g >> 2) << 5) + (pixelbuffer.b >> 3));
    }
    f3d_lcd_rowBuffer(rowcount,&buffer[0]);
    rowcount++;
  }
  f_close(&fimg);
}

void loadSpace(char * FILENAME,int x, int y){
  FIL fimg;
  FRESULT rc;
  UINT r;
  rc = f_open(&fimg, FILENAME, FA_READ);
  if (rc) die(rc);
  // Individual ScreenSpace
  // 16 - platnum
  // 16 - plat left
  // 16 - plat right
  // 16 - plat height
  int hc=0,wc=0,platnum=0,pc;
  int left = 0, right = 0, height = 0;
  f_read(&fimg,&platnum,4,&r);
  pc = 0;
  world[x][y].platnum = platnum;
  printf("number of plats: %d",platnum);
  while (pc < platnum){
    f_read(&fimg,&left,4,&r);
    f_read(&fimg,&right,4,&r);
    f_read(&fimg,&height,4,&r);
    printf("Left: %d Right %d B: %d",left,right,height);
    world[x][y].plats[pc].crect.l = left;
    world[x][y].plats[pc].crect.r = right;
    world[x][y].plats[pc].crect.b = height;
    world[x][y].plats[pc].crect.t = height+PLATTHICKNESS;
    pc++;
  }
  //REMOVE LATER; JUST INCORRECT CODING
  world[0][0].plats[0].crect.r = 160;
  f_close(&fimg);
}


void mask(player_t * play){
  int dx,dy,ml,mr,mt,mb,updownflag;
  updownflag = 0;
  dy = play->crect.l - play->crect.pl;
  dx = play->crect.t - play->crect.pt;
  if (dy > 0){
    //move right                                                                
    ml = play->crect.pl;
    mr = play->crect.l-1;
    mt = play->crect.pt;
    mb = play->crect.pb;
    f3d_lcd_rect(mb,ml,mt,mr,BACKGROUNDCOLOR);
  }else if (dy < 0){
    //move left                                                                 
    ml = play->crect.r-1;
    mr = play->crect.pr;
    mt = play->crect.pt;
    mb = play->crect.pb;
    f3d_lcd_rect(mb,ml,mt,mr,BACKGROUNDCOLOR);
  }else{    
    updownflag = 1;
    //this is a passing thought to fix the still                                
    //small overlap the mask uses.                                              
  }
  if (dx > 0){
    //move up                                                                   
    mt = play->crect.b;
    mb = play->crect.pb;
    mr = play->crect.r;
    ml = play->crect.l;
    f3d_lcd_rect(mb,ml,mt,mr,BACKGROUNDCOLOR);
  }else if (dx < 0){
    //move down          
    mt = play->crect.pt;
    mb = play->crect.t;
    mr = play->crect.r;
    ml = play->crect.l;
    f3d_lcd_rect(mb,ml,mt,mr,BACKGROUNDCOLOR);
  }
}

int floorcollide(player_t * play) {
  //What the player's next coords are while falling
  rect_t playrect; 
  playrect.l = play->crect.l;
  playrect.r = play->crect.r;
  playrect.t = play->crect.t-JUMPSPEED;
  playrect.b = play->crect.b-JUMPSPEED;
  sspace_t * cspace = &world[play->wx][play->wy];

  int platcount = cspace->platnum;
  int platcounter = 0;
  while (platcounter < platcount){
    /*if(play->jump_flag == 2) {
      printf("Rect %d:", platcounter);
      printRect(&(cspace->plats[platcounter].crect));
      }*/
    if (rect_collide(&playrect,&cspace->plats[platcounter].crect)){
      //Keeps from jumping to new platform
      if(play->crect.b > cspace->plats[platcounter].crect.b - DIFFICULTYMARGIN) { 
	return cspace->plats[platcounter].crect.t+1;
      }
    }
    platcounter++;
  }
  return 0;
}

int boundcollide(player_t * play) {
  if (play->crect.b-JUMPSPEED < 0)   return 3;
  if (play->crect.t+JUMPSPEED  > 128) return 4;
  if (play->crect.l-WALKSPEED < 0)   return 1;
  if (play->crect.r+WALKSPEED > 160) return 2;
  return 0;
}
                                                                                
int rect_collide(rect_t *one, rect_t *two){
  return !(one->l > two->r || one->r < two->l || one->t < two->b || one->b > two->t);  
}

void drawRect(rect_t * rect, uint16_t c) {
  f3d_lcd_rect(rect->b,rect->l,rect->t,rect->r,c);
}                                                                                
void drawPlatforms(player_t * play){
  sspace_t * cspace = &world[play->wx][play->wy];
  int platcount = cspace->platnum;
  int platcounter = 0;
  plat_t * current;
  while (platcounter < platcount){
    current = &cspace->plats[platcounter++];
    drawRect(&current->crect,PLATCOLOR);
  }
  //printf("PLATS\n");
}

void drawPickups(player_t * play) {
  sspace_t * cspace = &world[play->wx][play->wy];
  
  if(cspace->pickup.vis_flag == 1) {
    printRect(&cspace->pickup.crect);
    drawRect(&cspace->pickup.crect,RED);
  }
}

//int gameover(int state){
//  Game = state;
//  return state;
//}

int Game=1;
FATFS Fatfs;

void main() {
  init();
  printf("Reset\n");
  
  FRESULT rc;
  nunchuk_t chuk1;
  player_t pone;
  int startpause = 1;
  rc = f_mount(0, &Fatfs);
  if (rc){
    die(rc);
  } 
  //startscreen();
  startscreen("start.bmp");
  uint16_t playerbuffer[400] = {0};
  loadPlayer("BB.BMP", &playerbuffer[0]); 
  LOADWORLDONE(); 
  playsound("PICKUP.wav");
  //inform the game is ready
  //  while (startpause){
  //  printf("j");
  //  f3d_nunchuk_read(&chuk1);
  //  if (chuk1.c){
  //    printf("here");
  //    startpause = 0;
  //  }
  //}
  //Data
  while (1){
    f3d_lcd_fillScreen(BACKGROUNDCOLOR);
    int endstate;
    Game = 1;
    endstate = maingameloop(&chuk1, &pone, &playerbuffer[0]);
    if(endstate == 0) {
      playsound("WILHELM.wav");
      startscreen("lose.bmp");
    } else if(endstate == 2) {
      playsound("WIN.wav");
      startscreen("victory.bmp");
    }
    startpause = 1;
    pickupinit(); //reset pickups
    //while (startpause){
    //  f3d_nunchuk_read(&chuk1);
    //  if (chuk1.c){
    //	printf("HERE\n");
    //	startpause = 0;
    // }
    //}
  }
}

int restartloop(int state) {
  
}

int maingameloop(nunchuk_t * chuk1, player_t * pone, uint16_t * playerbuffer){ 
  player1_init(pone);
  while (Game == 1){    
    if (input_flag){
      f3d_nunchuk_read(chuk1);
      update_player(pone,chuk1);
      input_flag = 0;
    }
    drawPlatforms(pone);
    drawPickups(pone);
    draw_player(pone,playerbuffer);
  }
  return Game;
}

//**********************Function definitions*******************

//************************Player Functions********************* 
void player1_init(player_t * play){                                                       play->crect.l = 0;
  play->crect.l = 0+24*3;
  play->crect.r = 16+24*3;
  play->crect.t = 24+16*5;
  play->crect.b = 0+16*5;
  play->crect.pl = 0;
  play->crect.pr = 16;
  play->crect.pt = 24;
  play->crect.pb = 0;
  play->drawFlag = 1;
  play->c = RED;
  play->jump_flag = 0;
  play->jump_tick = 0;
  play->wx = 0;
  play->wy = 0;
  play->deltaW = 0;
  play->anim_tick = 0;
  play->dir = 0;
  play->maxJumps = 1;
  play->jumps = 1;
  play->jet = 0;
  play->key = 0;
  play->fuel = 100;
}

void update_player(player_t * play, nunchuk_t * chuk){
  
  //PICKUP COLLISION - AND END GAME
  if (world[play->wx][play->wy].pickup.vis_flag){
    if (rect_collide(&play->crect,&world[play->wx][play->wy].pickup.crect)){
      switch (world[play->wx][play->wy].pickup.type){
      case 1:
	//HOME
	if (play->key){
	  Game = 2;
	}
	break;
      case 2:
	play->maxJumps = 2;
	world[play->wx][play->wy].pickup.vis_flag = 0;
	playsound("DBLJUMP.wav");
	//Double Jump
	break;
      case 3:
	play->jet = 1;
	world[play->wx][play->wy].pickup.vis_flag = 0;	
	playsound("GETJP.wav");
	//JetPack
	break;
      case 4:
	//gas station
	break;
      case 5:
	play->key = 1;
	world[play->wx][play->wy].pickup.vis_flag = 0;
	playsound("KEY.wav");
	break;
      }
    }
  }
  //**********************************
  //**********LEFT AND RIGHT**********
  //**********************************
  if (chuk->jx > 150){
    if (play->crect.r + WALKSPEED > RIGHTBOUND){
      if (play->wy+1 > WORLDWIDTH-1){
	printf("BOUND");
      }else{
	f3d_lcd_fillScreen(BACKGROUNDCOLOR);
	play->wy++;
	play->crect.l = 0;
	play->crect.r = 16;
	//pageright
      }
    }else{
      play->crect.pl = play->crect.l;
      play->crect.pr = play->crect.r;
      play->crect.l = play->crect.l + WALKSPEED;
      play->crect.r = play->crect.r + WALKSPEED;
      play->drawFlag = 1;
      play->dir = 1;
    }
  }else if (chuk->jx < 120){  //if walking left                                 
    if (play->crect.l - WALKSPEED < LEFTBOUND){
      if (play->wy -1 < 0){
	printf("bounds");
      }else{
	f3d_lcd_fillScreen(BACKGROUNDCOLOR);
	play->wy--;
	play->crect.l = 159-16;
	play->crect.r = 159;
	//pageleft
      }
    }else{
      play->crect.pl = play->crect.l;
      play->crect.pr = play->crect.r;
      play->crect.l = play->crect.l - WALKSPEED;
      play->crect.r = play->crect.r - WALKSPEED; 
      play->drawFlag = 1;
      play->dir = 2;
    }
  }else{
    play->dir = 0;
  }
  // *******************************
  // **********UP AND DONW**********
  // *******************************
  
  if (chuk->c && play->jet && play->fuel){
    //JEEEET
    play->fuel--;
    printf("Fuelamt: %d",play->fuel);
  }else{
    if (!(play->jump_flag) || (play->jumps)){  //no jumping
      if (chuk->jy > 150){//&& player has jumps left
	play->jump_flag = 1;
	if (play->jumps){
	  play->jumps--;
	  //printf("Jumpos: %d",play->jumps);
	}
      }
    }
    
    if (play->jump_flag == 1){ //jumping going up
      play->drawFlag = 1;
      play->jump_tick++;
      if (play->jump_tick < 5){
	if (play->crect.t + JUMPSPEED > TOPBOUND){
	  if (play->wx-1 < 0){
	    printf("BOUND");
	  }else{
	    //page up
	    f3d_lcd_fillScreen(BACKGROUNDCOLOR);
	    play->wx--;
	    play->crect.t = 24;
	    play->crect.b = 0;
	  }
	}else{
	  play->crect.pt = play->crect.t;
	  play->crect.pb = play->crect.b;
	  play->crect.t = play->crect.t + JUMPSPEED;
	  play->crect.b = play->crect.b + JUMPSPEED;
	}
      }else{ //falling... just sets fall flag. 
	play->crect.pt = play->crect.t;
	play->crect.pb = play->crect.b;
	play->jump_flag = 2;
	play->jump_tick = 0;
      }
    }
    //printf("floor %d",floor);
    if(play->jump_flag != 1){ //if not jumping
      int floor = floorcollide(play);
      if(floor) { //Ran into floor; no longer falling
	if(play->jump_flag){
	  playsound("JUMP.wav");
	}
	play->jumps = play->maxJumps;
	play->drawFlag = 1;
	play->jump_flag = 0;
	play->crect.pt = play->crect.t;
	play->crect.pb = play->crect.b;
	play->crect.t = floor+24;
	play->crect.b = floor;
	//playsound("gameWAVs/MarioLand.wav");
      } else { //Falling!
	
	if (play->crect.b - JUMPSPEED < BOTTOMBOUND){
	  if (play->wx+1 > WORLDHEIGHT-1){
	    Game = 0;
	    printf("DEATH!\n");
	  }else{
	    //pagedown
	    f3d_lcd_fillScreen(BACKGROUNDCOLOR);
	    play->wx++;
	    play->crect.t = 127;
	    play->crect.b = 127-24;
	    //      	printf("falling into a trap\n");
	  }
	}else{
	  play->drawFlag = 1;
	  play->jump_flag = 2;
	  play->crect.pt = play->crect.t;
	  play->crect.pb = play->crect.b;
	  play->crect.t = play->crect.t - JUMPSPEED;
	  play->crect.b = play->crect.b - JUMPSPEED;
	}
      }
    }
  } 
}
void draw_player(player_t * play, uint16_t * buffer){
  //int dl, dr, dt, db;
  static int prevdir = 0;
  //dl = play->crect.l - play->crect.pl;
  //dr = play->crect.r - play->crect.pr;
  //dt = play->crect.t - play->crect.pt;
  //db = play->crect.b - play->crect.pb;
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //printf("X: %d, Y: %d", play->wx,play->wy);
  if (play->drawFlag == 1){
    if (prevdir == play->dir){
      play->anim_tick = play->anim_tick + 1;
    }else{
      play->anim_tick = 0;
    }
    prevdir = play->dir;
    if (play->jump_flag == 1){
      if (play->dir == 1){
	loadPlayer("anim/JUMPR.BMP",buffer);
      } else if (play->dir == 2){
	loadPlayer("anim/JUMPL.BMP",buffer);
      } else {
	loadPlayer("anim/JUMP.BMP",buffer);
      }
    } else if (play->jump_flag == 2){
           if (play->dir == 1){
	loadPlayer("anim/FALLR.BMP",buffer);
      } else if (play->dir == 2){
	loadPlayer("anim/FALLL.BMP",buffer);
      } else {
	loadPlayer("anim/FALL.BMP",buffer);
      }
    } else if (play->dir == 1){
      switch ((play->anim_tick % 8))
	{
	case 0:
	  loadPlayer("anim/WALKR1.BMP",buffer);
	  break;
	case 1:
	  loadPlayer("anim/WALKR2.BMP",buffer);
	  break;
	case 2:
	  loadPlayer("anim/WALKR3.BMP",buffer);
	  break;
	case 3:
	  loadPlayer("anim/WALKR4.BMP",buffer);
	  break;
	case 4:
	  loadPlayer("anim/WALKR5.BMP",buffer);
	  break;
	case 5:
	  loadPlayer("anim/WALKR6.BMP",buffer);
	  break;
	case 6:
	  loadPlayer("anim/WALKR7.BMP",buffer);
	  break;
	case 7:
	  loadPlayer("anim/WALKR8.BMP",buffer);
	  break;
	}
    }else if (play->dir == 2){
      switch ((play->anim_tick % 8))
	{
	case 0:
	  loadPlayer("anim/WALKL1.BMP",buffer);
	  break;
	case 1:
	  loadPlayer("anim/WALKL2.BMP",buffer);
	  break;
	case 2:
	  loadPlayer("anim/WALKL3.BMP",buffer);
	  break;
	case 3:
	  loadPlayer("anim/WALKL4.BMP",buffer);
	  break;
	case 4:
	  loadPlayer("anim/WALKL5.BMP",buffer);
	  break;
	case 5:
	  loadPlayer("anim/WALKL6.BMP",buffer);
	  break;
	case 6:
	  loadPlayer("anim/WALKL7.BMP",buffer);
	  break;
	case 7:
	  loadPlayer("anim/WALKL8.BMP",buffer);
	  break;
	}
    } else {
      loadPlayer("anim/IDLE.BMP",buffer);
    }
    //f3d_lcd_rect(play->crect.b,play->crect.l,play->crect.t,play->crect.r,play->c);
    f3d_lcd_drawPlayer(play->crect.b,play->crect.l,play->crect.t-1,play->crect.r,buffer);
    mask(play);
    play->drawFlag = 0;    
  } 
}

//********************Rectangle Functions*********************
void printRect(rect_t * r){
  printf("L: %d R: %d T: %d B: %d\n",r->l,r->r,r->t,r->b);
}

void binit_crect(int l, int r, int t, int b, rect_t * rect){
  rect->l = l;
  rect->r = r;
  rect->t = t;
  rect->b = b;
}

//********************Nunchuk fucntions***********************
int ctoggle(nunchuk_t * chuk){
  static state = 0;
  if (chuk->c == 1){
    if (state == 0){
      state = 1;
      return 1;}}
  else {state = 0;}
  return 0;
}

int ztoggle(nunchuk_t * chuk){
  static state = 0;
  if (chuk->z == 1){
    if (state == 0){
      state = 1;
      return 1;}}
  else {state = 0;}
  return 0;
}

void init() {
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  f3d_delay_init();
  f3d_uart_init();
  delay(100);
  f3d_led_init();
  f3d_i2c1_init();
  delay(100);
  f3d_i2c2_init();
  delay(100);
  f3d_nunchuk_init();
  delay(1000);
  f3d_nunchuk2_init();
  delay(100);
  f3d_lcd_init();
  delay(100);
  f3d_rtc_init();
  delay(100);
  delay(100);
  delay(100);
  f3d_systick_init();
  delay(100);
  f3d_dac_init();
  delay(100);
  f3d_timer2_init();
}
void pickupInit(pickup_t * one, int ty, int l, int r, int t, int b){ 
  binit_crect(l,r,b,t,&one->crect);
  one->type = ty;
  one->vis_flag = 1;
}


void readckhd(FIL *fid, struct ckhd *hd, uint32_t ckID) {
  f_read(fid, hd, sizeof(struct ckhd), &ret);
  if (ret != sizeof(struct ckhd))
    exit(-1);
  if (ckID && (ckID != hd->ckID))
    exit(-1);
}


int playsound(char * name){
  FRESULT rc;			/* Result code */
  DIR dir;			/* Directory object */
  FILINFO fno;			/* File information object */
  UINT bw, br;
  unsigned int retval;
  int bytesread;
  
  f_mount(0, &Fatfs);/* Register volume work area */
  
  printf("\nOpen thermo.wav\n");
  rc = f_open(&fid, name, FA_READ);
  
  if (!rc) {
    struct ckhd hd;
    uint32_t  waveid;
    struct fmtck fck;
    
    readckhd(&fid, &hd, 'FFIR');
    
    f_read(&fid, &waveid, sizeof(waveid), &ret);
    if ((ret != sizeof(waveid)) || (waveid != 'EVAW'))
      return -1;
    
    readckhd(&fid, &hd, ' tmf');
     
    f_read(&fid, &fck, sizeof(fck), &ret);
    
    // skip over extra info
    
    if (hd.cksize != 16) {
      printf("extra header info %d\n", hd.cksize - 16);
      f_lseek(&fid, hd.cksize - 16);
    }
    
    printf("audio format 0x%x\n", fck.wFormatTag);
    printf("channels %d\n", fck.nChannels);
    printf("sample rate %d\n", fck.nSamplesPerSec);
    printf("data rate %d\n", fck.nAvgBytesPerSec);
    printf("block alignment %d\n", fck.nBlockAlign);
    printf("bits per sample %d\n", fck.wBitsPerSample);
    
    // now skip all non-data chunks !
    
    while(1){
      readckhd(&fid, &hd, 0);
      if (hd.ckID == 'atad')
	break;
      f_lseek(&fid, hd.cksize);
    }
    
    printf("Samples %d\n", hd.cksize);
    
    // Play it !
    
    //      audioplayerInit(fck.nSamplesPerSec);
    
    f_read(&fid, Audiobuf, AUDIOBUFSIZE, &ret);
    hd.cksize -= ret;
    audioplayerStart();
    while (hd.cksize) {
      int next = hd.cksize > AUDIOBUFSIZE/2 ? AUDIOBUFSIZE/2 : hd.cksize;
      if (audioplayerHalf) {
	if (next < AUDIOBUFSIZE/2)
	  bzero(Audiobuf, AUDIOBUFSIZE/2);
	f_read(&fid, Audiobuf, next, &ret);
	hd.cksize -= ret;
	audioplayerHalf = 0;
      }
      if (audioplayerWhole) {
	if (next < AUDIOBUFSIZE/2)
	  bzero(&Audiobuf[AUDIOBUFSIZE/2], AUDIOBUFSIZE/2);
	f_read(&fid, &Audiobuf[AUDIOBUFSIZE/2], next, &ret);
	hd.cksize -= ret;
	audioplayerWhole = 0;
      }
    }
    audioplayerStop();
  }
  
  printf("\nClose the file.\n"); 
  rc = f_close(&fid);
  if (rc) die(rc);

}



void die (FRESULT rc) {
  printf("Failed with rc=%u.\n", rc);
  while (1);
}
 
#ifdef USE_FULL_ASSERT 
 void assert_failed(uint8_t* file, uint32_t line) {  
   //Infinite loop
   //Use GDB to find out why we're here 
  while (1); 
 }
#endif
void pickupinit(void){
  pickupInit(&world[3][2].pickup,2,56,72,16,32); //doublejump 
  pickupInit(&world[0][4].pickup,5,143,159,101,117); //KEY
  pickupInit(&world[4][2].pickup,3,123,139,56,72); //JETPACK
  pickupInit(&world[1][2].pickup,1,42,58,26,42); //HOME
}

void LOADWORLDONE(void){
  pickupinit();
  //pickupInit(&world[3][2].pickup,2,56,72,16,32); 
  //  pickupInit(&world[3][2].pickup,2,56,72,16,32); 
  loadSpace("map/0/0.lvl",0,0);
  loadSpace("map/0/1.lvl",0,1);
  loadSpace("map/0/2.lvl",0,2);
  loadSpace("map/0/3.lvl",0,3);
  loadSpace("map/0/4.lvl",0,4);  

  loadSpace("map/1/0.lvl",1,0);
  loadSpace("map/1/1.lvl",1,1);
  loadSpace("map/1/2.lvl",1,2);
  loadSpace("map/1/3.lvl",1,3);
  //loadSpace("map/1/4.lvl",1,4); //BLANK

  loadSpace("map/2/0.lvl",2,0);
  loadSpace("map/2/1.lvl",2,1);
  loadSpace("map/2/2.lvl",2,2);
  loadSpace("map/2/3.lvl",2,3);
  //loadSpace("map/2/4.lvl",2,4); //BLANK

  loadSpace("map/3/0.lvl",3,0);
  loadSpace("map/3/1.lvl",3,1);
  loadSpace("map/3/2.lvl",3,2); 
  loadSpace("map/3/3.lvl",3,3);
  loadSpace("map/3/4.lvl",3,4);

  loadSpace("map/4/0.lvl",4,0);
  loadSpace("map/4/1.lvl",4,1);
  loadSpace("map/4/2.lvl",4,2);
  loadSpace("map/4/3.lvl",4,3);
  //loadSpace("map/4/4.lvl",4,4); //BLANK
}

/* main.c ends here */
