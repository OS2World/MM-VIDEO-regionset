OBJS = regionset.obj

INCL = -I..
CFLAGS = /Gm+ /Mc -DOS2
OFLAGS = /O /qtune=pentium /qarch=pentium
#DFLAGS = /Ti

.c.obj:
	icc -c /Q -DOS2 $(INCL) $(CFLAGS) $(OFLAGS) $(DFLAGS) /Fo$* $<

.cpp.obj:
	icc -c /Q -DOS2 $(INCL) $(CFLAGS) $(OFLAGS) /Gx+ $(DFLAGS) /Fo$* $<

all: regionset.exe

wvision.exe: $(OBJS) $(@B).def
#   icc /Ti+ /Q /B"/nol /debug /map" -Fe $@ $(@B).def $(OBJS) $(LIBS)
   icc /Q /B"/nol /nodebug /ex:2" -Fe $@ $(@B).def $(OBJS) $(LIBS)

