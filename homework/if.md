# if.c
* code
    * [if.c]()
```
void IF(){
  int ifBegin = nextLabel();
  int ifMid = nextLabel();
  int ifEnd = nextLabel();
  emit("(L%d)\n", ifBegin);
  skip("if");
  skip("(");
  int e = E();
  emit("if not T%d goto L%d\n", e, ifMid); //若判斷式不成立則進入下一個條件式
  skip(")");
  STMT();
  emit("goto L%d\n", ifEnd); //表示完成條件式中的code，並結束整個條件式
  emit("(L%d)\n", ifMid);
  if(isNext("else")){ //else if和else的條件式
    skip("else");
    STMT();
    emit("(L%d)\n",ifEnd); //表示完成條件式中的code，並結束整個條件式
  }
}
```
* 執行結果
```
PS C:\Users\柯泓吉\Desktop\課程\系統程式\sp109b\homework> ./compiler test/if.c
if (a>3) {
  t=1;
} else {
  t=2;
}

========== lex ==============
token=if
token=(
token=a
token=>
token=3
token=)
token={
token=t
token==
token=1
token=;
token=}
token=else
token={
token=t
token==
token=2
token=;
token=}
========== dump ==============
0:if
1:(
2:a
3:>
4:3
5:)
6:{
7:t
8:=
9:1
10:;
11:}
12:else
13:{
14:t
15:=
16:2
17:;
18:}
============ parse =============
(L0)
t0 = a
t1 = 3
t2 = t0 > t1
if not T2 goto L1
t3 = 1
t = t3
goto L2
(L1)
t4 = 2
t = t4
(L2)
```