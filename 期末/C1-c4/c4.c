#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>
//此處引入標頭檔 C4會自動忽略並轉為類似printf等類函式庫

char *p, *lp, // 當前源代碼位置 (*:指標 p: 目前原始碼指標, lp: 上一行原始碼指標)
     *data;   // 資料段機器碼指標

int *e, *le,  // 發出代碼目前位置 (e: 目前機器碼指標, le: 上一行機器碼指標)
    *id,      // 目前解析出來的標識名稱(符) (id: 目前的 id)
    *sym,     // 簡單的標識列表 (符號表)
    tk,       // 當前標記(目前 token)
    ival,     // 當前標記值 (目前的 token 值)
    ty,       // 當前表示式型態 (目前的運算式型態)
    loc,      // 區域變數位移 (區域變數的位移)
    line,     // 目前行號 (目前行號)
    src,      // 印出原始碼和程序標誌 (印出原始碼)
    debug;    // 印出執行指令 (印出執行指令 -- 除錯模式)

// tokens and classes (運算符最後並按優先順序排列) (按優先權順序排列)
enum { // token(標記) : 0-127 直接用該字母表達， 128 以後用代號。
  Num = 128, Fun, Sys, Glo, Loc, Id,
  Char, Else, Enum, If, Int, Return, Sizeof, While,
  Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};

// opcodes (機器碼的 op)
// LEA : load local address 載入區域變數 
// IMM : load global address or immediate 載入全域變數或立即值 
// JMP : jump               躍躍指令  
// JSR : jump to subroutine 跳到副程式 
// BZ  : branch if zero     if (a==0) goto m[pc] 
// BNZ : branch if not zero if (a!=0) goto m[pc]   
// ENT : enter subroutine   進入副程式 
// ADJ : stack adjust       調整堆疊
// LEV : leave subroutine   離開副程式 
// LI  : load int           載入整數
// LC  : load char          載入字元 
// SI  : store int          儲存整數
// SC  : store char         儲存字元  
// PSH : push               推入堆疊   
// OR  : a = a OR pop       OR(|) pop 代表從堆疊中取出一個元素 
// XOR : a = a XOR pop      XOR(^)
// AND : a = a AND pop      AND(&)
// EQ : a = a EQ pop        EQ(==)
// NE : a = a NE pop        NE(!=)
// LT : a = a LT pop        LT(<)
// GT : a = a GT pop        GT(>)
// LE : a = a LE pop        LE(<=)
// GE : a = a GE pop        GE(>=)  
// SHL : a = a SHL pop      SHL(<<)
// SHR : a = a SHR pop      SHR(>>) 
// ADD : a = a ADD pop      ADD(+)
// SUB : a = a SUB pop      SUB(-)
// MUL : a = a MUL pop      MUL(*)
// DIV : a = a DIV pop      DIV(/)
// MOD : a = a MOD pop      MOD(%)
// EXIT : 終止離開 
enum { LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,
       OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,
       OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT };

// types (支援型態，只有 int, char, pointer)
enum { CHAR, INT, PTR };

// 因為沒有 struct，所以使用 offset 代替，例如 id[Tk] 代表 id.Tk (token), id[Hash] 代表 id.Hash, id[Name] 代表 id.Name, .....
// 標識符位移 (since we can't create an ident struct)
enum { Tk, Hash, Name, Class, Type, Val, HClass, HType, HVal, Idsz }; // HClass, HType, HVal 是暫存的備份

