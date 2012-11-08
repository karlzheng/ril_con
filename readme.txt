for ARM:
set CROSS_COMPILE and then "make" to gen bin and run the ril_con bin file.

test in PC:
first: sudo apt-get install libreadline6 libreadline-dev
and then: "make ril_con_pc" and run: ./ril_con_pc

type "quit" or "exit" to exit this program.


wget "ftp://ftp.gnu.org/gnu/readline/readline-5.2.tar.gz"

cross_compile:readline & ncurses-5.5:
./configure --disable-shared --host=arm-linux CC=arm-none-linux-gnueabi-gcc \
--prefix=$(pwd)/_install
