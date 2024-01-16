# Neko CC: A top-down C Compiler and a LR(1) compiler

---

## Introduction

Neko CC has two parts:
### A top-down C(subset) Compiler

It's basically follow the C99 standard, with some changes:
- Remove K&R gramma
(Format like `int f(a, b) int, int` or default return int function, is there anyone using it?)
- Remove switch statement support (techeniclly can implement it, though.)
- va_list
- initlizer list

### An LR(1) compiler-compiler

An LR(1) compiler like with native CPP. Though now it's not as powerful as YACC, like it doesn't have error restoration. But I believe it can give you a basic structure of a LR parser.

### An LL(1) compiler (not very functional)

An LL(1) parser is also included, and it can run(in some way), though it's not a finisd functional compiler, as... Well, why not use the LR instead?

You can learn the structure and idea of it.

## Why?

Indeed... Most people don't care -- or don't do related jobs -- about compiler.

Even those who do, they mostly use LEX/YACC or tools like that.

But... Almost all software are based on compilers, knowing them certainly helps a lot!

Besides... It's fun, right?

## I heard they are complicated and hard!

Well... That's simply not the truth, at least nowadays, it's not hard to write a complete compiler(even for a complicate language like C!). Sometimes they look complicate, as they may neet to meet some special requirement, like size limit, historical support, efficienty and so on... But without these limitation, it's quiet straight foward to write a compiler, especially for some simple language.

## Where should I start?

If you are new to this... I strongly suggested you reading the series "Let's build a compler". Though this series is based on a fairly old platform and Pascal, it do helps me a lot, and let me understand the compiler could be easy and fun!

Then if you want to know some theory or techinique under it, then find a book about compiler theory.

## Behind the neko cc

So it's initially my compiler theory homework. But why stop at a LL or LR(0) parser which can only do 
```
F = F *\/ T
T = T +\- I
I = (F) / i
```

if it's not difficult?

So if you want to look at HOW does a top-down, LL(1) or LR(1) parser looks like, I believe it can at least give you some inspiration (though you may need a book with algorithm in it).

## What can I learn from it?

How to deal with types and function call. (Lot's of schools, like mine, don't mind var types in teaching. But they can be faily complicate and mind blow if you want to implement something other than int.)

How to write a top-down compiler for a more complicite language.

And... **How to finish the school homework!**

## How to use it

Run `make menuconfig` to comfig which part of the project you want to use (the top-down parser? LL? LR?)

The structure is basiclly the same. 
- Create the lex (if you're using the LL/LR)
- Read the file you want to parse and open the output file.
- Init the translater env.
- Run translation_unit.

If you want to write your own rule for the LR parser, take a look at the doc/lex.yml to get the structure.

Use `reg_sym` to register your own reduce function.

The tokens are in inc/tok.hh, change them if you want.

## This project is bad!

Well... I'm really sorry. After all, it's my first try. Let me know how it can be improved.