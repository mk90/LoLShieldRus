#include "Charliplexing.h"
#include "Font.h"

//#include "WProgram.h"

//char test[]="AБВГДEЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ   ";
char test[]="ГЕРОИЧЕСКИЙ И ПЛАМЕННЫЙ ПРИВЕТ ЗАЩИТНИКАМ ОДЕССЫ ОТ ДОБЛЕСТНЫХ ЗАЩИТНИКОВ СЕВАСТОПОЛЯ!";
//char test[]="ABCDEFGH ";

unsigned int StrDraw(char *s, unsigned int offset) {
  unsigned int l = 0, l1=0, k = 0;
  Serial.println( (int) (l-offset) );
  while ( s[k] && ( (int)(l-offset) < 14) ) {
    l += Font::Draw(s[k],l-offset,0);
    if (!k) l1 = l;
    Serial.println(s[k]);
    k++;
  }
  return l1;
}

void setup() {
  Serial.begin(9600);
  LedSign::Init();
}


void loop() {
  LedSign::Clear();
  unsigned int i=0, sl=strlen(test);
  while (i < sl) {
    byte x = 0;
    unsigned int l=0;
    do {
      l = StrDraw(test+i,x);
      if (l) delay(50);
      LedSign::Clear();
      x++;
    } while (l>x);
    i++;
  }
}