void next() // 詞彙解析 lexer
{
  char *pp;  // 用循環來忽略空白字符,不過不能被詞法分析器識別的字符都被認為是空白字符

  while (tk = *p) {
    ++p;
    if (tk == '\n') { // 換行
      if (src) {
        printf("%d: %.*s", line, p - lp, lp); // 印出該行 行號目前位置 上一行源代碼指標
        lp = p; // lp = p = 新一行的原始碼開頭
        while (le < e) { //  當上一行機器碼指標小於目前機器碼指標時 印出上一行的所有目的碼
          printf("%8.4s", &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,"
                           "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
                           "OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT,"[*++le * 5]);
          if (*le <= ADJ) printf(" %d\n", *++le); else printf("\n"); // LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ 有一個參數。
        }// 假如上一行機器碼指標小於等於調整堆疊 
      }
      ++line;
    }
    else if (tk == '#') { // 取得 #include <stdio.h> 引入標頭檔這類的一整行
      while (*p != 0 && *p != '\n') ++p;//當目前原始碼指標不等於零and不為空值
    }
    else if ((tk >= 'a' && tk <= 'z') || (tk >= 'A' && tk <= 'Z') || tk == '_') { // 取得變數名稱
      pp = p - 1;//因为有++p,pp回退一个字符,pp指向 [这个符號] 的首字母
      while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')//目前原始碼指標(大於等於a and 小於等於z)or(大於等於A and 小於等於Z)or(大於等於0 and 小於等於9)or為空值
        tk = tk * 147 + *p++;  // 計算雜湊值
      tk = (tk << 6) + (p - pp); // 符號表的雜湊位址 ??
      id = sym;//  目前解析出來的名稱=列表
      while (id[Tk]) { // 檢查是否碰撞 ?
        if (tk == id[Hash] && !memcmp((char *)id[Name], pp, p - pp)) { tk = id[Tk]; return; } // 沒碰撞就傳回   tokenmemcmp(const void *str1, const void *str2, size_t n)) str1、str2 -- 這就是指針的內存塊。 n -- 這是要比較的字節數。
        id = id + Idsz; // 碰撞，前進到下一格。
      }
      id[Name] = (int)pp; // id.Name = ptr(變數名稱)
      id[Hash] = tk; // id.Hash = 雜湊值
      tk = id[Tk] = Id; // token = id.Tk = Id  token 類型為 identifier
      return;
    }
    else if (tk >= '0' && tk <= '9') { // 取得數字串
      if (ival = tk - '0') { while (*p >= '0' && *p <= '9') ival = ival * 10 + *p++ - '0'; } // 十進位
      else if (*p == 'x' || *p == 'X') { // 十六進位  「x」則代表十六進位（就如「O」代表八進位)
        while ((tk = *++p) && ((tk >= '0' && tk <= '9') || (tk >= 'a' && tk <= 'f') || (tk >= 'A' && tk <= 'F'))) // 16 進位
          ival = ival * 16 + (tk & 15) + (tk >= 'A' ? 9 : 0);
      }
      else { while (*p >= '0' && *p <= '7') ival = ival * 8 + *p++ - '0'; } // 八進位
      tk = Num; // token = Number
      return;
    }
    else if (tk == '/') {
      if (*p == '/') { // 註解
        ++p;
        while (*p != 0 && *p != '\n') ++p; // 略過註解
      }
      else { // 除法
        tk = Div;//當前標記為除
        return;
      }
    }
    else if (tk == '\'' || tk == '"') { // 字元或字串
      pp = data;//等於資料段
      while (*p != 0 && *p != tk) {//直到找到匹配的引號為止
        if ((ival = *p++) == '\\') {
          if ((ival = *p++) == 'n') ival = '\n'; // 處理 \n 的特殊情況('\n' 认为是'\n' 其他直接忽略'\')
        }
        if (tk == '"') *data++ = ival; // 把字串塞到資料段裏(如果是"則視為字符串，向資料段輸入字符)
      }
      ++p;
      if (tk == '"') ival = (int)pp; else tk = Num; // (若是字串) ? (ival = 字串 (在資料段中的) 指標) : (字元值) 
      //雙引號(")則ival指向data中字符串开始,單引號則視為数字
      return;
    } // 以下為運算元 =+-!<>|&^%*[?~, ++, --, !=, <=, >=, ||, &&, ~  ;{}()],:
    else if (tk == '=') { if (*p == '=') { ++p; tk = Eq; } else tk = Assign; return; }//等於,賦值
    else if (tk == '+') { if (*p == '+') { ++p; tk = Inc; } else tk = Add; return; }//加
    else if (tk == '-') { if (*p == '-') { ++p; tk = Dec; } else tk = Sub; return; }//減
    else if (tk == '!') { if (*p == '=') { ++p; tk = Ne; } return; }//不等於
    else if (tk == '<') { if (*p == '=') { ++p; tk = Le; } else if (*p == '<') { ++p; tk = Shl; } else tk = Lt; return; }//< ， <= ，<<
    else if (tk == '>') { if (*p == '=') { ++p; tk = Ge; } else if (*p == '>') { ++p; tk = Shr; } else tk = Gt; return; }//> ， >= ，>>
    else if (tk == '|') { if (*p == '|') { ++p; tk = Lor; } else tk = Or; return; }//或 
    else if (tk == '&') { if (*p == '&') { ++p; tk = Lan; } else tk = And; return; }//和
    else if (tk == '^') { tk = Xor; return; }
    else if (tk == '%') { tk = Mod; return; }
    else if (tk == '*') { tk = Mul; return; }
    else if (tk == '[') { tk = Brak; return; }
    else if (tk == '?') { tk = Cond; return; }
    else if (tk == '~' || tk == ';' || tk == '{' || tk == '}' || tk == '(' || tk == ')' || tk == ']' || tk == ',' || tk == ':') return;
  }
}

