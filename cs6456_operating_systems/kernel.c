void main2();
void main(){ main2();}

int CurrentProcess; //currently running process

typedef struct{
  int active;
  int sp;
} entryProcessTable;
entryProcessTable ProcessTable[8];

void currentDir();
void initializeFS();
void createDir(char* dirname);
void getMessage(char* msg);
void sendMessage(char* mail,int rcp);
void chDir(char* dirname);
void killProcess(int process);
void handleTimerInterrupt(int segment, int sp);
void terminate();
void executeProgram(char* name);
void writeFile(char* filename,char inbuf[]);
void readFile(char* filename, char outbuf[]);
void deleteFile(char* filename);
void directory();
void writeSector(char* buffer, int sector);
void eraseSector(int sector);
void readSector(char* buffer, int sector);
void printString(char* str);
void readString(char* buffer);
void handleInterrupt21(int ax, int bx, int cx, int dx);
int MOD(int x, int y);
int DIV(int x, int y);

void main2()
{
  int i,j,k;
  setKernelDataSegment();
  for(i=0; i<8; i++){
    ProcessTable[i].active = 0;
    ProcessTable[i].sp=0xff00;
  }
  restoreDataSegment();
  CurrentProcess=0;

  //for(j=0;j<56;j++) putInMemory(0x0000,j*100,0);
  for(k=0;k<4;k++) putInMemory(0x0000,k,0);

  makeInterrupt21();
  makeTimerInterrupt();
  interrupt(0x21, 9, "shell", 0, 0);
}


void initializeFS(){
  int i=0,j=0,k=0;
  char dir[512];
  readSector(dir,2);
  for(i=0;dir[i]!=0;i+=0x20);
  dir[480]='-';
  dir[481]='r';
  dir[482]='o';
  dir[483]='o';
  dir[484]='t';
  for(j=0;j<DIV(i,0x20);j++) dir[485+j]=j+1;
  for(k=0;k<4;k++) putInMemory(0x0000,k,dir[481+k]);
  writeSector(dir,2);

}

void createDir(char* dirname){
	char dir[512];
	int count=0,k,j,i;
	readSector(dir,2);

	for(j=384; j<480; j+=32){
		if(dir[j] == '-') count++;
		else
			break;
	}
	if(count < 4){
	   	dir[j]='-';
		for(i=0;i<4;i++) dir[j+1+i]=dirname[i];
	}
	for(k=0;k<32;k++) if(dir[480+k] == 0) break;
	dir[480+k]= DIV(j,0x20)+1;
	writeSector(dir,2);
}


void currentDir(){
   char cwd[7];
   int i;
   for(i=0;i<4;i++) cwd[i]=readFromMemory(0x0000,i);
   cwd[4]=0x0A;
   cwd[5]=0x0D;
   cwd[6]=0;
   printString(cwd);
}

void chDir(char* dirname){
  int i;
  for(i=0;i<5;i++) putInMemory(0x0000,i,dirname[i]);
}


void sendMessage(char* mail, int rcp){
  int currentProc,countMsg=0,i=0,segment=0x0000,base;

  setKernelDataSegment();
  currentProc=CurrentProcess;
  restoreDataSegment();

  base=((rcp-1)*700)+((currentProc-1)*100);
  while(mail[i] != 0){
	  putInMemory(segment,base+1+i,mail[i]);
	  i++;
  }
  putInMemory(segment,base,++countMsg);
}


void getMessage(char* msg){
  int i=0,j=0,currentProc,k=0,base,segment=0x0000;
  int numMsg=0,boxNum,timeStamp,oldest=7,oldestMsg;

  setKernelDataSegment();
  currentProc=CurrentProcess;
  restoreDataSegment();

  boxNum=(currentProc-1)*7;

  for(i=0; i<100;++i) msg[i]=0;

  while(msg[0] == 0){
	  for(j=0;j<7;++j){
			if((timeStamp=readFromMemory(segment,(boxNum+j)*100)) > 0){
				numMsg++;
				if(timeStamp < oldest){
					oldest=timeStamp;
					oldestMsg=j;
				}
		   }
	  }
      if(numMsg != 0) while((msg[k]=readFromMemory(0x0000,((boxNum+oldestMsg)*100)+1+k)) != 0) k++;
  }

}



void killProcess(int process){
  setKernelDataSegment();
  ProcessTable[process].active = 0;
  ProcessTable[process].sp = 0xff00;
  restoreDataSegment();
}

void terminate(){
  setKernelDataSegment();
  ProcessTable[CurrentProcess].active = 0;
  restoreDataSegment();
  setKernelDataSegment();
  ProcessTable[CurrentProcess].sp = 0xff00;
  restoreDataSegment();
  while(1);
}

