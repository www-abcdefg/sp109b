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