void expr(int lev) // 表示式分析(運算式) expression, 其中 lev 代表優先等級
{
  int t, *d;

  if (!tk) { printf("%d: unexpected eof in expression\n", line); exit(-1); } // EOF
  else if (tk == Num) { *++e = IMM; *++e = ival; next(); ty = INT; } // 取數為表達示數值
  else if (tk == '"') { // 字串
    *++e = IMM; *++e = ival; next();
    while (tk == '"') next();
    data = (char *)((int)data + sizeof(int) & -sizeof(int)); ty = PTR; // 用 int 為大小對齊 ??
  }
  else if (tk == Sizeof) { // 處理 sizeof(type) ，其中 type 可能為 char, int 或 ptr
    next(); if (tk == '(') next(); else { printf("%d: open paren expected in sizeof\n", line); exit(-1); }
    ty = INT; if (tk == Int) next(); else if (tk == Char) { next(); ty = CHAR; }
    while (tk == Mul) { next(); ty = ty + PTR; }//多级指標,每多一级加PTR
    if (tk == ')') next(); else { printf("%d: close paren expected in sizeof\n", line); exit(-1); }
    *++e = IMM; *++e = (ty == CHAR) ? sizeof(char) : sizeof(int);//除了char是一字節,int和多级指標都是int大小
    ty = INT;
  }
  else if (tk == Id) { // 處理 id ...
    d = id; next();
    if (tk == '(') { // id (args) ，這是 call
      next();
      t = 0;//形式参數
      while (tk != ')') { expr(Assign); *++e = PSH; ++t; if (tk == ',') next(); } // 推入 arg 計算形式参数的值且傳參數
      next();
      // d[Class] 可能為 Num = 128, Fun, Sys, Glo, Loc, ...
      if (d[Class] == Sys) *++e = d[Val]; // token 是系統呼叫 ???
      else if (d[Class] == Fun) { *++e = JSR; *++e = d[Val]; } // token 是自訂函數，用 JSR : jump to subroutine 指令呼叫
      else { printf("%d: bad function call\n", line); exit(-1); }
      if (t) { *++e = ADJ; *++e = t; } // 有參數，要調整堆疊  (ADJ : stack adjust)
      ty = d[Type];//函數返回值
    }
    else if (d[Class] == Num) { *++e = IMM; *++e = d[Val]; ty = INT; } // 該 id 是數值
    else {//變量先取地址然後再LC/LI(字元，整數載入ax)
      if (d[Class] == Loc) { *++e = LEA; *++e = loc - d[Val]; } // 該 id 是區域變數，載入區域變數 (LEA : load local address)區域變數位移
      else if (d[Class] == Glo) { *++e = IMM; *++e = d[Val]; }  // 該 id 是全域變數，載入該全域變數 (IMM : load global address or immediate 載入全域變數或立即值)全域變數位移
      else { printf("%d: undefined variable\n", line); exit(-1); }
      *++e = ((ty = d[Type]) == CHAR) ? LC : LI; // LI  : load int, LC  : load char
    }
  }
  else if (tk == '(') { // (E) : 有括號的運算式 ...
    next();
    if (tk == Int || tk == Char) {//強制類型轉換
      t = (tk == Int) ? INT : CHAR; next();
      while (tk == Mul) { next(); t = t + PTR; }
      if (tk == ')') next(); else { printf("%d: bad cast\n", line); exit(-1); }
      expr(Inc); // 處理 ++, -- 的情況 (優先級)
      ty = t;
    }
    else {//一般語法括號
      expr(Assign); // 處理 (E) 中的 E      (E 運算式必須能處理 (t=x) op y 的情況，所以用 expr(Assign))
      if (tk == ')') next(); else { printf("%d: close paren expected\n", line); exit(-1); }
    }
  }
  else if (tk == Mul) { // * 乘法
    next(); expr(Inc);//高優先级
    if (ty > INT) ty = ty - PTR; else { printf("%d: bad dereference\n", line); exit(-1); }
    *++e = (ty == CHAR) ? LC : LI;
  }
  else if (tk == And) { // & AND 取得地址
    next(); expr(Inc);
    if (*e == LC || *e == LI) --e; else { printf("%d: bad address-of\n", line); exit(-1); }//token為變量時都是先取地址再LI/LC,所以 e就變成了取地址到a
    ty = ty + PTR;
  }
  else if (tk == '!') { next(); expr(Inc); *++e = PSH; *++e = IMM; *++e = 0; *++e = EQ; ty = INT; } // NOT !x等於 x==0
  else if (tk == '~') { next(); expr(Inc); *++e = PSH; *++e = IMM; *++e = -1; *++e = XOR; ty = INT; } // Logical NOT ~x等於 x ^ -1
  else if (tk == Add) { next(); expr(Inc); ty = INT; }
  else if (tk == Sub) {
    next(); *++e = IMM;
    if (tk == Num) { *++e = -ival; next(); } else { *++e = -1; *++e = PSH; expr(Inc); *++e = MUL; } // -Num or -E 乘以-1
    ty = INT;
  }
  else if (tk == Inc || tk == Dec) { // ++ or --  處理++x,--x//x--,x++在後面處理
    t = tk; next(); expr(Inc); //處理++x,--x
    if (*e == LC) { *e = PSH; *++e = LC; }//下面SC/SI用到,再取數
    else if (*e == LI) { *e = PSH; *++e = LI; }
    else { printf("%d: bad lvalue in pre-increment\n", line); exit(-1); }
    *++e = PSH;//將數值放入棧
    *++e = IMM; *++e = (ty > PTR) ? sizeof(int) : sizeof(char);//指標則加减一字,否則加減1
    *++e = (t == Inc) ? ADD : SUB;//運算
    *++e = (ty == CHAR) ? SC : SI;//存回變量
  }
  else { printf("%d: bad expression\n", line); exit(-1); }
  // 參考: https://en.wikipedia.org/wiki/Operator-precedence_parser, https://www.cnblogs.com/rubylouvre/archive/2012/09/08/2657682.html https://web.archive.org/web/20151223215421/http://hall.org.ua/halls/wizzard/pdf/Vaughan.Pratt.TDOP.pdf
  //tk為ASCII碼的都不会超過Num=128
  while (tk >= lev) { // "precedence climbing" or "Top Down Operator Precedence" method(優先爬升或自上而下的運算符優先級”方法)
    t = ty;//ty在遞迴過程中可能會被改變,所以備份當前處裡的表示式
    if (tk == Assign) {//賦值
      next();
      if (*e == LC || *e == LI) *e = PSH; else { printf("%d: bad lvalue in assignment\n", line); exit(-1); }//左邊被tk=Id中變量部分處理過,將地址傳入棧中
      expr(Assign); *++e = ((ty = t) == CHAR) ? SC : SI;//取得右值expr的值,作為a=expr的结果
    }
    else if (tk == Cond) {
      next();
      *++e = BZ; d = ++e;
      expr(Assign);
      if (tk == ':') next(); else { printf("%d: conditional missing colon\n", line); exit(-1); }
      *d = (int)(e + 3); *++e = JMP; d = ++e;
      expr(Cond);
      *d = (int)(e + 1);
    }
    else if (tk == Lor) { next(); *++e = BNZ; d = ++e; expr(Lan); *d = (int)(e + 1); ty = INT; }//短路,邏輯Or運算符左左邊true則表示式為true,不用計算運算符右側的值
    else if (tk == Lan) { next(); *++e = BZ;  d = ++e; expr(Or);  *d = (int)(e + 1); ty = INT; }//短路,邏輯And
    else if (tk == Or)  { next(); *++e = PSH; expr(Xor); *++e = OR;  ty = INT; }//將當前值Push,計算運算符右边值,再與當前值(在棧中)做運算;
    else if (tk == Xor) { next(); *++e = PSH; expr(And); *++e = XOR; ty = INT; }//expr中lev指明遞迴函数中最结合性不得低於哪一个運算符
    else if (tk == And) { next(); *++e = PSH; expr(Eq);  *++e = AND; ty = INT; }
    else if (tk == Eq)  { next(); *++e = PSH; expr(Lt);  *++e = EQ;  ty = INT; }
    else if (tk == Ne)  { next(); *++e = PSH; expr(Lt);  *++e = NE;  ty = INT; }
    else if (tk == Lt)  { next(); *++e = PSH; expr(Shl); *++e = LT;  ty = INT; }
    else if (tk == Gt)  { next(); *++e = PSH; expr(Shl); *++e = GT;  ty = INT; }
    else if (tk == Le)  { next(); *++e = PSH; expr(Shl); *++e = LE;  ty = INT; }
    else if (tk == Ge)  { next(); *++e = PSH; expr(Shl); *++e = GE;  ty = INT; }
    else if (tk == Shl) { next(); *++e = PSH; expr(Add); *++e = SHL; ty = INT; }
    else if (tk == Shr) { next(); *++e = PSH; expr(Add); *++e = SHR; ty = INT; }
    else if (tk == Add) {
      next(); *++e = PSH; expr(Mul);
      if ((ty = t) > PTR) { *++e = PSH; *++e = IMM; *++e = sizeof(int); *++e = MUL;  }//處理指標
      *++e = ADD;
    }
    else if (tk == Sub) {
      next(); *++e = PSH; expr(Mul);
      if (t > PTR && t == ty) { *++e = SUB; *++e = PSH; *++e = IMM; *++e = sizeof(int); *++e = DIV; ty = INT; }//指標相減
      else if ((ty = t) > PTR) { *++e = PSH; *++e = IMM; *++e = sizeof(int); *++e = MUL; *++e = SUB; }//指標減數值
      else *++e = SUB;
    }
    else if (tk == Mul) { next(); *++e = PSH; expr(Inc); *++e = MUL; ty = INT; }
    else if (tk == Div) { next(); *++e = PSH; expr(Inc); *++e = DIV; ty = INT; }
    else if (tk == Mod) { next(); *++e = PSH; expr(Inc); *++e = MOD; ty = INT; }
    else if (tk == Inc || tk == Dec) {//处理x++,x--
      if (*e == LC) { *e = PSH; *++e = LC; }
      else if (*e == LI) { *e = PSH; *++e = LI; }
      else { printf("%d: bad lvalue in post-increment\n", line); exit(-1); }
      *++e = PSH; *++e = IMM; *++e = (ty > PTR) ? sizeof(int) : sizeof(char);
      *++e = (tk == Inc) ? ADD : SUB;//先自增/自減
      *++e = (ty == CHAR) ? SC : SI;//存到内存里
      *++e = PSH; *++e = IMM; *++e = (ty > PTR) ? sizeof(int) : sizeof(char);
      *++e = (tk == Inc) ? SUB : ADD;//再相反操作,保證後自增/自減不影響這次表示式的求值
      next();
    }
    else if (tk == Brak) {//數組下標
      next(); *++e = PSH; expr(Assign);//保存數組指標, 計算下標
      if (tk == ']') next(); else { printf("%d: close bracket expected\n", line); exit(-1); }
      if (t > PTR) { *++e = PSH; *++e = IMM; *++e = sizeof(int); *++e = MUL;  }//t==PTR時是Char,Char = 0
      else if (t < PTR) { printf("%d: pointer type expected\n", line); exit(-1); }
      *++e = ADD;
      *++e = ((ty = t - PTR) == CHAR) ? LC : LI;
    }
    else { printf("%d: compiler error tk=%d\n", line, tk); exit(-1); }
  }
}

