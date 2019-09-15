
CPPFLAGS += -I /opt/vc/include

CFLAGS = -Wall -g -O3

LDLIBS += -L/opt/vc/lib -lbcm_host

SRCS = $(shell ls *.c)

OBJS = $(subst .c,.o, $(SRCS))

DEPS = $(subst .c,.d,$(SRCS))

all:	pi2ws2812pwm

pi2ws2812pwm:	$(OBJS)
	$(LINK.o) $^ $(LDLIBS) -o $@

clean:
	$(RM) *.d *.o pi2ws2812pwm

.PHONY:	all clean

ifneq "$(MAKECMDGOALS)" "clean"
-include $(DEPS)
endif

define make-depend
@gcc -MM -MF $3 -MP -MT $2 $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) $1
endef

%.o:	%.c
	$(call make-depend,$<,$@,$(subst .o,.d,$@))
	$(COMPILE.c) $(OUTPUT_OPTION) $<

