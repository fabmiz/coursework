int strcmp(char* str1, char* str2);

void main()
{
    char command[50];
    char prog[10];
    char arg[20];
    int temp;
    char buffer[13312];
    int i,k,j,m,count,l;
    char line[100]; 
    char cwd[5];
    enableInterrupts();
	interrupt(0x21,13,0,0,0);
    while(1){
       i=k=m=0; 
       interrupt(0x21, 0,"SHELL$ ", 0, 0);
       interrupt(0x21, 1,command, 0, 0);
       interrupt(0x21, 0,"\r\n", 0, 0);
       for(m=0;m<5;m++) cwd[m]=0; 
       if(strcmp("dir",command)){
	   		interrupt(0x21,3,0,0,0);
	   		continue;
		}
	   else if(strcmp("pwd",command)){
	   		interrupt(0x21,14,0,0,0);
			continue;
		}
       else
         {
				while((prog[i]=command[i]) != 0x20) i++;	
				while((arg[k]=command[k+i+1]) != '\0') k++;
   		 }	
   
	   if(strcmp("type",prog)){
		  interrupt(0x21,6,arg,buffer,0);
		  interrupt(0x21,0,buffer,0,0);
	   }
	   else if(strcmp("mkdir",prog)){
		  interrupt(0x21,15,arg,0,0);
	   } 
	 /*  else if(strcmp("del",prog))   
		 interrupt(0x21,4,arg,0,0);
	 */	   
	   else if(strcmp("execute",prog)){
		  interrupt(0x21, 9, arg, 0, 0);
	   }
	  /* else if(strcmp("kill",prog)){
		  temp=arg[0];
		  interrupt(0x21, 7, temp -'0', 0, 0);
	   }
	  */
	   else if(strcmp("cd",prog)){
		  for(l=0;l<4;l++) cwd[l]=arg[l];	
		  if(cwd[0]==0){
			cwd[0]='r';
			cwd[1]='o';
			cwd[2]='o';
			cwd[3]='t';
			cwd[4]=0;
		  }		  	
		  interrupt(0x21,12,cwd,0,0);
	   }	
   	   else if(strcmp("create",prog)){
			  buffer[0]='\0';
			  count=0;
			  line[0]='\0'; 
			  do {
				   line[0]='\0';
				   j=0;
				   interrupt(0x21, 1, line, 0, 0);
				   while((buffer[count]=line[j]) != '\0'){
						count++;
						j++;
				   }
				   interrupt(0x21,0,"\r\n",0,0);

			 } while(line[0] != 0x0A);
	
			 interrupt(0x21,8,arg,buffer,0);
	   }
   
        
    } 

}
      
int strcmp(char* str1, char* str2){
   int i=0;
   while(str1[i] != '\0'){
      if(str1[i] != str2[i])
	    return 0;
      i++;  
   }
   return 1;
} 