void stmt() // 陳述 statement
{
  int *a, *b;

  if (tk == If) { // if 語句
    next();
    if (tk == '(') next(); else { printf("%d: open paren expected\n", line); exit(-1); }
    expr(Assign);
    if (tk == ')') next(); else { printf("%d: close paren expected\n", line); exit(-1); }
    *++e = BZ; b = ++e;
    stmt();//繼續分析
    if (tk == Else) { // else 語句
      *b = (int)(e + 3);//e + 3 位置是else 起始位置
       *++e = JMP;// if 語句 else 之前插入 JMP 跳過Else 部分
        b = ++e;//JMP 的目標
      next();
      stmt();//分析else
    }
    *b = (int)(e + 1);//if 語句结束,無論是if BZ 轉跳目標還是 else 之前的JMP跳的目標
  }
  else if (tk == While) { // while 語句 (循環)
    next();
    a = e + 1;//循環開始的地址
    if (tk == '(') next(); else { printf("%d: open paren expected\n", line); exit(-1); }
    expr(Assign);
    if (tk == ')') next(); else { printf("%d: close paren expected\n", line); exit(-1); }
    *++e = BZ; b = ++e;// b = While 語句結束後的地址
    stmt();//處理While 語句
    *++e = JMP; *++e = (int)a;//無條件轉跳到While語句开始(包函循環條件的代碼),實現循環
    *b = (int)(e + 1);//BZ轉跳目標(循環结束)
  }
  else if (tk == Return) { // return 語句
    next();
    if (tk != ';') expr(Assign);//計算返回值
    *++e = LEV;
    if (tk == ';') next(); else { printf("%d: semicolon expected\n", line); exit(-1); }
  }
  else if (tk == '{') { // 區塊 {...} 複合語句
    next();
    while (tk != '}') stmt();
    next();
  }
  else if (tk == ';') { // ; 空陳述
    next();
  }
  else { // 指定 assign
    expr(Assign);//一般的語句視為賦值語句 或者 表示式
    if (tk == ';') next(); else { printf("%d: semicolon expected\n", line); exit(-1); }
  }
}

