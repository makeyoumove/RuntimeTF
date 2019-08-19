CC=g++
TARGET=KernelSim

all : sim

sim :
	if [ -e build ] ; then echo "build dir exist" ; else mkdir build ; fi
	$(CC) -o build/$(TARGET) src/$(TARGET).cpp #-fpermissive
	cp data/* build/

clean : 
	rm -rf build
