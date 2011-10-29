
CC            = gcc

C_FLAGS       = -std=c99 -pedantic -Wall -Wextra -fstrict-aliasing -Wshadow \
                -Wwrite-strings -Wpointer-arith -Wcast-align -Wnested-externs \
                -Wmissing-prototypes -Wstrict-prototypes  -Winline -Wcast-qual \
                -Wmissing-declarations -Wredundant-decls

DEBUG_FLAGS   = -O0 -g

RELEASE_FLAGS = -O3

PREPROCESSOR  =

SOURCES       = xml.c test/test.c

TARGET        = run

INCLUDE_DIRS  = .


###################
###################


OBJS    = $(SOURCES:.c=.o)
INCLUDE = $(addprefix -I ,$(INCLUDE_DIRS))#
PREP    = $(addprefix -D ,$(PREPROCESSOR))#


##################
##################


debug: override C_FLAGS += $(DEBUG_FLAGS)
debug: compile

release: override C_FLAGS += $(RELEASE_FLAGS)
release: compile

compile: preclean $(OBJS) target postclean

$(OBJS):
	$(CC) $(C_FLAGS) $(PREP) $(INCLUDE) -c $(@:.o=.c)

target:
	$(CC) $(C_FLAGS) $(PREP) $(INCLUDE) *.o -o $(TARGET)

preclean:
	$(shell rm -f *.o)

postclean:
	$(shell rm -f *.o)

clean:
	rm -f *.o $(TARGET)