void handleTimerInterrupt(int segment, int sp){
	int k=0;
	if(segment == 0x1000){
		segment=0x2000;
	    setKernelDataSegment();
		sp = 0xff00;
	    restoreDataSegment();
	    setKernelDataSegment();
	    ProcessTable[0].active=1;
	    restoreDataSegment();
	}

     	setKernelDataSegment();
	if(ProcessTable[CurrentProcess].active == 1){
	    restoreDataSegment();
	    setKernelDataSegment();
 		ProcessTable[CurrentProcess].sp = sp;
	    restoreDataSegment();
    }
	setKernelDataSegment();
    k=CurrentProcess+1;
	restoreDataSegment();
    setKernelDataSegment();
	while(ProcessTable[MOD(k,8)].active == 0){
        setKernelDataSegment();
	    k++;
	   if(MOD(k,8) == CurrentProcess)
           break;
    	restoreDataSegment();

    }
    restoreDataSegment();
	setKernelDataSegment();
         if(MOD(k,8) != CurrentProcess) {
			 restoreDataSegment();
			 setKernelDataSegment();
			 CurrentProcess = MOD(k,8);
			 restoreDataSegment();
			 setKernelDataSegment();
			 ProcessTable[CurrentProcess].active=1;
			 restoreDataSegment();
			 setKernelDataSegment();
			 segment=0x2000+0x1000*CurrentProcess;
			 restoreDataSegment();
			 setKernelDataSegment();
			 sp=ProcessTable[CurrentProcess].sp;
			 restoreDataSegment();
		}
	returnFromTimer(segment,sp);
}

void executeProgram(char* name){
   char prog[4096];
   int i=0,j=0,segment;
   readFile(name,prog);
   setKernelDataSegment();
   while(ProcessTable[MOD(j,8)].active == 1){ j++; }
   restoreDataSegment();
   segment=0x2000 + (0x1000*MOD(j,8));
   while(i < 4096){
     putInMemory(segment,i,prog[i]);
     i++;
   }
   setKernelDataSegment();
   ProcessTable[MOD(j,8)].active=1;
   restoreDataSegment();

initializeProgram(segment);
}

void writeFile(char* filename,char inbuf[]){
   int i=0,m=0,l=0,j=0,k=0,p=0,count=0;
   char map[512];
   char dir[512];
   char cwd[5];

   readSector(map,1); //read in MAP
   readSector(dir,2); //read in DIR
  for(p=0;p<5;p++) cwd[p]=readFromMemory(0x0000,p); //get current working dir

   while(map[i] != 0x0){
     i++;                  //find next free sector
   }
   map[i] = 0xFF;         //taken
   writeSector(map,1); //write MAP back
   while(dir[j*32] != 0x0){
    	j++;  //find next free spot in DIR
  	}
  while(k<6){
     dir[j*32+k] = filename[k]; //write filename to DIR
     k++;
  }
  dir[j*32+k] = i+1;

  for(l=384; l<512; l+=32){
     if(dir[l] == '-'){
		for(m=0;m<4;m++){
			if(dir[l+1+m] != cwd[m]) break;
		}
	 if(m==4) break;
     }
  }

  if(m==4){
	for(m=0;dir[l+m]!=0; m++);
	dir[l+m] = j+1;
  }
  writeSector(dir,2);
  writeSector(inbuf,i+1);
}

void readFile(char* filename, char outbuf[]){
  int i,j,k=0,h=0,l,index,count, nameSize=0;
  int file_sectors[26];
  char dir[512];
  char tempbuf[512];
  readSector(dir,2);
  for(i=0; i<16; i++){
    count=0;
    for(j=i*32;dir[j]!=0x0;j++){
       if(filename[count] == dir[j]){
         count++;
       }
       else
         break;
    }
    if(dir[j]==0x0){
        index = (i*32) + 6;         // i*32
        break;
    }
  }
  while(dir[index+k] != 0x0){
     file_sectors[k] = dir[index+k];
     k++;
  }
  j=0;
  while(j < k){
      readSector(tempbuf,file_sectors[j]);
      l=0;
      while(l<512){
         outbuf[(512*j)+l] = tempbuf[l];
         l++;
      }
      j++;
  }

}

