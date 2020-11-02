main(){

    while(loop){
        // in ra man hinh ssh>
        printf("osh >");

        // lay input
        input = layinput(); 

        if(input == null){
            continue;
        }

        int type=processInput(inputLine,token1,token2, newthread);
        // tách làm 2 phần .. | > < .....
        // kiểm tra luôn có & không và lưu vào bool newthread
        // trả về type 
        // >:1, <:2, |:3


        process(tokenArr1, tokenArr2, type, newthread)
        // trong này xử lí luôn việc exit        
    
    }
}

void xuly>(char* token1,char* token2,bool newthread){}
void xuly<(char* token1,char* token2, bool newthread){}
void xuly|(char* token1,char* token2, bool newthread){}


int type=processInput(char* inputLine,char* token1, char* token2, bool newthread){}

void process(char* token1, char* token2, int type,bool newthread){
    if (type==1)  { // ">"
        xuly>(token1,token2, newthread);                  
    }
    else if (type==2) // "<"
    {
        xuly<(token1,token2, newthread);
    }
    else if (type==3) // "|"
    {
        xuly|(token1,token2, newthread);   
    }
    else
    {
    
        if(token1 == "exits") -> loop = false
        else if(token1 == "!!"){
            // thuc thi cau lenh chua trong history
        }
        else if(token1 == null){
            // in ra man hinh thong bao cau lenh rong
        }
        else{
            execArgv(tokenArr1);        
        }
    }
}
        