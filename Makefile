TARGET=netsim_app
OBJECTS=main.o ZtpHost.o ZtpPacket.o app_config.o 
LIBRARY=../netsim/libnetsim.a
SOURCES=main.cpp ZtpHost.cpp ZtpPacket.cpp app_config.cpp
CCFLAGS=-g

all: netsim $(TARGET)

netsim:
	(cd ../netsim; make)

$(TARGET): $(OBJECTS) $(LIBRARY)
	g++ $(CCFLAGS) -o $(TARGET) $(OBJECTS) $(LIBRARY)

%.o: %.cpp
	g++ -c $(CCFLAGS) $*.cpp

%.o: %.c
	gcc -c $(CCFLAGS) $*.c

depend: $(SOURCES)
	(cd ../netsim; make depend)
	makedepend $(SOURCES)

clean:
	(cd ../netsim; make clean)
	rm *.o

# DO NOT DELETE

main.o: ../netsim/common.h /usr/include/sys/types.h /usr/include/_ansi.h
main.o: /usr/include/newlib.h /usr/include/sys/config.h
main.o: /usr/include/machine/ieeefp.h /usr/include/sys/features.h
main.o: /usr/include/machine/_types.h /usr/include/machine/_default_types.h
main.o: /usr/include/sys/_types.h /usr/include/sys/lock.h
main.o: /usr/include/machine/types.h /usr/include/stdlib.h
main.o: /usr/include/sys/reent.h /usr/include/machine/stdlib.h
main.o: /usr/include/alloca.h ../netsim/Config.h ../netsim/Packet.h
main.o: ../netsim/Timer.h ../netsim/PacketScheduler.h ../netsim/Scheduler.h
main.o: ../netsim/Topology.h /usr/include/unistd.h /usr/include/sys/unistd.h
ZtpHost.o: ../netsim/common.h /usr/include/sys/types.h /usr/include/_ansi.h
ZtpHost.o: /usr/include/newlib.h /usr/include/sys/config.h
ZtpHost.o: /usr/include/machine/ieeefp.h /usr/include/sys/features.h
ZtpHost.o: /usr/include/machine/_types.h
ZtpHost.o: /usr/include/machine/_default_types.h /usr/include/sys/_types.h
ZtpHost.o: /usr/include/sys/lock.h /usr/include/machine/types.h
ZtpHost.o: /usr/include/stdlib.h /usr/include/sys/reent.h
ZtpHost.o: /usr/include/machine/stdlib.h /usr/include/alloca.h
ZtpHost.o: ../netsim/Node.h ../netsim/FIFONode.h ../netsim/Packet.h
ZtpHost.o: ZtpPacket.h ../netsim/Timer.h ../netsim/PacketScheduler.h
ZtpHost.o: ../netsim/Scheduler.h ZtpHost.h
ZtpPacket.o: ../netsim/common.h /usr/include/sys/types.h /usr/include/_ansi.h
ZtpPacket.o: /usr/include/newlib.h /usr/include/sys/config.h
ZtpPacket.o: /usr/include/machine/ieeefp.h /usr/include/sys/features.h
ZtpPacket.o: /usr/include/machine/_types.h
ZtpPacket.o: /usr/include/machine/_default_types.h /usr/include/sys/_types.h
ZtpPacket.o: /usr/include/sys/lock.h /usr/include/machine/types.h
ZtpPacket.o: /usr/include/stdlib.h /usr/include/sys/reent.h
ZtpPacket.o: /usr/include/machine/stdlib.h /usr/include/alloca.h
ZtpPacket.o: ../netsim/Node.h ../netsim/Packet.h ../netsim/Timer.h
ZtpPacket.o: ../netsim/PacketScheduler.h ../netsim/FIFONode.h
ZtpPacket.o: ../netsim/Scheduler.h ZtpPacket.h
app_config.o: /usr/include/string.h /usr/include/_ansi.h
app_config.o: /usr/include/newlib.h /usr/include/sys/config.h
app_config.o: /usr/include/machine/ieeefp.h /usr/include/sys/features.h
app_config.o: /usr/include/sys/reent.h /usr/include/sys/_types.h
app_config.o: /usr/include/machine/_types.h
app_config.o: /usr/include/machine/_default_types.h /usr/include/sys/lock.h
app_config.o: /usr/include/sys/string.h ../netsim/common.h
app_config.o: /usr/include/sys/types.h /usr/include/machine/types.h
app_config.o: /usr/include/stdlib.h /usr/include/machine/stdlib.h
app_config.o: /usr/include/alloca.h ../netsim/Node.h ../netsim/Packet.h
app_config.o: ../netsim/Timer.h ../netsim/PacketScheduler.h
app_config.o: ../netsim/FIFONode.h ../netsim/Scheduler.h ../netsim/Config.h
app_config.o: ZtpHost.h
