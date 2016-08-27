# CHIP-8
A [CHIP-8](https://en.wikipedia.org/wiki/CHIP-8) interpreter written in C++.
![A sampling of games.](/demo.png)
##Features
* Feature rich CLI to customize how apps run.
* Primitive debugging mode.
* Cross platform support.

For dusty CHIP-8 apps, check out the **ROM files** bundled with [Emma 02](http://www.emma02.hobby-site.com/download.html).

## Install
1. Requires [TCLAP](http://tclap.sourceforge.net/) and [SDL2](https://www.libsdl.org/download-2.0.php) includes to compile. Set the path to these in the Makefile.
2. Requires the [SDL2](https://www.libsdl.org/download-2.0.php) libraries. The easiest way is to install a package such as `libsdl2-dev` (Ubuntu) for your distribution.
3. Compile with gcc:
```sh
$ make
```

## Keyboard mapping
Imagine this hexadecimal keypad is laid on top of the 1 key. Thus, on the English keyboard, 1 maps to 1, Q maps 4, and so on. Your specific keyboard layout shouldn't change where these keys are mapped.

  |   |   | | 
------------ | ------------ | ------------ | ------------ 
1 | 2 | 3 | C
4 | 5 | 6 | D
7 | 8 | 9 | E
A | 0 | B | F
