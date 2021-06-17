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