# Test 1:
define i32 @main() {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  ret i32 3
}

# Test 2:
@a = global i32 3, align 4
@b = global i32 5, align 4
define i32 @main() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 5, i32* %2, align 4
  %3 = load i32, i32* %2, align 4
  %4 = load i32, i32* @b, align 4
  %5 = add nsw i32 %3, %4
  ret i32 %5
}

# Test 3:
define i32 @main() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 1, i32* %2, align 4
  store i32 2, i32* %3, align 4
  store i32 3, i32* %4, align 4
  %5 = load i32, i32* %3, align 4
  %6 = load i32, i32* %4, align 4
  %7 = add nsw i32 %5, %6
  ret i32 %7
}

# Test 4:
@a = constant i32 10, align 4
@b = constant i32 5, align 4
define i32 @main() {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  ret i32 5
}

# Test 5:
define i32 @main() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 10, i32* %2, align 4
  store i32 5, i32* %3, align 4
  ret i32 5
}

# Test 6:
@a = global i32 0, align 4
define i32 @func(i32 %0) {
  %2 = alloca i32, align 4
  store i32 %0, i32* %2, align 4
  %3 = load i32, i32* %2, align 4
  %4 = sub nsw i32 %3, 1
  store i32 %4, i32* %2, align 4
  %5 = load i32, i32* %2, align 4
  ret i32 %5
}
define i32 @main() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 10, i32* @a, align 4
  %3 = load i32, i32* @a, align 4
  %4 = call i32 @func(i32 %3)
  store i32 %4, i32* %2, align 4
  %5 = load i32, i32* %2, align 4
  ret i32 %5
}

# Test 7:
define i32 @defn() {
  ret i32 4
}
define i32 @main()  {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %3 = call i32 @defn()
  store i32 %3, i32* %2, align 4
  %4 = load i32, i32* %2, align 4
  ret i32 %4
}

# Test 8:
define i32 @main() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 10, i32* %2, align 4
  store i32 -1, i32* %3, align 4
  %4 = load i32, i32* %2, align 4
  %5 = load i32, i32* %3, align 4
  %6 = add nsw i32 %4, %5
  ret i32 %6
}

# Test 9:
@a = constant i32 10, align 4
define i32 @main() {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  ret i32 15
}

# Test 10:
@a = constant i32 10, align 4
define i32 @main() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 2, i32* %2, align 4
  %3 = load i32, i32* %2, align 4
  %4 = sub nsw i32 %3, 10
  ret i32 %4
}

# Test 11:
define i32 @main() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 10, i32* %2, align 4
  %3 = load i32, i32* %2, align 4
  %4 = sub nsw i32 %3, 2
  ret i32 %4
}

# Test 12:
define i32 @main() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 10, i32* %2, align 4
  store i32 5, i32* %3, align 4
  %4 = load i32, i32* %2, align 4
  %5 = load i32, i32* %3, align 4
  %6 = mul nsw i32 %4, %5
  ret i32 %6
}

# Test 13:
@a = constant i32 5, align 4
define i32 @main() {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  ret i32 25
}

# Test 14:
define i32 @main() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 10, i32* %2, align 4
  store i32 5, i32* %3, align 4
  %4 = load i32, i32* %2, align 4
  %5 = load i32, i32* %3, align 4
  %6 = sdiv i32 %4, %5
  ret i32 %6
}

# Test 15:
@a = constant i32 10, align 4
define i32 @main() {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  ret i32 2
}

# Test 16:
define i32 @main() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 10, i32* %2, align 4
  %3 = load i32, i32* %2, align 4
  %4 = sdiv i32 %3, 3
  ret i32 %4
}

# Test 17:
define i32 @main() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 10, i32* %2, align 4
  %3 = load i32, i32* %2, align 4
  %4 = srem i32 %3, 3
  ret i32 %4
}

# Test 18:
define i32 @ifElseIf() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  store i32 5, i32* %2, align 4
  store i32 10, i32* %3, align 4
  %4 = load i32, i32* %2, align 4
  %5 = icmp eq i32 %4, 6
  br i1 %5, label %9, label %6