void deleteFile(char* filename){
  char map[512],dir[512];
  int file_sectors[26];
  int i,j,k=0,l,m,index,count;
  static int empty[512] = {0};
  //read map and directory into map[512],dir[512] respectively.
  readSector(map,1);
  readSector(dir,2);

  //Search for filename in dir[512]
  for(i=0; i<16; i++){
    count=0;
    for(j=i*32;j<(i*32)+6;j++){
       if(filename[count] == dir[j]){
         count++;
       }
       else
         break;
    }
    if(count == 6){
        index = (i*32) + 6;         // i*32
        break;
    }
  }
  //get sectors allocated to "filename" from dir[512]
  while(dir[index+k] != 0x0){
     file_sectors[k] = dir[index+k];
     writeSector(empty,file_sectors[k]);
     k++;
  }

  //Erase sectors from dir and map

  for(l=0; l<k;l++){
     map[file_sectors[k]-1] = 0x0; //erase from map
     writeSector(empty,file_sectors[l]);
  }

  for(m=0; m<32;m++){
     dir[i*32+m] = 0x0;   //erase from dir
  }
  //write back map and dir
  writeSector(map,1);
  writeSector(dir,2);

}


void directory(){
   int i,j,k,l,m,pos=0;
   char dir[512];
   char cwd[5];
   readSector(dir,2);
   for(m=0; m<5;m++) cwd[m]=0;
   for(i=0;i<5;i++) cwd[i]=readFromMemory(0x0000,i);

   for(j=384; j<512; j+=32){
     if(dir[j] == '-'){
		for(k=0;k<4;k++){
			if(dir[j+1+k] != cwd[k]) break;
		}
     }
     if(k==4){
		for(l=0;dir[j+5+l]!=0;l++){
	  		pos=dir[j+5+l]-1;
            for(m=pos*32; m<(pos*32)+6; m++){
				interrupt(0x10,0xe*256+dir[m],0,0,0);
			}
	 		interrupt(0x10,0xe*256+0x0A,0,0,0);
			interrupt(0x10,0xe*256+0x0D,0,0,0);
		}

	 }
  }

/*
  for(i=0;i<16;i++){
    for(j=i*32;j<(i*32)+6;j++){
       if(dir[i*32] == 0x0)
         break;
       interrupt(0x10,0xe*256+dir[j],0,0,0);
       if(j == (i*32)+5){
         interrupt(0x10,0xe*256+0x0A,0,0,0);
         interrupt(0x10,0xe*256+0x0D,0,0,0);
       }
    }
  }
*/

}

void handleInterrupt21(int ax, int bx, int cx, int dx){
 switch(ax){
   case 0:
      printString(bx);
      break;
   case 1:
      readString(bx);
      break;
   case 2:
      readSector(bx,cx);
      break;
   case 3:
      directory();
      break;
   case 4:
      deleteFile(bx);
      break;
   case 5:
      terminate();
      break;
   case 6:
      readFile(bx,cx);
      break;
   case 8:
      writeFile(bx,cx);
      break;
   case 9:
      executeProgram(bx);
      break;
   case 7:
      killProcess(bx);
      break;
   case 10:
	  sendMessage(bx,cx);
	  break;
   case 11:
   	  getMessage(bx);
	  break;
   case 12:
   	  chDir(bx);
	  break;
   case 13:
	  initializeFS();
	  break;
   case 14:
	  currentDir();
	  break;
   case 15:
	  createDir(bx);
	  break;
   }
}


int DIV(int x, int y){
   int q = 0,p,quotient;
   while((p = y*q) < x){
     q=q+1;
   }
   if(p == x)
     quotient=q;
   else
     quotient=q-1;

   return quotient;
}

int MOD(int x, int y){
   while(x >= y)
    x=x-y;

   return x;
}
void writeSector(char* buf, int sector){
   int AX,CX,DX;
   AX = 3*256 + 1;
   CX = DIV(sector,36)*256 + MOD(sector,18) + 1;
   DX = MOD(DIV(sector,18),2)*256;
   interrupt(0x13,AX,buf,CX,DX);
}

void readSector(char* buffer, int sector){

   int AX,CX,DX;
   AX = 2*256 + 1;
   CX = DIV(sector,36)*256 + MOD(sector,18) + 1;
   DX = MOD(DIV(sector,18),2)*256;
   interrupt(0x13,AX,buffer,CX,DX);

}

void printString(char* str){
   int  i=0;
   while(str[i] != '\0'){
       interrupt(0x10,0xe*256+str[i],0,0,0);
       i=i+1;
   }

}

void readString(char* buffer){
  int i=0,k;
  while((k=interrupt(0x16,0,0,0,0)) != 0xd){
	 if(k == 0x8 && i > 0){
        interrupt(0x10,0xe*256+k,0,0,0);
        i=i-1;
     }
     else {
        buffer[i]=k;
        interrupt(0x10,0xe*256+buffer[i],0,0,0);
        i=i+1;
        }

  }

  buffer[i]=0xA;
  buffer[i+1]=0xd;
  buffer[i+2] = 0x0;
}