int prog() { // 編譯整個程式 Program
  int bt, i;
  line = 1;
  next();
  while (tk) {
    bt = INT; // basetype
    if (tk == Int) next();//已經有bt == INT
    else if (tk == Char) { next(); bt = CHAR; }//char 變量
    else if (tk == Enum) { // enum Id? {... 列舉
      next();
      if (tk != '{') next(); // 略過 Id
      if (tk == '{') {
        next();
        i = 0; // 紀錄 enum 的目前值 默認重0開始
        while (tk != '}') {
          if (tk != Id) { printf("%d: bad enum identifier %d\n", line, tk); return -1; }
          next();
          if (tk == Assign) { // 有 Id=Num 的情況 賦值語句
            next();
            if (tk != Num) { printf("%d: bad enum initializer\n", line); return -1; }
            i = ival;
            next();
          } //id 已經由 next 函數處理過
          id[Class] = Num; id[Type] = INT; id[Val] = i++;
          if (tk == ',') next();
        }
        next();
      }
    }//Enum 處理玩tk == ';', 略過下面
    while (tk != ';' && tk != '}') { // 掃描直到區塊結束
      ty = bt;
      while (tk == Mul) { next(); ty = ty + PTR; }// tk == Mul 表示已*開頭,為指標類型,類型加PTR表示何整類型的指標
      if (tk != Id) { printf("%d: bad global declaration\n", line); return -1; }
      if (id[Class]) { printf("%d: duplicate global definition\n", line); return -1; } // id.Class 已經存在，重複宣告了！
      next();
      id[Type] = ty;//賦值類型
      if (tk == '(') { // function 函數定義 ex: int f( ...
        id[Class] = Fun;
        id[Val] = (int)(e + 1);
        next(); i = 0;
        while (tk != ')') { // 掃描參數直到 ...)
          ty = INT;
          if (tk == Int) next();
          else if (tk == Char) { next(); ty = CHAR; }
          while (tk == Mul) { next(); ty = ty + PTR; }
          if (tk != Id) { printf("%d: bad parameter declaration\n", line); return -1; }
          if (id[Class] == Loc) { printf("%d: duplicate parameter definition\n", line); return -1; } // 這裡的 id 會指向 hash 搜尋過的 symTable 裏的那個 (在 next 裏處理的)，所以若是該 id 已經是 Local，那麼就重複了！
          // 把 id.Class, id.Type, id.Val 暫存到 id.HClass, id.HType, id.Hval ，因為 Local 優先於 Global
          id[HClass] = id[Class]; id[Class] = Loc;
          id[HType]  = id[Type];  id[Type] = ty;
          id[HVal]   = id[Val];   id[Val] = i++;//區域變量編號
          next();
          if (tk == ',') next();
        }
        next();
        if (tk != '{') { printf("%d: bad function definition\n", line); return -1; } // BODY 開始 {...
        loc = ++i;//區域變量偏移值
        next();
        while (tk == Int || tk == Char) { // 宣告 函數內變量
          bt = (tk == Int) ? INT : CHAR;
          next();
          while (tk != ';') {
            ty = bt;
            while (tk == Mul) { next(); ty = ty + PTR; }//處理指標
            if (tk != Id) { printf("%d: bad local declaration\n", line); return -1; }
            if (id[Class] == Loc) { printf("%d: duplicate local definition\n", line); return -1; }//備份符號訊息
            // 把 id.Class, id.Type, id.Val 暫存到 id.HClass, id.HType, id.Hval ，因為 Local 優先於 Global
            id[HClass] = id[Class]; id[Class] = Loc;
            id[HType]  = id[Type];  id[Type] = ty;
            id[HVal]   = id[Val];   id[Val] = ++i;//儲存變量偏移值
            next();
            if (tk == ',') next();
          }
          next();
        }
        *++e = ENT; *++e = i - loc;//函數區域變量數目
        while (tk != '}') stmt();//語法分析
        *++e = LEV;//函數返回
        id = sym; // unwind symbol table locals (把被區域變數隱藏掉的那些 Local id 還原，恢復全域變數的符號定義)
        while (id[Tk]) {//恢復符號訊息
          if (id[Class] == Loc) {
            id[Class] = id[HClass];
            id[Type] = id[HType];
            id[Val] = id[HVal];
          }
          id = id + Idsz;
        }
      }
      else {
        id[Class] = Glo;//全域變量
        id[Val] = (int)data;//全域變量在資料段分配內存
        data = data + sizeof(int);
      }
      if (tk == ',') next();
    }
    next();
  }
  return 0;
}

