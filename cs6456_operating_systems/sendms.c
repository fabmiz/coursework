void main(){
	int i=0;
	char msg[100];
	enableInterrupts();
	msg[0]='H';
	msg[1]='i';
	msg[2]=0xa;
	msg[3]=0xd;
	msg[4]=0;
	interrupt(0x21,10,msg,2,0);
	interrupt(0x21,0,"Msg sent: ",0,0);
	interrupt(0x21,0,msg,0,0);
	while(1);
	//interrupt(0x21,5,0,0,0);
}