6:
  %7 = load i32, i32* %3, align 4
  %8 = icmp eq i32 %7, 11
  br i1 %8, label %9, label %11

9:
  %10 = load i32, i32* %2, align 4
  store i32 %10, i32* %1, align 4
  br label %34

11:
  %12 = load i32, i32* %3, align 4
  %13 = icmp eq i32 %12, 10
  br i1 %13, label %14, label %18

14:
  %15 = load i32, i32* %2, align 4
  %16 = icmp eq i32 %15, 1
  br i1 %16, label %17, label %18

17:
  store i32 25, i32* %2, align 4
  br label %31

18:
  %19 = load i32, i32* %3, align 4
  %20 = icmp eq i32 %19, 10
  br i1 %20, label %21, label %27

21:
  %22 = load i32, i32* %2, align 4
  %23 = icmp eq i32 %22, -5
  br i1 %23, label %24, label %27

24:
  %25 = load i32, i32* %2, align 4
  %26 = add nsw i32 %25, 15
  store i32 %26, i32* %2, align 4
  br label %30

27:
  %28 = load i32, i32* %2, align 4
  %29 = sub nsw i32 0, %28
  store i32 %29, i32* %2, align 4
  br label %30

30:
  br label %31

31:
  br label %32

32:
  %33 = load i32, i32* %2, align 4
  store i32 %33, i32* %1, align 4
  br label %34

34:
  %35 = load i32, i32* %1, align 4
  ret i32 %35
}

define i32 @main() {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %2 = call i32 @ifElseIf()
  %3 = call i32 @putint(i32 %2)
  ret i32 0
}

# Test 19:
define i32 @ififElse() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 5, i32* %1, align 4
  store i32 10, i32* %2, align 4
  %3 = load i32, i32* %1, align 4
  %4 = icmp eq i32 %3, 5
  br i1 %4, label %5, label %13

5:
  %6 = load i32, i32* %2, align 4
  %7 = icmp eq i32 %6, 10
  br i1 %7, label %8, label %9

8:
  store i32 25, i32* %1, align 4
  br label %12

9:
  %10 = load i32, i32* %1, align 4
  %11 = add nsw i32 %10, 15
  store i32 %11, i32* %1, align 4
  br label %12

12:
  br label %13

13:
  %14 = load i32, i32* %1, align 4
  ret i32 %14
}
define i32 @main() {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %2 = call i32 @ififElse()
  ret i32 %2
}

# Test 20:
define i32 @if_ifElse_() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 5, i32* %1, align 4
  store i32 10, i32* %2, align 4
  %3 = load i32, i32* %1, align 4
  %4 = icmp eq i32 %3, 5
  br i1 %4, label %5, label %13

5:
  %6 = load i32, i32* %2, align 4
  %7 = icmp eq i32 %6, 10
  br i1 %7, label %8, label %9

8:
  store i32 25, i32* %1, align 4
  br label %12

9: 
  %10 = load i32, i32* %1, align 4
  %11 = add nsw i32 %10, 15
  store i32 %11, i32* %1, align 4
  br label %12

12:
  br label %13

13:
  %14 = load i32, i32* %1, align 4
  ret i32 %14
}

define i32 @main() {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %2 = call i32 @if_ifElse_()
  ret i32 %2
}

# Test 21:
define i32 @if_if_Else() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 5, i32* %1, align 4
  store i32 10, i32* %2, align 4
  %3 = load i32, i32* %1, align 4
  %4 = icmp eq i32 %3, 5
  br i1 %4, label %5, label %10

5:
  %6 = load i32, i32* %2, align 4
  %7 = icmp eq i32 %6, 10
  br i1 %7, label %8, label %9

8:
  store i32 25, i32* %1, align 4
  br label %9

9: 
  br label %13

10: 
  %11 = load i32, i32* %1, align 4
  %12 = add nsw i32 %11, 15
  store i32 %12, i32* %1, align 4
  br label %13