int run(int *pc, int *bp, int *sp) { // 虛擬機 => pc: 程式計數器, sp: 堆疊暫存器, bp: 框架暫存器
  int a, cycle; // a: 累積器, cycle: 執行指令數
  int i, *t;    // i: instruction, t:temps

  cycle = 0;
  while (1) {
    i = *pc++; ++cycle;
    if (debug) {
      printf("%d> %.4s", cycle,
        &"LEA ,IMM ,JMP ,JSR ,BZ  ,BNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PSH ,"
         "OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,"
         "OPEN,READ,CLOS,PRTF,MALC,FREE,MSET,MCMP,EXIT,"[i * 5]);
      if (i <= ADJ) printf(" %d\n", *pc); else printf("\n");
    }
    if      (i == LEA) a = (int)(bp + *pc++);                             // load local address 載入區域變數
    else if (i == IMM) a = *pc++;                                         // load global address or immediate 載入全域變數或立即值
    else if (i == JMP) pc = (int *)*pc;                                   // jump               躍躍指令
    else if (i == JSR) { *--sp = (int)(pc + 1); pc = (int *)*pc; }        // jump to subroutine 跳到副程式
    else if (i == BZ)  pc = a ? pc + 1 : (int *)*pc;                      // branch if zero     if (a==0) goto m[pc]
    else if (i == BNZ) pc = a ? (int *)*pc : pc + 1;                      // branch if not zero if (a!=0) goto m[pc]
    else if (i == ENT) { *--sp = (int)bp; bp = sp; sp = sp - *pc++; }     // enter subroutine   進入副程式
    else if (i == ADJ) sp = sp + *pc++;                                   // stack adjust       調整堆疊
    else if (i == LEV) { sp = bp; bp = (int *)*sp++; pc = (int *)*sp++; } // leave subroutine   離開副程式
    else if (i == LI)  a = *(int *)a;                                     // load int           載入整數
    else if (i == LC)  a = *(char *)a;                                    // load char          載入字元
    else if (i == SI)  *(int *)*sp++ = a;                                 // store int          儲存整數
    else if (i == SC)  a = *(char *)*sp++ = a;                            // store char         儲存字元
    else if (i == PSH) *--sp = a;                                         // push               推入堆疊

    else if (i == OR)  a = *sp++ |  a; // a = a OR *sp
    else if (i == XOR) a = *sp++ ^  a; // a = a XOR *sp
    else if (i == AND) a = *sp++ &  a; // ...
    else if (i == EQ)  a = *sp++ == a;
    else if (i == NE)  a = *sp++ != a;
    else if (i == LT)  a = *sp++ <  a;
    else if (i == GT)  a = *sp++ >  a;
    else if (i == LE)  a = *sp++ <= a;
    else if (i == GE)  a = *sp++ >= a;
    else if (i == SHL) a = *sp++ << a;
    else if (i == SHR) a = *sp++ >> a;
    else if (i == ADD) a = *sp++ +  a;
    else if (i == SUB) a = *sp++ -  a;
    else if (i == MUL) a = *sp++ *  a;
    else if (i == DIV) a = *sp++ /  a;
    else if (i == MOD) a = *sp++ %  a;

    else if (i == OPEN) a = open((char *)sp[1], *sp); // 開檔
    else if (i == READ) a = read(sp[2], (char *)sp[1], *sp); // 讀檔
    else if (i == CLOS) a = close(*sp); // 關檔
    else if (i == PRTF) { t = sp + pc[1]; a = printf((char *)t[-1], t[-2], t[-3], t[-4], t[-5], t[-6]); } // printf("....", a, b, c, d, e)
    else if (i == MALC) a = (int)malloc(*sp); // 分配記憶體
    else if (i == FREE) free((void *)*sp); // 釋放記憶體
    else if (i == MSET) a = (int)memset((char *)sp[2], sp[1], *sp); // 設定記憶體
    else if (i == MCMP) a = memcmp((char *)sp[2], (char *)sp[1], *sp); // 比較記憶體
    else if (i == EXIT) { printf("exit(%d) cycle = %d\n", *sp, cycle); return *sp; } // EXIT 離開
    else { printf("unknown instruction = %d! cycle = %d\n", i, cycle); return -1; } // 錯誤處理
  }
}

