// Test 1:
int main() {
    return 3;
}

// Test 2:
int a = 3;
int b = 5;
int main() {
    int a = 5;
    return a + b;
}

// Test 3:
int main() {
    int a, b0, _c;
    a = 1;
    b0 = 2;
    _c = 3;
    return b0 + _c;
}

// Test 4:
const int a = 10;
const int b = 5;
int main() {
    return b;
}

// Test 5:
int main() {
    const int a = 10;
    const int b = 5;
    return b;
}

// Test 6:
int a;
int func(int p){
	p = p - 1;
	return p;
}
int main(){
	int b;
	a = 10;
	b = func(a);
	return b;
}

// Test 7:
int defn() {
    return 4;
}
int main() {
    int a = defn();
    return a;
}

// Test 8:
int main(){
    int a, b;
    a = 10;
    b = -1;
    return a + b;
}

// Test 9:
const int a = 10;
int main() {
    return a + 5;
}

// Test 10:
const int a = 10;
int main() {
    int b = 2;
    return b - a;
}

// Test 11:
int main(){
    int a;
    a = 10;
    return a - 2;
}

// Test 12:
int main(){
    int a, b;
    a = 10;
    b = 5;
    return a * b;
}

// Test 13:
const int a = 5;
int main() {
    return a * 5;
}

// Test 14:
int main(){
    int a, b;
    a = 10;
    b = 5;
    return a / b;
}

// Test 15:
const int a = 10;
int main() {
    return a / 5;
}

// Test 16:
int main(){
    int a;
    a = 10;
    return a / 3;
}

// Test 17:
int main(){
    int a;
    a = 10;
    return a % 3;
}

// Test 18:
int ifElseIf() {
    int a;
    a = 5;
    int b;
    b = 10;
    if(a == 6 || b == 0xb) {
      return a;
    }
    else {
      if (b == 10 && a == 1)
        a = 25;
      else if (b == 10 && a == -5)
        a = a + 15;
      else
        a = -+a;
    }
  
    return a;
}
  
int main(){
    putint(ifElseIf());
    return 0;
}

// Test 19:
int ififElse() {
    int a;
    a = 5;
    int b;
    b = 10;
    if(a == 5)
      if (b == 10) 
        a = 25;
      else 
        a = a + 15;
      
    return (a);
}
  
int main(){
    return (ififElse());
}

// Test 20:
int if_ifElse_() {
    int a;
    a = 5;
    int b;
    b = 10;
    if(a == 5){
      if (b == 10) 
        a = 25;
      else 
        a = a + 15;
    }
    return (a);
}
  
int main(){
    return (if_ifElse_());
}

// Test 21:
int if_if_Else() {
    int a;
    a = 5;
    int b;
    b = 10;
    if(a == 5){
      if (b == 10) 
        a = 25;
    }
      else 
        a = a + 15;
    return (a);
}
  
int main(){
    return (if_if_Else());
}  

// Test 22:
int get_one(int a) {
    return 1;
}
  
int deepWhileBr(int a, int b) {
    int c;
    c = a + b;
    while (c < 75) {
      int d;
      d = 42;
      if (c < 100) {
        c = c + d;
        if (c > 99) {
          int e;
          e = d * 2;
          if (get_one(0) == 1) {
            c = e * 2;
          }
        }
      }
    }
    return (c);
}
  
int main() {
    int p;
    p = 2;
    p = deepWhileBr(p, p);
    putint(p);
    return 0;
}

// Test 23:
int doubleWhile() {
    int i;
    i = 5;
    int j;
    j = 7;
    while (i < 100) {
      i = i + 30;
      while(j < 100){
        j = j + 6;
      }
      j = j - 100;
    }
    return (j);
}
  
int main() {
    return doubleWhile();
}
  

// Test 24:
int a[10][10];
int main() {
    return 0;
}

// Test 25:
const int a[5] = {0, 1, 2, 3, 4};
int main() {
    return a[4];
}