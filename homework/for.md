# for.c
* code
    * [for.c]()
```
void FOR(){
  int forBegin = nextLabel();
  int forEnd = nextLabel();
  emit("(L%d)\n", forBegin);
  skip("for");
  skip("(");
  ASSIGN(); //for裡面判斷式用的變數定義
  int e = E(); //判斷式
  emit("if not T%d goto L%d\n", e, forEnd); //當判斷式不成立，跳出for迴圈
  skip(";");
  int e1 = E(); //變數的累加
  skip(")");
  STMT();
  emit("goto L%d\n", forBegin);
  emit("L%d", forEnd);
}
```
* 執行結果
```
PS C:\Users\柯泓吉\Desktop\課程\系統程式\sp109b\homework> ./compiler test/for.c
for(i=0; i<10; i++){
    a = a + 1;
}
========== lex ==============
token=for
token=(
token=i
token==
token=0
token=;
token=i
token=<
token=10
token=;
token=i
token=++
token=)
token={
token=a
token==
token=a
token=+
token=1
token=;
token=}
========== dump ==============
0:for
1:(
2:i
3:=
4:0
5:;
6:i
7:<
8:10
9:;
10:i
11:++
12:)
13:{
14:a
15:=
16:a
17:+
18:1
19:;
20:}
============ parse =============
(L0)
t0 = 0
i = t0
t1 = i
t2 = 10
t3 = t1 < t2
if not T3 goto L1
t5 = a
t6 = 1
t7 = t5 + t6
a = t7
i = i + 1
t4 = i

goto L0
(L1)
```