int main(int argc, char **argv) // 主程式
{
  int fd, ty, poolsz, *idmain;
  int *pc, *bp, *sp;
  int i, *t;

  --argc; ++argv;
  if (argc > 0 && **argv == '-' && (*argv)[1] == 's') { src = 1; --argc; ++argv; }
  if (argc > 0 && **argv == '-' && (*argv)[1] == 'd') { debug = 1; --argc; ++argv; }
  if (argc < 1) { printf("usage: c4 [-s] [-d] file ...\n"); return -1; }

  if ((fd = open(*argv, 0)) < 0) { printf("could not open(%s)\n", *argv); return -1; }

  poolsz = 256*1024; // arbitrary size
  if (!(sym = malloc(poolsz))) { printf("could not malloc(%d) symbol area\n", poolsz); return -1; } // 符號段
  if (!(le = e = malloc(poolsz))) { printf("could not malloc(%d) text area\n", poolsz); return -1; } // 程式段
  if (!(data = malloc(poolsz))) { printf("could not malloc(%d) data area\n", poolsz); return -1; } // 資料段
  if (!(sp = malloc(poolsz))) { printf("could not malloc(%d) stack area\n", poolsz); return -1; }  // 堆疊段

  memset(sym,  0, poolsz);
  memset(e,    0, poolsz);
  memset(data, 0, poolsz);

  p = "char else enum if int return sizeof while "
      "open read close printf malloc free memset memcmp exit void main";
  i = Char; while (i <= While) { next(); id[Tk] = i++; } // add keywords to symbol table
  i = OPEN; while (i <= EXIT) { next(); id[Class] = Sys; id[Type] = INT; id[Val] = i++; } // add library to symbol table
  next(); id[Tk] = Char; // handle void type
  next(); idmain = id; // keep track of main

  if (!(lp = p = malloc(poolsz))) { printf("could not malloc(%d) source area\n", poolsz); return -1; }
  if ((i = read(fd, p, poolsz-1)) <= 0) { printf("read() returned %d\n", i); return -1; }
  p[i] = 0; // 設定程式 p 字串結束符號 \0
  close(fd);

  if (prog() == -1) return -1;

  if (!(pc = (int *)idmain[Val])) { printf("main() not defined\n"); return -1; }
  if (src) return 0;

  // setup stack
  bp = sp = (int *)((int)sp + poolsz);
  *--sp = EXIT; // call exit if main returns
  *--sp = PSH; t = sp;
  *--sp = argc;
  *--sp = (int)argv;
  *--sp = (int)t;
  return run(pc, bp, sp);
}
