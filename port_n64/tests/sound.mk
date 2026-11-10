CXX := g++
GEN := work/ddsom/n64soundtest
BUILD := port_n64/build/sound-host
CPPFLAGS := -Iruntime -Isound -I$(GEN) -DDDSOM_SOUND_AOT_CHECK=1 -DDDSOM_SOUND_115=1
CXXFLAGS := -std=c++20 -O2 -g0 -ffunction-sections -fdata-sections -MMD -MP
SRCS := $(wildcard $(GEN)/z80_*.cpp) sound/z80.cpp runtime/recomp_z80.cpp port_n64/sound_ei.cpp port_n64/tests/sound_sequence.cpp
OBJS := $(patsubst %.cpp,$(BUILD)/%.o,$(SRCS))
all: $(BUILD)/sequence-test
$(BUILD)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
$(BUILD)/sequence-test: $(OBJS)
	$(CXX) -Wl,--gc-sections $(OBJS) -o $@
-include $(OBJS:.o=.d)