13: 
  %14 = load i32, i32* %1, align 4
  ret i32 %14
}
define i32 @main() {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %2 = call i32 @if_if_Else()
  ret i32 %2
}

# Test 22:
define i32 @get_one(i32 %0) {
  %2 = alloca i32, align 4
  store i32 %0, i32* %2, align 4
  ret i32 1
}
define i32 @deepWhileBr(i32 %0, i32 %1) {
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  %5 = alloca i32, align 4
  %6 = alloca i32, align 4
  %7 = alloca i32, align 4
  store i32 %0, i32* %3, align 4
  store i32 %1, i32* %4, align 4
  %8 = load i32, i32* %3, align 4
  %9 = load i32, i32* %4, align 4
  %10 = add nsw i32 %8, %9
  store i32 %10, i32* %5, align 4
  br label %11

11:
  %12 = load i32, i32* %5, align 4
  %13 = icmp slt i32 %12, 75
  br i1 %13, label %14, label %34

14:
  store i32 42, i32* %6, align 4
  %15 = load i32, i32* %5, align 4
  %16 = icmp slt i32 %15, 100
  br i1 %16, label %17, label %33

17:
  %18 = load i32, i32* %5, align 4
  %19 = load i32, i32* %6, align 4
  %20 = add nsw i32 %18, %19
  store i32 %20, i32* %5, align 4
  %21 = load i32, i32* %5, align 4
  %22 = icmp sgt i32 %21, 99
  br i1 %22, label %23, label %32

23:
  %24 = load i32, i32* %6, align 4
  %25 = mul nsw i32 %24, 2
  store i32 %25, i32* %7, align 4
  %26 = call i32 @get_one(i32 0)
  %27 = icmp eq i32 %26, 1
  br i1 %27, label %28, label %31

28:
  %29 = load i32, i32* %7, align 4
  %30 = mul nsw i32 %29, 2
  store i32 %30, i32* %5, align 4
  br label %31

31:
  br label %32

32:
  br label %33

33:
  br label %11, !llvm.loop !6

34:
  %35 = load i32, i32* %5, align 4
  ret i32 %35
}
define i32 @main() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 2, i32* %2, align 4
  %3 = load i32, i32* %2, align 4
  %4 = load i32, i32* %2, align 4
  %5 = call i32 @deepWhileBr(i32 %3, i32 %4)
  store i32 %5, i32* %2, align 4
  %6 = load i32, i32* %2, align 4
  %7 = call @putint(i32 %6)
  ret i32 0
}

# Test 23:
define i32 @doubleWhile() {
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 5, i32* %1, align 4
  store i32 7, i32* %2, align 4
  br label %3

3:
  %4 = load i32, i32* %1, align 4
  %5 = icmp slt i32 %4, 100
  br i1 %5, label %6, label %18

6:
  %7 = load i32, i32* %1, align 4
  %8 = add nsw i32 %7, 30
  store i32 %8, i32* %1, align 4
  br label %9

9:
  %10 = load i32, i32* %2, align 4
  %11 = icmp slt i32 %10, 100
  br i1 %11, label %12, label %15

12:
  %13 = load i32, i32* %2, align 4
  %14 = add nsw i32 %13, 6
  store i32 %14, i32* %2, align 4
  br label %9, !llvm.loop !6

15:
  %16 = load i32, i32* %2, align 4
  %17 = sub nsw i32 %16, 100
  store i32 %17, i32* %2, align 4
  br label %3, !llvm.loop !8

18:
  %19 = load i32, i32* %2, align 4
  ret i32 %19
}
define i32 @main() {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %2 = call i32 @doubleWhile()
  ret i32 %2
}

# Test 24:
@a = global [10 x [10 x i32]] zeroinitializer, align 16
define i32 @main() {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  ret i32 0
}

# Test 25:
@a = constant [5 x i32] [i32 0, i32 1, i32 2, i32 3, i32 4], align 16
define i32 @main() {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  %2 = load i32, i32* getelementptr inbounds ([5 x i32], [5 x i32]* @a, i64 0, i64 4), align 16
  ret i32 %2
}
