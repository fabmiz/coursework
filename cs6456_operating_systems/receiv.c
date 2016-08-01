void main(){
  char msg[100];
  enableInterrupts();
  interrupt(0x21,11,msg,0,0);
  interrupt(0x21,0,"Got mail: ",0,0);
  interrupt(0x21,0,msg,0,0);
  //interrupt(0x21,5,0,0,0);
  while(1);
}
