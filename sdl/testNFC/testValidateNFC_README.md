rm -f testValidateNFC.o ; gcc testValidateNFC.c -o testValidateNFC.o

# Type text
## send data (first data)
./testValidateNFC.o 1 0 0 12345 ; 
sleep 8 ; 
##send detect 
./testValidateNFC.o 1 0 1 12345 ; //
sleep 8 ; 

##send data ( not the first)
./testValidateNFC.o 1 0 2 6789 ;
sleep 8 ; 
##send detect
./testValidateNFC.o 1 0 1 6789 


## Type URI
##send data (first data)
./testValidateNFC.o 0 0 0 google.com ; 
sleep 8 ; 
##send detect 
./testValidateNFC.o 0 0 1 google.com ; 
sleep 8 ; 

##send data ( not the first)
./testValidateNFC.o 0 0 2 blog.zenika.com ;
sleep 8 ; 
##send detect
./testValidateNFC.o 0 0 1 blog.zenika.com

# Type Smart Poster
## send data (first data)
##send data (first data)
./testValidateNFC.o 2 0 0 google.com ;
sleep 8 ;
##send detect
./testValidateNFC.o 2 0 1 google.com ;
sleep 8 ;

##send data ( not the first)
./testValidateNFC.o 2 0 2 blog.zenika.com ;
sleep 8 ;
##send detect
./testValidateNFC.o 2 0 1 blog.zenika.com
