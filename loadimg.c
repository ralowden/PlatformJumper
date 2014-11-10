/* main.c --- 
 * 
 * Filename: main.c
 * Description: 
 * Author: 
 * Maintainer: 
 * Created: Thu Jan 10 11:23:43 2013
 * Last-Updated: 
 *           By: 
 *     Update #: 0
 * Keywords: 
 * Compatibility: 
 * 
 */

/* Commentary: 
 * 
 * 
 * 
 */

/* Change log:
 * 
 * 
 */
/* Code: */
#include <loadimg.h>
#include <stdio.h>
void die (FRESULT rc) {
  printf("Failed with rc=%u.\n", rc);
  while (1);
}


int openImage(char* FILENAME, uint16_t *loadspace){
  printf("called\n");
  FIL fimg;
  FRESULT rc;
  struct bmpfile_magic magic;
  struct bmpfile_header header;
  BITMAPINFOHEADER info;
  int f,progress;
  UINT r;
  //uint16_t screenbuffer[128];
  int w,h,size,count,row;
  bmppixel pixelbuffer;
  printf("openattempt\n");
  //Attempt to open file
  rc = f_open(&fimg, FILENAME, FA_READ);
  if (rc) die(rc);
    //{
    //  printf("problem opening file\n");
    //  return 1;
    //}else{
    //printf("success\n");}
    
  //read file header & info
  f_read(&fimg, &magic, sizeof magic,&r);
  f_read(&fimg, &header, sizeof header,&r);
  f_read(&fimg, &info, sizeof info,&r);

  
  h = info.height;
  w = info.width;
  size = h*w;
  count = 0;
  progress = 0;
  row = 127;
  while (count < 20){//384
    f_read(&fimg, &pixelbuffer, sizeof pixelbuffer,&r);
    loadspace[count++] = (((pixelbuffer.r >> 3) << 11) + ((pixelbuffer.g >> 2) << 5) + (pixelbuffer.b >> 3));
  }

  /*
  while (row >= 0){
    while (count < 128){
      f_read(&fimg, &pixelbuffer, sizeof pixelbuffer,&r);
      screenbuffer[count++] = (((pixelbuffer.r >> 3) << 11) + ((pixelbuffer.g >> 2) << 5) + (pixelbuffer.b >> 3));
    }
    count = 0;
    row--;
    f3d_lcd_rowBuffer(row,screenbuffer);
    
    printf("%d",row);
  }

*/
  f_close (&fimg);
  
  return 0;

}

