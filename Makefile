SDCC=sdcc
SDLD=sdld
OBJECTS=stm8rf.ihx

.PHONY: all clean flash

all: $(OBJECTS)

clean:
	rm -f $(OBJECTS)

flash: $(OBJECT).ihx
	stm8flash -cstlinkv2 -pstm8s003?3 -w $(OBJECT).ihx

%.ihx: %.c
	$(SDCC) -mstm8 --out-fmt-ihx -lstm8 --std-sdcc99 --opt-code-size $(CFLAGS) $(LDFLAGS) $<
