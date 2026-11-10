CXX := g++
GEN := work/ddsom/n64test
BUILD := port_n64/build/host
CPPFLAGS := -DDDSOM_NATIVE_BOARD=1 -Iruntime -Imachine -I$(GEN)
CXXFLAGS := -std=gnu++20 -O1 -g0 -fno-strict-aliasing -ffunction-sections -fdata-sections -MMD -MP
SRCS := $(wildcard $(GEN)/native_*.cpp) machine/m68k.cpp machine/eeprom.cpp runtime/recomp.cpp runtime/game.cpp port_n64/board.cpp port_n64/tests/board_differential.cpp
OBJS := $(patsubst %.cpp,$(BUILD)/%.o,$(SRCS))
all: $(BUILD)/board-test
$(BUILD)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
$(BUILD)/board-test: $(OBJS)
	$(CXX) -Wl,--gc-sections $(OBJS) -o $@
-include $(OBJS:.o=.d)

$(BUILD)/rdp_video.o: port_n64/rdp_video.cpp port_n64/tests/fake/libdragon.h
	$(CXX) -Iport_n64/tests/fake $(CPPFLAGS) $(CXXFLAGS) -O2 -c $< -o $@
$(BUILD)/video_reference.o: port_n64/tests/video_reference.cpp port_n64/tests/fake/libdragon.h
	$(CXX) -Iport_n64/tests/fake $(CPPFLAGS) $(CXXFLAGS) -O2 -c $< -o $@
$(BUILD)/video.o: video/video.cpp video/video.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -O2 -c $< -o $@
$(BUILD)/video-test: $(filter-out $(BUILD)/port_n64/tests/board_differential.o,$(OBJS)) $(BUILD)/rdp_video.o $(BUILD)/video.o $(BUILD)/video_reference.o
	$(CXX) -Wl,--gc-sections $^ -o $@

